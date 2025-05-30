// File: GameplayEngine/ActivePlayer.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <cmath>
#include <algorithm>

#include "RiftStepLogic.h" // For RiftStepDefinition, RiftStepOutcome, ERiftStepType

// FlatBuffers generated headers (V0.0.3)
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h"
// #include "../FlatBuffers/V0.0.3/riftforged_item_definitions_generated.h" // Include if player directly interacts with full item defs
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h" // For C2S::RiftStepDirectionalIntent

// Project utilities
#include "../NetworkEngine/NetworkEndpoint.h"
#include "../Utils/Logger.h"
#include "../Utils/MathUtil.h" // For NormalizeQuaternion (used in inline SetOrientation if kept inline)

namespace RiftForged {
    namespace GameLogic {

        enum class PlayerMovementState : uint8_t {
            Idle = 0, Walking, Sprinting, Stunned, Rooted, Dead, Ability_In_Use
        };

        enum class EquippedWeaponCategory : uint32_t {
            Unarmed = 0, Generic_Melee_Sword = 1, Generic_Melee_Axe = 2, Generic_Melee_Maul = 3,
            Generic_Ranged_Bow = 101, Generic_Ranged_Gun = 102,
            Generic_Magic_Staff = 201, Generic_Magic_Wand = 202
        };

        // Unique Ability ID for RiftStep, used as a key in the cooldown map.
        const uint32_t RIFTSTEP_ABILITY_ID = 1;
        // Unique Ability ID for Basic Attack
        const uint32_t BASIC_ATTACK_ABILITY_ID = 2;


        struct ActivePlayer {
            // --- Identifiers ---
            uint64_t playerId;
            RiftForged::Networking::NetworkEndpoint networkEndpoint;

            // --- Core Transform State ---
            RiftForged::Networking::Shared::Vec3 position;
            RiftForged::Networking::Shared::Quaternion orientation;

            // --- Player Physical Properties ---
            float capsule_radius;
            float capsule_half_height;

            // --- Core Resources & Stats ---
            int currentWill;
            int maxWill;
            int currentHealth;
            int maxHealth;

            // --- Offensive Stats (Base values) ---
            float base_ability_cooldown_modifier; // Multiplier, e.g., 1.0 for no change
            float base_critical_hit_chance_percent;
            float base_critical_hit_damage_multiplier;
            float base_accuracy_rating_percent;
            float base_basic_attack_cooldown_sec;

            // --- Defensive Stats (Base values) ---
            int32_t flat_physical_damage_reduction; float percent_physical_damage_reduction;
            int32_t flat_radiant_damage_reduction;  float percent_radiant_damage_reduction;
            int32_t flat_frost_damage_reduction;    float percent_frost_damage_reduction;
            int32_t flat_shock_damage_reduction;    float percent_shock_damage_reduction;
            int32_t flat_necrotic_damage_reduction; float percent_necrotic_damage_reduction;
            int32_t flat_void_damage_reduction;     float percent_void_damage_reduction;
            int32_t flat_cosmic_damage_reduction;   float percent_cosmic_damage_reduction;
            int32_t flat_poison_damage_reduction;   float percent_poison_damage_reduction;
            int32_t flat_nature_damage_reduction;   float percent_nature_damage_reduction;
            int32_t flat_aetherial_damage_reduction;float percent_aetherial_damage_reduction;

            // --- Gameplay Modifiers & States ---
            RiftStepDefinition current_rift_step_definition;

            EquippedWeaponCategory current_weapon_category;
            uint32_t equipped_weapon_definition_id; // Links to ItemStaticData.definition_id

            std::vector<RiftForged::Networking::Shared::StatusEffectCategory> activeStatusEffects;
            uint32_t animationStateId; // Uses RiftForged::Networking::Shared::AnimationState
            PlayerMovementState movementState;

            std::map<uint32_t, std::chrono::steady_clock::time_point> abilityCooldowns;
            std::atomic<bool> isDirty;

            // --- Constructor ---
            ActivePlayer(uint64_t pId = 0,
                const RiftForged::Networking::NetworkEndpoint& ep = {},
                const RiftForged::Networking::Shared::Vec3& startPos = RiftForged::Networking::Shared::Vec3(0.0f, 0.0f, 0.0f),
                const RiftForged::Networking::Shared::Quaternion& startOrientation = RiftForged::Networking::Shared::Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                float cap_radius = 0.5f, float cap_half_height = 1.8f
            );

            // --- State Accessors & Modifiers (Declarations) ---
            // Simple setters might remain inline if preferred, complex ones in .cpp
            void SetPosition(const RiftForged::Networking::Shared::Vec3& newPosition);
            void SetOrientation(const RiftForged::Networking::Shared::Quaternion& newOrientation);

            void SetWill(int value);
            void DeductWill(int amount);
            void AddWill(int amount); // Added for completeness

            void SetHealth(int value);
            void TakeDamage(int raw_damage_amount, RiftForged::Networking::Shared::DamageType damage_type);
            void HealDamage(int amount); // Added for completeness

            void SetAnimationState(RiftForged::Networking::Shared::AnimationState newState);
            void SetMovementState(PlayerMovementState newState);

            bool IsAbilityOnCooldown(uint32_t abilityId) const;
            void SetAbilityCooldown(uint32_t abilityId, float duration_sec); // Takes float seconds

            void UpdateActiveRiftStepDefinition(const RiftStepDefinition& new_definition);
            bool CanPerformRiftStep() const;
            RiftStepOutcome PrepareRiftStepOutcome(RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent directional_intent);

            void AddStatusEffects(const std::vector<RiftForged::Networking::Shared::StatusEffectCategory>& effects_to_add);
            void RemoveStatusEffects(const std::vector<RiftForged::Networking::Shared::StatusEffectCategory>& effects_to_remove);

            void SetEquippedWeapon(uint32_t weapon_def_id, EquippedWeaponCategory category);
            RiftForged::Networking::Shared::Vec3 GetMuzzlePosition() const; // For ranged attacks
        };

    } // namespace GameLogic
} // namespace RiftForged