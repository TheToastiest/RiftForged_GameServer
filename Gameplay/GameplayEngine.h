// File: GameplayEngine/GameplayEngine.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
#pragma once

// Project-specific headers
#include "ActivePlayer.h"   // Defines GameLogic::ActivePlayer
#include "PlayerManager.h"  // Defines GameLogic::PlayerManager
#include "RiftStepLogic.h"  // Defines GameLogic::RiftStepOutcome, GameLogic::ERiftStepType etc.
#include "CombatLogic.h"    // Defines GameLogic::AttackOutcome (Assumed to exist) 

#include "../PhysicsEngine/PhysicsEngine.h" // Defines Physics::PhysicsEngine

// FlatBuffers generated headers (Assuming V0.0.3 is current)
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h" // For C2S::RiftStepDirectionalIntent
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h" // Potentially for constructing S2C messages later
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h"   // For Shared::Vec3 etc.

// Utility headers
#include "../Utils/MathUtil.h"
#include "../Utils/Logger.h"    // For RF_GAMEPLAY_DEBUG, RF_GAMEPLAY_INFO, etc.

namespace RiftForged {
    namespace Gameplay {

        class GameplayEngine {
        public:
            // Constructor injecting essential dependencies
            GameplayEngine(RiftForged::GameLogic::PlayerManager& playerManager,
                RiftForged::Physics::PhysicsEngine& physicsEngine);

            // Initialize player in the physics world
            void InitializePlayerInWorld(
                RiftForged::GameLogic::ActivePlayer* player,
                const RiftForged::Networking::Shared::Vec3& spawn_position,
                const RiftForged::Networking::Shared::Quaternion& spawn_orientation
            );

            // GetPlayerManager
            RiftForged::GameLogic::PlayerManager& GetPlayerManager(); // Or const version if appropriate

            // --- Player Actions ---

            // Handles player orientation changes based on client input
            void TurnPlayer(RiftForged::GameLogic::ActivePlayer* player, float turn_angle_degrees_delta);

            // Processes player movement input and interacts with the PhysicsEngine
            void ProcessMovement(
                RiftForged::GameLogic::ActivePlayer* player,
                const RiftForged::Networking::Shared::Vec3& local_desired_direction_from_client,
                bool is_sprinting,
                float delta_time_sec // Time elapsed for this tick/frame
            );

            // Orchestrates the RiftStep ability for a player
            RiftForged::GameLogic::RiftStepOutcome ExecuteRiftStep(
                RiftForged::GameLogic::ActivePlayer* player,
                RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent
            );

            // Orchestrates a basic attack for a player
            RiftForged::GameLogic::AttackOutcome ExecuteBasicAttack(
                RiftForged::GameLogic::ActivePlayer* attacker,
                const RiftForged::Networking::Shared::Vec3& world_aim_direction,
                uint64_t optional_target_entity_id // 0 if no specific target lock
            );

            // --- Potentially other gameplay logic methods ---
            // void UpdateGameWorld(float delta_time_sec); // Example for world events, NPC AI ticks, etc.          
            // void ApplyStatusEffectToPlayer(GameLogic::ActivePlayer* player, Networking::Shared::StatusEffectCategory effect, uint32_t duration_ms, float magnitude);
            
            // Ability Placeholder Logic
            // SomeAbilityOutcome ExecuteSolarStrike(GameLogic::ActivePlayer* melee, const BasicAttackTargetInfo& target);
            // SomeAbilityOutcome ExecuteSolarFlare(GameLogic::ActivePlayer* caster, const Networking::Shared::Vec3& target_point);
            // SomeAbilityOutcome ExecuteGlacialBolt(GameLogic::ActivePlayer* caster, const Networking::Shared::Vec3& target_point);
            // SomeAbilityOutcome ExecuteGlacialStrike(GameLogic::ActivePlayer* melee, const BasicAttackTargetInfo& target);
            // SomeAbilityOutcome ExecuteVerdantStrike(GameLogic::ActivePlayer* melee, const BasicAttackTargetInfo& target);
            // SomeAbilityOutcome ExecuteVerdantLash(GameLogic::ActivePlayer* caster, const Networking::Shared::Vec3& target_point);
            // SomeAbilityOutcome ExecuteVerdantHealingLink(GameLogic::ActivePlayer* healer, const BasicAttackTargetInfo& target);
            // SomeAbilityOutcome ExecuteVerdantLifegivingBoon(GameLogic::ActivePlayer* healer, const Networking::Shared::Vec3& target_point);
            // SomeAbilityOutcome ExecuteRiftBolt(GameLogic::ActivePlayer* caster, const Networking::Shared::Vec3& target_point);
            // SomeAbilityOutcome ExecuteRiftStrike(GameLogic::ActivePlayer* melee, const BasicAttackTargetInfo& target);

        private:
            RiftForged::GameLogic::PlayerManager& m_playerManager;
            RiftForged::Physics::PhysicsEngine& m_physicsEngine;

            // --- Core Game Constants (Consider moving to a dedicated config/constants file/namespace later) ---

            // RiftStep - Min cooldown is a global rule. Ability ID to key into player's cooldown map.
            // Base distance/cooldown are now per RiftStepDefinition on the ActivePlayer.
            static constexpr float RIFTSTEP_MIN_COOLDOWN_SEC = 0.25f; // Absolute minimum cooldown achievable
            // static constexpr uint32_t RIFTSTEP_ABILITY_ID = 1; // Defined in ActivePlayer.h, ensure consistency or centralize

            // Movement - Speeds in units (e.g., meters) per second
            static constexpr float BASE_WALK_SPEED_MPS = 3.0f;
            static constexpr float SPRINT_SPEED_MULTIPLIER = 1.5f;
            // static constexpr float PLAYER_MAX_TURN_RATE_DPS = 360.0f; // Degrees per second, if turn speed is capped

            // Combat - Ability ID for cooldown map
            // static constexpr uint32_t BASIC_ATTACK_ABILITY_ID = 2; // Example, ensure unique & consistent with ActivePlayer constants
        };

    } // namespace Gameplay
} // namespace RiftForged