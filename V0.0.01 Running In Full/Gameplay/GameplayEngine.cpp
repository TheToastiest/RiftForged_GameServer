// GameplayEngine/GameplayEngine.cpp
#include "GameplayEngine.h"
#include <iostream> // For logging
#include <cmath>    // For sqrt, acos, etc. if needed for vector math
#include <algorithm>
#include "../NetworkEngine/NetworkEndpoint.h" // For NetworkEndpoint if needed
#include "../Utils/Logger.h"

// For M_PI if not defined on Windows with <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace RiftForged {
    namespace Gameplay {

        // Constructor
        GameplayEngine::GameplayEngine(/* Potential dependencies like RiftForged::Physics::PhysicsEngine& physics */)
            /* : m_physicsEngine(physics) // Example if injecting physics engine */ {
            RF_GAMEPLAY_INFO("GameplayEngine: Initialized.");
        }

        // --- Player Action Implementations ---

        void GameplayEngine::TurnPlayer(RiftForged::GameLogic::ActivePlayer* player, float turn_angle_degrees_delta) {
            if (!player) {
                RF_GAMEPLAY_ERROR("GameplayEngine::TurnPlayer: Called with null player.");
                return;
            }

            // Assuming World UP is positive Z for yaw rotation. 
            const RiftForged::Networking::Shared::Vec3 world_up_axis(0.0f, 0.0f, 1.0f);

            RiftForged::Networking::Shared::Quaternion rotation_delta_q =
                RiftForged::Utilities::Math::FromAngleAxis(turn_angle_degrees_delta, world_up_axis);

            // Apply rotation: new_orientation = rotation_delta_q * current_orientation
            RiftForged::Networking::Shared::Quaternion new_orientation =
                RiftForged::Utilities::Math::MultiplyQuaternions(rotation_delta_q, player->orientation);

            player->SetOrientation(RiftForged::Utilities::Math::NormalizeQuaternion(new_orientation));

            RF_GAMEPLAY_TRACE("Player {} turned by {} degrees.", player->playerId, turn_angle_degrees_delta);
        }

        void GameplayEngine::ProcessMovement(
            RiftForged::GameLogic::ActivePlayer* player,
            const RiftForged::Networking::Shared::Vec3& local_desired_direction, // e.g., (0,1,0) for client's forward
            bool is_sprinting) {

            if (!player) {
                RF_GAMEPLAY_ERROR("GameplayEngine::ProcessMovement: Null player.");
                return;
            }
            if (player->movementState == RiftForged::GameLogic::PlayerMovementState::Stunned ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Rooted ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Dead) {
                RF_GAMEPLAY_DEBUG("Player {} cannot move due to state: {}", player->playerId, static_cast<int>(player->movementState));
                return;
            }

            float speed_multiplier = is_sprinting ? SPRINT_SPEED_MULTIPLIER : 1.0f;
            float displacement_this_call = BASE_WALK_DISPLACEMENT_PER_CALL * speed_multiplier;

            RiftForged::Networking::Shared::Vec3 world_move_direction =
                RiftForged::Utilities::Math::RotateVectorByQuaternion(local_desired_direction, player->orientation);

            world_move_direction = RiftForged::Utilities::Math::NormalizeVector(world_move_direction);

            RiftForged::Networking::Shared::Vec3 displacement(
                world_move_direction.x() * displacement_this_call,
                world_move_direction.y() * displacement_this_call,
                world_move_direction.z() * displacement_this_call
            );

            RiftForged::Networking::Shared::Vec3 current_position = player->position;
            RiftForged::Networking::Shared::Vec3 intended_new_position(
                current_position.x() + displacement.x(),
                current_position.y() + displacement.y(),
                current_position.z() + displacement.z()
            );

            // TODO: Incorporate PhysicsEngine.SweepTest(player->playerId, current_position, intended_new_position, player_radius) to get actual_new_position
            RiftForged::Networking::Shared::Vec3 actual_new_position = intended_new_position; // For now, direct move

            player->SetPosition(actual_new_position);
            player->SetMovementState(is_sprinting ? RiftForged::GameLogic::PlayerMovementState::Sprinting
                : RiftForged::GameLogic::PlayerMovementState::Walking);
            player->SetAnimationState(is_sprinting ?
                RiftForged::Networking::Shared::AnimationState_Running :  // Pass enum directly
                RiftForged::Networking::Shared::AnimationState_Walking
            );

            RF_GAMEPLAY_TRACE("Player {} processed movement. From: ({:.1f},{:.1f},{:.1f}) To: ({:.1f},{:.1f},{:.1f}), LocalDir: ({:.1f},{:.1f},{:.1f})",
                player->playerId,
                current_position.x(), current_position.y(), current_position.z(),
                player->position.x(), player->position.y(), player->position.z(),
                local_desired_direction.x(), local_desired_direction.y(), local_desired_direction.z());
        }

        RiftForged::GameLogic::RiftStepOutcome GameplayEngine::ExecuteRiftStep(
            RiftForged::GameLogic::ActivePlayer* player,
            RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent) {

            // Namespace aliases for convenience within this function
            using RiftForged::GameLogic::RiftStepOutcome;
            using RiftForged::GameLogic::GameplayEffectInstance;
            using RiftForged::Networking::Shared::Vec3;
            using namespace RiftForged::Networking::UDP::C2S; // For RiftStepDirectionalIntent enum values
            using namespace RiftForged::Networking::UDP::S2C; // For RiftStepEffectPayload enum values
            using namespace RiftForged::Networking::Shared;   // For DamageType, StunSeverity, AnimationState enums

            RiftStepOutcome outcome;
            if (!player) {
                outcome.success = false;
                outcome.failure_reason_code = "INVALID_PLAYER";
                RF_GAMEPLAY_ERROR("ExecuteRiftStep: Called with null player.");
                return outcome;
            }

            RF_GAMEPLAY_DEBUG("Player {} attempting RiftStep. Intent: {} ({})",
                player->playerId, EnumNameRiftStepDirectionalIntent(intent), static_cast<int>(intent));

            // 1. Check Preconditions
            if (player->movementState == GameLogic::PlayerMovementState::Stunned ||
                player->movementState == GameLogic::PlayerMovementState::Rooted ||
                player->movementState == GameLogic::PlayerMovementState::Dead) {
                outcome.success = false;
                outcome.failure_reason_code = "INVALID_PLAYER_STATE";
                RF_GAMEPLAY_WARN("Player {} cannot RiftStep due to movement state: {}", player->playerId, static_cast<int>(player->movementState));
                return outcome;
            }

            if (player->IsAbilityOnCooldown(RIFTSTEP_ABILITY_ID)) {
                outcome.success = false;
                outcome.failure_reason_code = "ON_COOLDOWN";
                RF_GAMEPLAY_INFO("RiftStep for Player {} is ON COOLDOWN.", player->playerId);
                return outcome;
            }

            // (Resource Cost Check - currently 0 "Will")

            // --- Calculate Effective Parameters (base + future passives/modifiers) ---
            // TODO: Apply passive modifications from player->allocated_passive_node_ids to these base values
            float effective_distance = player->base_riftstep_distance;
            float effective_cooldown_sec = player->base_riftstep_cooldown_sec;
            // Cosmetic travel time for client visuals, server resolution is instant.
            outcome.travel_duration_sec = RIFTSTEP_COSMETIC_TRAVEL_TIME_SEC;

            effective_cooldown_sec = std::max(RIFTSTEP_MIN_COOLDOWN_SEC, effective_cooldown_sec);
            int final_cooldown_ms = static_cast<int>(effective_cooldown_sec * 1000.0f);

            // --- Determine Direction and Calculate Target ---
            outcome.actual_start_position = player->position;
            Vec3 step_direction_vector;

            Vec3 player_world_forward = Utilities::Math::GetWorldForwardVector(player->orientation);
            Vec3 player_world_right = Utilities::Math::GetWorldRightVector(player->orientation);

            switch (intent) {
            case RiftStepDirectionalIntent_Intentional_Forward:  step_direction_vector = player_world_forward; break;
            case RiftStepDirectionalIntent_Intentional_Backward: step_direction_vector = { -player_world_forward.x(), -player_world_forward.y(), -player_world_forward.z() }; break;
            case RiftStepDirectionalIntent_Intentional_Left:     step_direction_vector = { -player_world_right.x(), -player_world_right.y(), -player_world_right.z() }; break;
            case RiftStepDirectionalIntent_Intentional_Right:    step_direction_vector = player_world_right; break;
            case RiftStepDirectionalIntent_Default_Backward:
            default:                                            step_direction_vector = { -player_world_forward.x(), -player_world_forward.y(), -player_world_forward.z() }; break;
            }
            step_direction_vector = Utilities::Math::NormalizeVector(step_direction_vector);

            outcome.calculated_target_position = Vec3(
                player->position.x() + step_direction_vector.x() * effective_distance,
                player->position.y() + step_direction_vector.y() * effective_distance,
                player->position.z() + step_direction_vector.z() * effective_distance
            );

            // --- Physics/Collision Check (Placeholder) ---
            // outcome.actual_final_position = m_physicsEngine->SweepCharacter(player->playerId, outcome.actual_start_position, outcome.calculated_target_position);
            outcome.actual_final_position = outcome.calculated_target_position;

            // --- INSTANTLY Update Player State on Server ---
            player->SetPosition(outcome.actual_final_position);
            player->SetMovementState(RiftForged::GameLogic::PlayerMovementState::Idle);
            player->SetAbilityCooldown(RIFTSTEP_ABILITY_ID, final_cooldown_ms);
            player->SetAnimationState(AnimationState_Rifting_End); // Use the direct enum from Shared namespace

            // --- Determine Effects & Populate Outcome Data for S2C Message ---
            // TODO: This logic should be data-driven based on player->activeRiftStepModifierId and passives.
            if (player->activeRiftStepModifierId == 1) { // Example: Solar Detonation
                outcome.start_vfx_id = "VFX_RiftStep_Solar_Start";
                outcome.travel_vfx_id = "VFX_RiftStep_Solar_Travel_Short";
                outcome.end_vfx_id = "VFX_RiftStep_Solar_End";

                GameplayEffectInstance entry_stun;
                entry_stun.effect_payload_type = RiftStepEffectPayload_AreaStun; // Use S2C enum value
                entry_stun.center_position = outcome.actual_start_position;
                entry_stun.radius = 3.0f;
                entry_stun.stun = { StunSeverity_Medium, 2000 }; // Use Shared enum value
                outcome.entry_effects_data.push_back(entry_stun);

                GameplayEffectInstance exit_damage;
                exit_damage.effect_payload_type = RiftStepEffectPayload_AreaDamage;
                exit_damage.center_position = outcome.actual_final_position;
                exit_damage.radius = 5.0f;
                exit_damage.damage = { 150, DamageType_Radiant }; // Use Shared enum value
                outcome.exit_effects_data.push_back(exit_damage);
                RF_GAMEPLAY_DEBUG("Player {} RiftStep generated Solar effects (Modifier ID: 1).", player->playerId);
            }
            else {
                outcome.start_vfx_id = "VFX_RiftStep_Default_Start";
                outcome.travel_vfx_id = "";
                outcome.end_vfx_id = "VFX_RiftStep_Default_End";
                RF_GAMEPLAY_DEBUG("Player {} RiftStep generated Default effects (Modifier ID: {}).", player->playerId, player->activeRiftStepModifierId);
            }

            outcome.success = true;
            RF_GAMEPLAY_INFO("Player {} RiftStep SUCCESS. Start: ({:.1f},{:.1f},{:.1f}), NewPos: ({:.1f},{:.1f},{:.1f}). CD: {}ms. CosmeticTravel: {}s.",
                player->playerId,
                outcome.actual_start_position.x(), outcome.actual_start_position.y(), outcome.actual_start_position.z(),
                outcome.actual_final_position.x(), outcome.actual_final_position.y(), outcome.actual_final_position.z(),
                final_cooldown_ms, outcome.travel_duration_sec);

            return outcome;
        }

    }
} // namespace RiftForged::Gameplay