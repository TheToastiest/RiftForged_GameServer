#pragma once
// Ensure ActivePlayer.h path is correct from GameplayEngine's perspective
#include "ActivePlayer.h" 
#include "RiftStepLogic.h"  // For RiftStepOutcome
// For C2S::RiftStepDirectionalIntent from the FlatBuffers messages
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h"
#include "../Utils/MathUtil.h"

namespace RiftForged {
    // Forward declare PhysicsEngine if it's a dependency to be injected later
    // namespace Physics { class PhysicsEngine; }

    namespace Gameplay {

        class GameplayEngine {
        public:
            GameplayEngine(/* Potential dependencies like PhysicsEngine& physics */);

            // --- Player Actions ---
            void TurnPlayer(RiftForged::GameLogic::ActivePlayer* player, float turn_angle_degrees_delta);

            void ProcessMovement(
                RiftForged::GameLogic::ActivePlayer* player,
                const RiftForged::Networking::Shared::Vec3& local_desired_direction_from_client,
                bool is_sprinting
            );

            RiftForged::GameLogic::RiftStepOutcome ExecuteRiftStep(
                RiftForged::GameLogic::ActivePlayer* player,
                RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent
            );

            // TODO: Add more ability execution methods here, e.g.:
            // SomeAbilityOutcome ExecuteSolarFlare(GameLogic::ActivePlayer* caster, const Networking::Shared::Vec3& target_point);

        private:
            // RiftForged::Physics::PhysicsEngine* m_physicsEngine; // Example dependency

            // --- Game Constants (can be loaded from config files later) ---
            // RiftStep
            const float RIFTSTEP_BASE_DISTANCE = 5.0f;
            const float RIFTSTEP_BASE_COOLDOWN_SEC = 1.25f;
            const float RIFTSTEP_COSMETIC_TRAVEL_TIME_SEC = 0.25f; // For client visuals, server logic is instant
            const float RIFTSTEP_MIN_COOLDOWN_SEC = 0.5f;
            const uint32_t RIFTSTEP_ABILITY_ID = 1001;      // Example unique ID for RiftStep for cooldown map

            // Movement
            // Displacement per logic call (server tick should define delta_time for speed later)
            const float BASE_WALK_DISPLACEMENT_PER_CALL = 0.1f;
            const float SPRINT_SPEED_MULTIPLIER = 1.5f;
            const float PLAYER_TURN_DEGREES_PER_INPUT_PRESS = 5.0f; // Example default turn increment
        };

    }
} // namespace RiftForged::Gameplay