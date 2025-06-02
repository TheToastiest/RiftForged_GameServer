// File: GameplayEngine/GameplayEngine.cpp
//  RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "GameplayEngine.h"
// ActivePlayer.h, PlayerManager.h, RiftStepLogic.h, CombatLogic.h,
// PhysicsEngine.h, MathUtil.h, Logger.h, and FlatBuffers C2S/S2C/Common
// are included via GameplayEngine.h

#include <cmath>
#include <algorithm>
#include <random> // For stubbed damage roll

// Make sure Ability IDs from ActivePlayer.h are accessible
// const uint32_t RiftForged::GameLogic::RIFTSTEP_ABILITY_ID; // Defined in ActivePlayer.h
// const uint32_t RiftForged::GameLogic::BASIC_ATTACK_ABILITY_ID; // Defined in ActivePlayer.h


namespace RiftForged {
    namespace Gameplay {

        // --- Temporary Stub for Weapon Properties ---
        struct TempWeaponProperties {
            bool isMelee;
            float range;
            float attackCooldownSec;
            RiftForged::Networking::Shared::DamageInstance baseDamageInstance;
            float projectileSpeed;
            std::string projectileVfxTag;
        };

        RiftForged::GameLogic::PlayerManager& GameplayEngine::GetPlayerManager() {
            return m_playerManager; // m_playerManager is a member of GameplayEngine
        }

        static TempWeaponProperties GetStubbedWeaponProperties(GameLogic::ActivePlayer* attacker) {
            using namespace RiftForged::Networking::Shared;
            using RiftForged::GameLogic::EquippedWeaponCategory;

            float base_player_attack_cooldown = attacker ? attacker->base_basic_attack_cooldown_sec : 1.0f;
            EquippedWeaponCategory category = attacker ? attacker->current_weapon_category : EquippedWeaponCategory::Unarmed;

            auto create_dmg_inst = [&](int min_dmg, int max_dmg, DamageType type) {
                int amount = min_dmg + (max_dmg > min_dmg ? (rand() % (max_dmg - min_dmg + 1)) : 0);
                // Basic attacks from stub don't crit on roll, crit is determined by hit evaluation logic later
                return DamageInstance(amount, type, false);
                };

            switch (category) {
            case EquippedWeaponCategory::Generic_Melee_Sword:
            case EquippedWeaponCategory::Generic_Melee_Axe:
                return { true, 2.5f, base_player_attack_cooldown, create_dmg_inst(10, 15, DamageType::DamageType_Physical), 0.f, "" };
            case EquippedWeaponCategory::Generic_Melee_Maul:
                return { true, 3.0f, base_player_attack_cooldown * 1.2f, create_dmg_inst(15, 25, DamageType::DamageType_Physical), 0.f, "" };
            case EquippedWeaponCategory::Generic_Ranged_Bow:
                return { false, 30.0f, base_player_attack_cooldown, create_dmg_inst(12, 18, DamageType::DamageType_Physical), 40.f, "VFX_Projectile_Arrow" };
            case EquippedWeaponCategory::Generic_Ranged_Gun:
                return { false, 25.0f, base_player_attack_cooldown * 0.8f, create_dmg_inst(8, 12, DamageType::DamageType_Physical), 50.f, "VFX_Projectile_Bullet" };
            case EquippedWeaponCategory::Generic_Magic_Staff:
                return { false, 20.0f, base_player_attack_cooldown, create_dmg_inst(10, 16, DamageType::DamageType_Radiant), 30.f, "VFX_Magic_Bolt_Staff" };
            case EquippedWeaponCategory::Generic_Magic_Wand:
                return { false, 18.0f, base_player_attack_cooldown * 0.7f, create_dmg_inst(7, 11, DamageType::DamageType_Cosmic), 35.f, "VFX_Magic_Bolt_Wand" };
            case EquippedWeaponCategory::Unarmed:
            default:
                return { true, 1.5f, base_player_attack_cooldown, create_dmg_inst(1, 3, DamageType::DamageType_Physical), 0.f, "" };
            }
        }

        // --- Constructor ---
        GameplayEngine::GameplayEngine(RiftForged::GameLogic::PlayerManager& playerManager,
            RiftForged::Physics::PhysicsEngine& physicsEngine)
            : m_playerManager(playerManager),
            m_physicsEngine(physicsEngine) {
            RF_GAMEPLAY_INFO("GameplayEngine: Initialized and ready.");
        }

        // --- Initialize Players ---
        void GameplayEngine::InitializePlayerInWorld(
            RiftForged::GameLogic::ActivePlayer* player,
            const RiftForged::Networking::Shared::Vec3& spawn_position,
            const RiftForged::Networking::Shared::Quaternion& spawn_orientation) {

            if (!player) {
                RF_GAMEPLAY_ERROR("GameplayEngine::InitializePlayerInWorld: Null player pointer provided.");
                return;
            }
            if (player->playerId == 0) {
                RF_GAMEPLAY_ERROR("GameplayEngine::InitializePlayerInWorld: Attempted to initialize player with ID 0.");
                return;
            }

            RF_GAMEPLAY_INFO("GameplayEngine: Initializing player {} in world at Pos({:.2f}, {:.2f}, {:.2f}) Orient({:.2f},{:.2f},{:.2f},{:.2f})",
                player->playerId,
                spawn_position.x(), spawn_position.y(), spawn_position.z(),
                spawn_orientation.x(), spawn_orientation.y(), spawn_orientation.z(), spawn_orientation.w());

            // 1. Set initial logical state for the player
            player->SetPosition(spawn_position);           // Method from
            player->SetOrientation(spawn_orientation);     // Method from
            player->SetMovementState(RiftForged::GameLogic::PlayerMovementState::Idle); // Enum from
            player->SetAnimationState(RiftForged::Networking::Shared::AnimationState::AnimationState_Idle); // Enum from

            // Ensure ActivePlayer's capsule_radius and capsule_half_height are set.
            // These might be set in ActivePlayer's constructor to defaults, or loaded from character data.
            // If they are not set yet, you might want to set them here based on player type/class or defaults.
            // For this example, we assume they are already correctly set on the 'player' object.
            // Example if you needed to set them here:
            // player->capsule_radius = 0.5f; // Default or from config
            // player->capsule_half_height = 0.9f; // Default or from config

            if (player->capsule_radius <= 0.0f || player->capsule_half_height <= 0.0f) {
                RF_GAMEPLAY_ERROR("GameplayEngine::InitializePlayerInWorld: Player {} has invalid capsule dimensions (R: {:.2f}, HH: {:.2f}). Cannot create controller.",
                    player->playerId, player->capsule_radius, player->capsule_half_height);
                return;
            }

            // 2. Create the PhysX Character Controller via PhysicsEngine
            // The CreateCharacterController method in PhysicsEngine takes full height.
            bool controller_created = m_physicsEngine.CreateCharacterController( // Method from
                player->playerId,
                player->position,                   // Use the position we just set on ActivePlayer
                player->capsule_radius,             // Use radius from ActivePlayer
                player->capsule_half_height * 2.0f, // Pass full height
                nullptr,                            // Use default PxMaterial from PhysicsEngine
                reinterpret_cast<void*>(player->playerId) // Set player ID as user data on the PxActor
            );

            if (controller_created) {
                // 3. Set initial orientation in the physics world for the newly created controller's actor.
                // This ensures the PhysX actor's orientation matches the logical orientation.
                bool orientation_set = m_physicsEngine.SetCharacterControllerOrientation(player->playerId, player->orientation); // Method from

                if (orientation_set) {
                    RF_GAMEPLAY_INFO("Player {} PhysX controller created and initial pose set in world.", player->playerId);
                }
                else {
                    RF_GAMEPLAY_WARN("Player {} PhysX controller created, but failed to set initial orientation in physics world.", player->playerId);
                }
                // Player is now physically present in the world.
                // You might want to send an initial S2C_EntityStateUpdateMsg to the client here
                // if that's not handled elsewhere upon player joining.
            }
            else {
                RF_GAMEPLAY_ERROR("GameplayEngine: Failed to create PhysX controller for player {}. Player will lack physics presence.", player->playerId);
                // Handle this critical error appropriately.
                // The player object exists logically but not physically.
            }
        }

        // --- Player Actions Implementations ---

        void GameplayEngine::TurnPlayer(RiftForged::GameLogic::ActivePlayer* player, float turn_angle_degrees_delta) {
            if (!player) {
                RF_GAMEPLAY_ERROR("GameplayEngine::TurnPlayer: Called with null player.");
                return;
            }
            const RiftForged::Networking::Shared::Vec3 world_up_axis(0.0f, 0.0f, 1.0f);
            RiftForged::Networking::Shared::Quaternion rotation_delta_q =
                RiftForged::Utilities::Math::FromAngleAxis(turn_angle_degrees_delta, world_up_axis);

            RiftForged::Networking::Shared::Quaternion new_orientation =
                RiftForged::Utilities::Math::MultiplyQuaternions(player->orientation, rotation_delta_q);

            player->SetOrientation(RiftForged::Utilities::Math::NormalizeQuaternion(new_orientation));
        }

        void GameplayEngine::ProcessMovement(
            RiftForged::GameLogic::ActivePlayer* player,
            const RiftForged::Networking::Shared::Vec3& local_desired_direction_from_client,
            bool is_sprinting,
            float delta_time_sec) {

            if (!player) {
                RF_GAMEPLAY_ERROR("GameplayEngine::ProcessMovement: Null player.");
                return;
            }
            // Ensure playerID is valid for map lookups
            if (player->playerId == 0) {
                RF_GAMEPLAY_WARN("GameplayEngine::ProcessMovement: Invalid playerID (0) for player. Cannot fetch controller.");
                return;
            }
            if (player->movementState == RiftForged::GameLogic::PlayerMovementState::Stunned ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Rooted ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Dead) {
                return;
            }
            if (delta_time_sec <= 0.0f) return;

            float current_base_speed = BASE_WALK_SPEED_MPS; // From GameplayEngine.h
            // TODO: Adjust current_base_speed by player stats, buffs & debuffs.

            float actual_speed = current_base_speed * (is_sprinting ? SPRINT_SPEED_MULTIPLIER : 1.0f); // From GameplayEngine.h
            // TODO: Apply shield movement penalties etc.

            float displacement_amount = actual_speed * delta_time_sec;

            // With this (assuming Vec3 has x(), y(), z() accessors and a zero check is needed):
            if ((std::abs(local_desired_direction_from_client.x()) < 1e-6f &&
                std::abs(local_desired_direction_from_client.y()) < 1e-6f &&
                std::abs(local_desired_direction_from_client.z()) < 1e-6f) ||
                displacement_amount < 0.0001f) {
                if (player->movementState == RiftForged::GameLogic::PlayerMovementState::Walking || player->movementState == RiftForged::GameLogic::PlayerMovementState::Sprinting) {
                    player->SetMovementState(RiftForged::GameLogic::PlayerMovementState::Idle);
                }
                return;
            }

            RiftForged::Networking::Shared::Vec3 normalized_local_dir = RiftForged::Utilities::Math::NormalizeVector(local_desired_direction_from_client);
            RiftForged::Networking::Shared::Vec3 world_move_direction =
                RiftForged::Utilities::Math::RotateVectorByQuaternion(normalized_local_dir, player->orientation);
            // Ensure normalization if RotateVectorByQuaternion doesn't guarantee it for potentially non-unit quaternions,
            // or if subsequent scaling needs a precise unit vector. For PxController::move, it wants a displacement vector.
            // world_move_direction = RiftForged::Utilities::Math::NormalizeVector(world_move_direction); // Optional, depending on math lib

            RiftForged::Networking::Shared::Vec3 displacement_vector =
                RiftForged::Utilities::Math::ScaleVector(world_move_direction, displacement_amount);

            // --- NEW: Physics Interaction for Movement ---
            physx::PxController* px_controller = m_physicsEngine.GetPlayerController(player->playerId); //

            if (px_controller) {
                // The minimum travel distance, typically a small epsilon.
                // PhysX uses PxVec3 for displacement in controller->move. Your PhysicsEngine method takes SharedVec3.
                // Your PhysicsEngine::MoveCharacterController should handle the conversion if necessary.
                // For controller filters, if you want to ignore other players, you'd set up filter data and callbacks.
                // The current signature of your MoveCharacterController takes a vector of controllers to ignore.
                // For simplicity now, we'll pass an empty vector.
                std::vector<physx::PxController*> other_controllers_to_ignore;
                // You might want to populate this from m_playerManager if needed, excluding the current player's controller.

                RF_GAMEPLAY_DEBUG("GameplayEngine: Moving player {} controller with displacement ({:.2f}, {:.2f}, {:.2f})",
                    player->playerId, displacement_vector.x(), displacement_vector.y(), displacement_vector.z());

                physx::PxControllerCollisionFlags collisionFlags = static_cast<physx::PxControllerCollisionFlags>( // Cast return from uint32_t
                    m_physicsEngine.MoveCharacterController(
                        px_controller,
                        displacement_vector,
                        delta_time_sec, // Your method takes delta_time_sec, not minDist first as in my earlier generic PxController::move example
                        other_controllers_to_ignore // Pass the ignore list
                    )
                    );

                // Get the new position from the physics engine (after movement and collision resolution)
                // Your GetCharacterControllerPosition already returns SharedVec3

                // Replace 'SharedVec3' with the correct fully qualified type name
                RiftForged::Networking::Shared::Vec3 new_pos_from_physics = m_physicsEngine.GetCharacterControllerPosition(px_controller);
                player->SetPosition(new_pos_from_physics);

                RF_GAMEPLAY_DEBUG("GameplayEngine: Player {} new position after PhysX move: ({:.2f}, {:.2f}, {:.2f})",
                    player->playerId, new_pos_from_physics.x(), new_pos_from_physics.y(), new_pos_from_physics.z());

                // Example of using collision flags (you'll need to include PxQueryReport.h or similar for PxControllerCollisionFlag)
                if (collisionFlags & physx::PxControllerCollisionFlag::eCOLLISION_SIDES) {
                    RF_GAMEPLAY_DEBUG("Player {} collided with sides.", player->playerId);
                }
                if (collisionFlags & physx::PxControllerCollisionFlag::eCOLLISION_DOWN) {
                    // Player is on the ground (or hit something below)
                }
                if (collisionFlags & physx::PxControllerCollisionFlag::eCOLLISION_UP) {
                    RF_GAMEPLAY_DEBUG("Player {} collided above.", player->playerId);
                }

            }
            else {
                RF_GAMEPLAY_WARN("Player {} ProcessMovement - PhysX controller not found! Using direct kinematic move.", player->playerId);
                // Fallback to kinematic move (your existing placeholder code)
                RiftForged::Networking::Shared::Vec3 current_pos = player->position;
                RiftForged::Networking::Shared::Vec3 new_pos_direct = RiftForged::Utilities::Math::AddVectors(current_pos, displacement_vector);
                player->SetPosition(new_pos_direct);
            }
            // --- End NEW Physics Interaction ---

            player->SetMovementState(is_sprinting ? RiftForged::GameLogic::PlayerMovementState::Sprinting : RiftForged::GameLogic::PlayerMovementState::Walking);
        }

        RiftForged::GameLogic::RiftStepOutcome GameplayEngine::ExecuteRiftStep(
            RiftForged::GameLogic::ActivePlayer* player,
            RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent) { // intent from

            RiftForged::GameLogic::RiftStepOutcome outcome; // Defined in
            // Default constructor should initialize success = false;

            if (!player) {
                RF_GAMEPLAY_ERROR("ExecuteRiftStep: Null player received."); //
                outcome.success = false;
                outcome.failure_reason_code = "INTERNAL_ERROR_NULL_PLAYER";
                return outcome;
            }
            if (player->playerId == 0) { // Good to check for a valid player ID
                RF_GAMEPLAY_ERROR("ExecuteRiftStep: Invalid playerID (0) for player.");
                outcome.success = false;
                outcome.failure_reason_code = "INVALID_PLAYER_ID";
                return outcome;
            }

        if (!player->CanPerformRiftStep()) { // Method from
                outcome.success = false;
                outcome.failure_reason_code = player->IsAbilityOnCooldown(RiftForged::GameLogic::RIFTSTEP_ABILITY_ID) ? "ON_COOLDOWN" : "INVALID_PLAYER_STATE"; //
                RF_GAMEPLAY_INFO("Player {} RiftStep failed pre-check: {}", player->playerId, outcome.failure_reason_code); //
                return outcome;
            }

            // Player calculates its intended outcome based on its current_rift_step_definition and intent.
            // This should populate: outcome.actual_start_position, outcome.intended_target_position,
            // outcome.calculated_target_position (e.g., set to intended_target_position initially),
            // outcome.type_executed, outcome.entry_effects_data, outcome.exit_effects_data, VFX IDs etc.
            // Replace the problematic line with the following:  
            outcome = player->PrepareRiftStepOutcome(intent, player->current_rift_step_definition.type);
            if (!outcome.success) {
                RF_GAMEPLAY_INFO("Player {} RiftStep preparation failed internally by ActivePlayer: {}", player->playerId, outcome.failure_reason_code); //
                return outcome;
            }

            // --- Physics Interaction for RiftStep using Capsule Sweep ---
            physx::PxController* px_controller = m_physicsEngine.GetPlayerController(player->playerId); //
            physx::PxRigidActor* player_actor_to_ignore = nullptr;

            if (px_controller) {
                player_actor_to_ignore = px_controller->getActor();
            }
            else {
                RF_GAMEPLAY_ERROR("ExecuteRiftStep: Player {} has no PxController. Cannot perform physics sweep.", player->playerId);
                outcome.success = false;
                outcome.failure_reason_code = "NO_PHYSICS_CONTROLLER";
                return outcome;
            }

            // Get player's capsule dimensions (assuming these are now added to ActivePlayer)
            const float player_capsule_radius = player->capsule_radius;     // From ActivePlayer
            const float player_capsule_half_height = player->capsule_half_height; // From ActivePlayer

            // Calculate travel direction and distance from the outcome prepared by ActivePlayer
            RiftForged::Networking::Shared::Vec3 travel_direction_unit =
                RiftForged::Utilities::Math::SubtractVectors(outcome.intended_target_position, outcome.actual_start_position); // Using members from outcome
            float max_travel_distance = RiftForged::Utilities::Math::Magnitude(travel_direction_unit);

            // Default actual_final_position to start; it will be updated by sweep or if no travel needed.
            outcome.actual_final_position = outcome.actual_start_position; //

            if (max_travel_distance > 0.001f) { // Only sweep if there's a significant distance to travel
                travel_direction_unit = RiftForged::Utilities::Math::ScaleVector(travel_direction_unit, 1.0f / max_travel_distance); // Normalize

                RiftForged::Physics::HitResult hit_result; // Struct from

                // Define PxQueryFilterData for the sweep.
                // The primary filtering (dense vs. minor obstacles) will be handled by the
                // RiftStepSweepQueryFilterCallback inside PhysicsEngine::CapsuleSweepSingle.
                // This filterData can be used for broad-phase culling if needed.
                physx::PxFilterData query_filter_data_for_sweep;
                // Example: (You'll set this based on your collision layers/groups)
                // query_filter_data_for_sweep.word0 = YOUR_RIFTSTEP_SWEEP_COLLISION_MASK; 
                physx::PxQueryFilterData physx_query_filter_data(query_filter_data_for_sweep, physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::ePOSTFILTER);

                bool found_blocking_hit = m_physicsEngine.CapsuleSweepSingle( // Method from
                    outcome.actual_start_position,    // Start position of the sweep
                    player->orientation,              // Current orientation of the player for the capsule
                    player_capsule_radius,
                    player_capsule_half_height,
                    travel_direction_unit,            // Normalized direction of travel
                    max_travel_distance,
                    hit_result,                       // Output hit result
                    player_actor_to_ignore,           // Player's own PxRigidActor to ignore in the sweep
                    physx_query_filter_data           // Pass the filter data
                );

                if (found_blocking_hit) {
                    // A "dense" obstacle (as determined by your RiftStepSweepQueryFilterCallback) was hit.
                    // Set final position to just before the hit point.
                    // A small offset helps prevent getting stuck if the hit was exactly at a boundary.
                    float safe_distance = std::max(0.0f, hit_result.distance - (player_capsule_radius * 0.1f));
                    outcome.actual_final_position = RiftForged::Utilities::Math::AddVectors(
                        outcome.actual_start_position,
                        RiftForged::Utilities::Math::ScaleVector(travel_direction_unit, safe_distance)
                    );
                    RF_GAMEPLAY_INFO("Player {} RiftStep OBSTRUCTED by dense object. Intended Target: ({:.1f},{:.1f},{:.1f}), Actual Final: ({:.1f},{:.1f},{:.1f}) at dist {:.2f}",
                        player->playerId,
                        outcome.intended_target_position.x(), outcome.intended_target_position.y(), outcome.intended_target_position.z(),
                        outcome.actual_final_position.x(), outcome.actual_final_position.y(), outcome.actual_final_position.z(),
                        safe_distance);
                }
                else {
                    // No "dense" blocking hit found. Player reached the intended target (or passed through minor obstacles).
                    outcome.actual_final_position = outcome.intended_target_position;
                    RF_GAMEPLAY_INFO("Player {} RiftStep path clear to intended target (or passed through minor obstacles). Final Pos: ({:.1f},{:.1f},{:.1f})",
                        player->playerId, outcome.actual_final_position.x(), outcome.actual_final_position.y(), outcome.actual_final_position.z());
                }
            }
            else {
                // No significant travel distance, actual_final_position remains actual_start_position.
                RF_GAMEPLAY_INFO("Player {} RiftStep: No significant travel distance requested.", player->playerId);
            }

            // Physically move the character controller to the (potentially adjusted) actual_final_position.
            // SetCharacterControllerPose is suitable for this teleport-like repositioning.
            m_physicsEngine.SetCharacterControllerPose(px_controller, outcome.actual_final_position); // Method from

            // Update the ActivePlayer's logical position to match the physics outcome.
            // This will also set the isDirty flag.
            player->SetPosition(outcome.actual_final_position); // Method from

            // Apply Cooldown using the definition's base cooldown.
            player->SetAbilityCooldown(RiftForged::GameLogic::RIFTSTEP_ABILITY_ID, player->current_rift_step_definition.base_cooldown_sec); //

            // TODO: Implement robust application of entry/exit effects from outcome.entry_effects_data and outcome.exit_effects_data
            // This would involve iterating them, determining targets, and applying damage, buffs, creating persistent areas etc.
            // For example:
            // ApplyRiftStepEffects(player, outcome.actual_start_position, outcome.entry_effects_data);
            // ApplyRiftStepEffects(player, outcome.actual_final_position, outcome.exit_effects_data);

            RF_GAMEPLAY_INFO("Player {} RiftStep EXECUTED. Type: {}. Effects: Entry({}), Exit({}). Target: ({:.1f},{:.1f},{:.1f}), Final: ({:.1f},{:.1f},{:.1f})",
                player->playerId, static_cast<int>(outcome.type_executed),
                outcome.entry_effects_data.size(), outcome.exit_effects_data.size(),
                outcome.intended_target_position.x(), outcome.intended_target_position.y(), outcome.intended_target_position.z(),
                outcome.actual_final_position.x(), outcome.actual_final_position.y(), outcome.actual_final_position.z()
            );

            player->SetMovementState(RiftForged::GameLogic::PlayerMovementState::Idle); //
            player->SetAnimationState(RiftForged::Networking::Shared::AnimationState::AnimationState_Rifting_End); //

            outcome.success = true;
            // The S2C message will use outcome.actual_start_position, outcome.calculated_target_position,
            // outcome.actual_final_position, outcome.travel_duration_sec etc.
            // Ensure player->PrepareRiftStepOutcome() correctly sets outcome.calculated_target_position,
            // or set it here if it's simply the intended target before physics.
            // outcome.calculated_target_position = outcome.intended_target_position; // Already assumed to be done by PrepareRiftStepOutcome or now explicitly.

            return outcome;
        }

        RiftForged::GameLogic::AttackOutcome GameplayEngine::ExecuteBasicAttack(
            RiftForged::GameLogic::ActivePlayer* attacker,
            const RiftForged::Networking::Shared::Vec3& world_aim_direction,
            uint64_t optional_target_entity_id) {

            using namespace RiftForged::GameLogic;
            using namespace RiftForged::Networking::Shared;
            using namespace RiftForged::Networking::UDP::S2C; // For CombatEventType

            AttackOutcome outcome; // Default constructor (from your CombatLogic.h)
            outcome.is_basic_attack = true;

            if (!attacker) {
                outcome.success = false; outcome.failure_reason_code = "INVALID_ATTACKER";
                RF_GAMEPLAY_ERROR("ExecuteBasicAttack: Null attacker."); return outcome;
            }

            if (attacker->movementState == PlayerMovementState::Stunned ||
                attacker->movementState == PlayerMovementState::Rooted ||
                attacker->movementState == PlayerMovementState::Dead) {
                outcome.success = false; outcome.failure_reason_code = "INVALID_PLAYER_STATE"; return outcome;
            }

            TempWeaponProperties weapon_props = GetStubbedWeaponProperties(attacker);

            if (attacker->IsAbilityOnCooldown(GameLogic::BASIC_ATTACK_ABILITY_ID)) {
                outcome.success = false; outcome.failure_reason_code = "ON_COOLDOWN"; return outcome;
            }
            attacker->SetAbilityCooldown(GameLogic::BASIC_ATTACK_ABILITY_ID, weapon_props.attackCooldownSec);
            attacker->SetMovementState(PlayerMovementState::Ability_In_Use);
            attacker->SetAnimationState(AnimationState::AnimationState_Attacking_Primary);

            switch (attacker->current_weapon_category) {
            case EquippedWeaponCategory::Generic_Melee_Sword: case EquippedWeaponCategory::Generic_Melee_Axe: case EquippedWeaponCategory::Generic_Melee_Maul: case EquippedWeaponCategory::Unarmed:
                outcome.attack_animation_tag_for_caster = "Attack_Melee_Basic"; break;
            case EquippedWeaponCategory::Generic_Ranged_Bow: case EquippedWeaponCategory::Generic_Ranged_Gun:
                outcome.attack_animation_tag_for_caster = "Attack_Ranged_Basic"; break;
            case EquippedWeaponCategory::Generic_Magic_Staff: case EquippedWeaponCategory::Generic_Magic_Wand:
                outcome.attack_animation_tag_for_caster = "Attack_Magic_Basic"; break;
            }

            if (weapon_props.isMelee) {
                outcome.simulated_combat_event_type = CombatEventType_Miss; // Default for melee if no valid target hit
                ActivePlayer* target_player = nullptr;
                if (optional_target_entity_id != 0 && optional_target_entity_id != attacker->playerId) {
                    target_player = m_playerManager.FindPlayerById(optional_target_entity_id);
                }

                if (target_player && target_player->movementState != PlayerMovementState::Dead) {
                    float dist_sq = Utilities::Math::DistanceSquared(attacker->position, target_player->position);
                    Vec3 dir_to_target = Utilities::Math::NormalizeVector(Utilities::Math::SubtractVectors(target_player->position, attacker->position));
                    Vec3 normalized_aim_dir = Utilities::Math::NormalizeVector(world_aim_direction); // Client sends this
                    float dot_product = Utilities::Math::DotProduct(normalized_aim_dir, dir_to_target);

                    // TODO: Implement robust physics-based melee hit detection (e.g., sphere sweep from PhysicsEngine)
                    if (dist_sq <= weapon_props.range * weapon_props.range && dot_product > 0.707f) { // Simple range & cone check
                        DamageApplicationDetails hit_details;
                        hit_details.target_id = target_player->playerId;
                        hit_details.damage_type = weapon_props.baseDamageInstance.type();

                        // TODO: Implement Accuracy Roll & Critical Hit Roll
                        bool is_crit = false; // Placeholder for actual crit roll
                        int damage_to_deal = weapon_props.baseDamageInstance.amount();
                        if (is_crit) {
                            damage_to_deal = static_cast<int>(static_cast<float>(damage_to_deal) * attacker->base_critical_hit_damage_multiplier);
                        }
                        hit_details.was_crit = is_crit;
                        hit_details.final_damage_dealt = damage_to_deal; // Pre-mitigation damage for event

                        target_player->TakeDamage(damage_to_deal, hit_details.damage_type);

                        hit_details.was_kill = (target_player->currentHealth == 0);
                        outcome.damage_events.push_back(hit_details);
                        outcome.simulated_combat_event_type = CombatEventType_DamageDealt;
                    }
                    else { outcome.failure_reason_code = "OUT_OF_RANGE_OR_LOS"; }
                }
                else if (optional_target_entity_id != 0) { outcome.failure_reason_code = "TARGET_INVALID_OR_DEAD"; }
            }
            else { // Ranged Attack
                // For ranged, the ExecuteBasicAttack spawns the projectile.
                // The "hit" and "damage" events will be generated later when the projectile update logic detects a collision.
                outcome.simulated_combat_event_type = CombatEventType::CombatEventType_MIN; // Or a new CombatEventType_ProjectileFired if you add it. The main info is S2C_SpawnProjectileMsg.
                outcome.spawned_projectile = true;
                outcome.projectile_id = m_playerManager.GetNextAvailableProjectileID();
                outcome.projectile_start_position = attacker->GetMuzzlePosition();
                outcome.projectile_direction = Utilities::Math::NormalizeVector(world_aim_direction);
                outcome.projectile_speed = weapon_props.projectileSpeed;
                outcome.projectile_max_range = weapon_props.range;
                outcome.projectile_vfx_tag = weapon_props.projectileVfxTag;
                outcome.projectile_damage_on_hit = weapon_props.baseDamageInstance; // Projectile carries this damage package

                RF_GAMEPLAY_INFO("Player {} Basic Attack: SPAWNED Projectile ID {} (Dmg: {}, Type: {})",
                    attacker->playerId, outcome.projectile_id,
                    outcome.projectile_damage_on_hit.amount(), EnumNameDamageType(outcome.projectile_damage_on_hit.type()));
                // The GameplayEngine's main loop (or networking layer) will see outcome.spawned_projectile == true
                // and then construct and send an S2C_SpawnProjectileMsg using these details.
            }

            outcome.success = true;
            if (attacker->movementState == PlayerMovementState::Ability_In_Use) {
                attacker->SetMovementState(PlayerMovementState::Idle); // Quick reset; better tied to animation length
            }
            return outcome;
        }

    } // namespace Gameplay
} // namespace RiftForged