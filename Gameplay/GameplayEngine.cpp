// GameplayEngine/GameplayEngine.cpp
#include "GameplayEngine.h"
#include <iostream> // For logging
#include <cmath>    // For sqrt, acos, etc. if needed for vector math
#include "../NetworkEngine/NetworkEndpoint.h" // For NetworkEndpoint if needed

// For M_PI if not defined on Windows with <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace RiftForged {
    namespace Gameplay {

        // Constructor (can be empty for now if no complex setup)
        GameplayEngine::GameplayEngine(/* Potential dependencies like RiftForged::Physics::PhysicsEngine& physics */)
            /* : m_physicsEngine(physics) // Example if injecting physics engine */ {
            std::cout << "GameplayEngine: Initialized." << std::endl;
        }

        void GameplayEngine::TurnPlayer(RiftForged::GameLogic::ActivePlayer* player, float turn_angle_degrees_delta) {
            if (!player) {
                std::cerr << "GameplayEngine::TurnPlayer: Null player." << std::endl;
                return;
            }

            // Assuming World UP is positive Z for yaw rotation. 
            // This constant could be part of MathUtils or GameConstants.
            const RiftForged::Networking::Shared::Vec3 world_up_axis(0.0f, 0.0f, 1.0f);

            RiftForged::Networking::Shared::Quaternion rotation_delta_q =
                RiftForged::Utilities::Math::FromAngleAxis(turn_angle_degrees_delta, world_up_axis);

            // Apply rotation: new_orientation = rotation_delta_q * player->orientation
            RiftForged::Networking::Shared::Quaternion new_orientation =
                RiftForged::Utilities::Math::MultiplyQuaternions(rotation_delta_q, player->orientation);

            player->SetOrientation(RiftForged::Utilities::Math::NormalizeQuaternion(new_orientation));

            // std::cout << "GameplayEngine: Player " << player->playerId << " turned." << std::endl;
        }

        void GameplayEngine::ProcessMovement(
            RiftForged::GameLogic::ActivePlayer* player,
            const RiftForged::Networking::Shared::Vec3& local_desired_direction, // e.g., (0,1,0) for client's forward
            bool is_sprinting) {

            if (!player) {
                std::cerr << "GameplayEngine::ProcessMovement: Null player." << std::endl;
                return;
            }
            // Prevent movement if player is in a state that disallows it
            if (player->movementState == RiftForged::GameLogic::PlayerMovementState::Stunned ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Rooted ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Dead) {
                // std::cout << "GameplayEngine: Player " << player->playerId << " cannot move due to state: " << static_cast<int>(player->movementState) << std::endl;
                return;
            }

            float speed_multiplier = is_sprinting ? SPRINT_SPEED_MULTIPLIER : 1.0f;
            float displacement_this_call = BASE_WALK_DISPLACEMENT_PER_CALL * speed_multiplier;

            // Transform local_desired_direction (relative to player) to a world-space direction vector
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

            // --- FUTURE PHYSICS INTEGRATION POINT ---
            // RiftForged::Networking::Shared::Vec3 actual_new_position = 
            //     m_physicsEngine.MoveCharacter(player->playerId, current_position, intended_new_position);
            RiftForged::Networking::Shared::Vec3 actual_new_position = intended_new_position; // For now, direct move

            player->SetPosition(actual_new_position);
            player->SetMovementState(is_sprinting ? RiftForged::GameLogic::PlayerMovementState::Sprinting
                : RiftForged::GameLogic::PlayerMovementState::Walking);
            // Set animation state based on movement
            player->SetAnimationState(is_sprinting ?
                static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState_Running) :
                static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState_Walking)
            );

            // std::cout << "GameplayEngine: Player " << player->playerId << " processed movement. New Pos: ("
            //           << player->position.x() << "," << player->position.y() << "," << player->position.z() << ")" << std::endl;
        }

        RiftForged::GameLogic::RiftStepOutcome GameplayEngine::ExecuteRiftStep(
            RiftForged::GameLogic::ActivePlayer* player,
            RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent) {

            using RiftForged::GameLogic::RiftStepOutcome;
            using RiftForged::GameLogic::GameplayEffectInstance; // Assuming this is also in GameLogic namespace now
            using RiftForged::Networking::Shared::Vec3;
            using RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Default_Backward;
            using RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Forward;
            using RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Backward;
            using RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Left;
            using RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent_Intentional_Right;

            RiftStepOutcome outcome;
            if (!player) {
                outcome.success = false;
                outcome.failure_reason_code = "INVALID_PLAYER";
                std::cerr << "GameplayEngine::ExecuteRiftStep: Null player." << std::endl;
                return outcome;
            }

            if (player->movementState == RiftForged::GameLogic::PlayerMovementState::Stunned ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Rooted ||
                player->movementState == RiftForged::GameLogic::PlayerMovementState::Dead) {
                outcome.success = false;
                outcome.failure_reason_code = "INVALID_PLAYER_STATE";
                return outcome;
            }

            if (player->IsAbilityOnCooldown(RIFTSTEP_ABILITY_ID)) {
                outcome.success = false;
                outcome.failure_reason_code = "ON_COOLDOWN";
                // std::cout << "GameplayEngine: RiftStep for Player " << player->playerId << " is ON COOLDOWN." << std::endl;
                return outcome;
            }

            outcome.actual_start_position = player->position;
            Vec3 step_direction_vector;

            Vec3 player_world_forward = RiftForged::Utilities::Math::GetWorldForwardVector(player->orientation);
            Vec3 player_world_right = RiftForged::Utilities::Math::GetWorldRightVector(player->orientation);

            switch (intent) {
            case RiftStepDirectionalIntent_Intentional_Forward:  step_direction_vector = player_world_forward; break;
            case RiftStepDirectionalIntent_Intentional_Backward: step_direction_vector = { -player_world_forward.x(), -player_world_forward.y(), -player_world_forward.z() }; break;
            case RiftStepDirectionalIntent_Intentional_Left:     step_direction_vector = { -player_world_right.x(), -player_world_right.y(), -player_world_right.z() }; break;
            case RiftStepDirectionalIntent_Intentional_Right:    step_direction_vector = player_world_right; break;
            case RiftStepDirectionalIntent_Default_Backward:
            default:                                            step_direction_vector = { -player_world_forward.x(), -player_world_forward.y(), -player_world_forward.z() }; break;
            }
            step_direction_vector = RiftForged::Utilities::Math::NormalizeVector(step_direction_vector);

            // TODO: Apply passives from player->allocated_passive_node_ids to affect these base values
            float effective_distance = player->base_riftstep_distance;
            float effective_cooldown_sec = player->base_riftstep_cooldown_sec;
            outcome.travel_duration_sec = RIFTSTEP_COSMETIC_TRAVEL_TIME_SEC;

            effective_cooldown_sec = std::max(RIFTSTEP_MIN_COOLDOWN_SEC, effective_cooldown_sec);
            int final_cooldown_ms = static_cast<int>(effective_cooldown_sec * 1000.0f);

            outcome.calculated_target_position = Vec3(
                player->position.x() + step_direction_vector.x() * effective_distance,
                player->position.y() + step_direction_vector.y() * effective_distance,
                player->position.z() + step_direction_vector.z() * effective_distance
            );

            // --- FUTURE PHYSICS INTEGRATION POINT ---
            // outcome.actual_final_position = m_physicsEngine->SweepCharacter(player->playerId, outcome.actual_start_position, outcome.calculated_target_position);
            outcome.actual_final_position = outcome.calculated_target_position; // For now, teleport is direct

            // --- INSTANTLY Update Player State on Server ---
            player->SetPosition(outcome.actual_final_position);
            player->SetMovementState(RiftForged::GameLogic::PlayerMovementState::Idle); // Player is actionable immediately after instant rift
            player->SetAbilityCooldown(RIFTSTEP_ABILITY_ID, final_cooldown_ms);
            player->SetAnimationState(static_cast<uint32_t>(RiftForged::Networking::Shared::AnimationState_Rifting_End)); // Or a general "AbilityUse" anim

            // Determine Entry & Exit ("Impact") effects based on player->activeRiftStepModifierId & passives
            // TODO: This logic should be more data-driven based on the modifier definition.
            if (player->activeRiftStepModifierId == 1) { // Example: Solar Detonation (GDD Ref: Solarii Vanguard)
                outcome.start_vfx_id = "VFX_RiftStep_Solar_Start"; // Or just "Impact" if combined
                outcome.end_vfx_id = "VFX_RiftStep_Solar_End";   // 
                outcome.travel_vfx_id = outcome.start_vfx_id; // For a quick visual flash

                // Entry effect (happens at start point)
                GameplayEffectInstance entry_stun;
                entry_stun.effect_payload_type = RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaStun;
                entry_stun.center_position = outcome.actual_start_position;
                entry_stun.radius = 3.0f;
                entry_stun.stun = { RiftForged::Networking::Shared::StunSeverity_Medium, 2000 };
                outcome.entry_effects_data.push_back(entry_stun);

                // Exit effect (happens at end point)
                GameplayEffectInstance exit_damage;
                exit_damage.effect_payload_type = RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaDamage;
                exit_damage.center_position = outcome.actual_final_position;
                exit_damage.radius = 5.0f;
                exit_damage.damage = { 150, RiftForged::Networking::Shared::DamageType_Radiant }; // Make sure DamageType_Radiant is defined
                outcome.exit_effects_data.push_back(exit_damage);
            }
            else { // Default RiftStep
                outcome.start_vfx_id = "VFX_RiftStep_Default_Instant";
                outcome.end_vfx_id = outcome.start_vfx_id;
            }

            outcome.success = true;
            std::cout << "GameplayEngine: Player " << player->playerId << " INSTANT RiftStep. Start: ("
                << outcome.actual_start_position.x() << "," << outcome.actual_start_position.y() << "," << outcome.actual_start_position.z()
                << "), New Pos: (" << outcome.actual_final_position.x() << "," << outcome.actual_final_position.y() << "," << outcome.actual_final_position.z()
                << "). Cooldown set. Cosmetic Travel: " << outcome.travel_duration_sec << "s." << std::endl;

            return outcome;
        }

    }
} // namespace RiftForged::Gameplay