// File: Gameplay/ActivePlayer.h (Refactored)
// RiftForged Game Development Team
// Purpose: Defines the ActivePlayer class, representing a connected player's
//          state and capabilities within a game world instance.

#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <map>
#include <cstdint>
#include <mutex>      // For m_internalDataMutex
#include <algorithm>  // For std::max/min
#include <numeric>    // For std::accumulate (example in TakeDamage)

// Project-specific Networking & FlatBuffer Types (for data structures, not direct networking)
// NetworkEndpoint.h is NO LONGER included here.
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h" // For Vec3, Quaternion, AnimationState, StatusEffectCategory, DamageType
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S::RiftStepDirectionalIntent (as parameter type)

// Project-specific Game Logic Types
#include "RiftStepLogic.h"  // For GameLogic::RiftStepOutcome, ERiftStepType, RiftStepDefinition

// Utilities
#include "../Utils/MathUtil.h" // For potential math operations
#include "../Utils/Logger.h"   // For logging macros

namespace RiftForged {
    namespace GameLogic {

        // Represents the current movement state of the player.
        enum class PlayerMovementState : uint8_t {
            Idle, Walking, Sprinting, Rifting, Ability_In_Use, Stunned, Rooted, Dead
        };

        // Represents the category of weapon the player has equipped.
        enum class EquippedWeaponCategory : uint8_t {
            Unarmed, Generic_Melee_Sword, Generic_Melee_Axe, Generic_Melee_Maul,
            Generic_Ranged_Bow, Generic_Ranged_Gun, Generic_Magic_Staff, Generic_Magic_Wand
        };

        // Ability IDs (ensure these are consistent across your game data)
        const uint32_t RIFTSTEP_ABILITY_ID = 1;
        const uint32_t BASIC_ATTACK_ABILITY_ID = 2;
        // ... other ability IDs ...

        struct ActivePlayer {
            // --- Core Identifiers ---
            uint64_t playerId;
            // Networking::NetworkEndpoint networkEndpoint; // <<< REMOVED

			std::string characterName; // Player's character name, used for display and identification
            // --- Transform State ---
            Networking::Shared::Vec3 position;
            Networking::Shared::Quaternion orientation;

            // --- Physics Properties ---
            float capsule_radius;
            float capsule_half_height;

            // --- Core Stats ---
            int32_t currentHealth;
            int32_t maxHealth;
            int32_t currentWill;
            uint32_t maxWill; // Consider int32_t for consistency if buffs can temporarily exceed, then clamp.

            // --- Combat Stats & Resistances ---
            float base_ability_cooldown_modifier;     // e.g., 1.0 for no change, 0.8 for 20% faster cooldowns
            float base_critical_hit_chance_percent;   // e.g., 5.0 for 5%
            float base_critical_hit_damage_multiplier;// e.g., 1.5 for +50% damage
            float base_accuracy_rating_percent;       // e.g., 90.0 for 90%
            float base_basic_attack_cooldown_sec;

            int32_t flat_physical_damage_reduction;    float percent_physical_damage_reduction;
            int32_t flat_radiant_damage_reduction;     float percent_radiant_damage_reduction;
            int32_t flat_frost_damage_reduction;       float percent_frost_damage_reduction;
            int32_t flat_shock_damage_reduction;       float percent_shock_damage_reduction;
            int32_t flat_necrotic_damage_reduction;    float percent_necrotic_damage_reduction;
            int32_t flat_void_damage_reduction;        float percent_void_damage_reduction;
            int32_t flat_cosmic_damage_reduction;      float percent_cosmic_damage_reduction;
            int32_t flat_poison_damage_reduction;      float percent_poison_damage_reduction;
            int32_t flat_nature_damage_reduction;      float percent_nature_damage_reduction;
            int32_t flat_aetherial_damage_reduction;   float percent_aetherial_damage_reduction;

            // --- Equipment & Abilities ---
            EquippedWeaponCategory current_weapon_category;
            uint32_t equipped_weapon_definition_id;
            RiftStepDefinition current_rift_step_definition;
            std::map<uint32_t, std::chrono::steady_clock::time_point> abilityCooldowns;

            // --- State Flags and Info ---
            PlayerMovementState movementState;
            uint32_t animationStateId; // Corresponds to Networking::Shared::AnimationState or a game-specific enum
            std::vector<Networking::Shared::StatusEffectCategory> activeStatusEffects; // Effects applied
            std::atomic<bool> isDirty; // Flag for state synchronization

            // --- Input Intentions (updated by GameplayEngine based on processed commands) ---
            Networking::Shared::Vec3 last_processed_movement_intent; // Normalized direction vector or magnitude
            bool was_sprint_intended;

            // --- Synchronization ---
            mutable std::mutex m_internalDataMutex; // Protects members like abilityCooldowns, activeStatusEffects if accessed/modified by multiple systems concurrently (less likely if GameplayEngine is single-threaded for player logic)

            // --- Constructor ---
            ActivePlayer(uint64_t pId,
                const Networking::Shared::Vec3& startPos = { 0.f, 0.f, 1.f },
                const Networking::Shared::Quaternion& startOrientation = { 0.f, 0.f, 0.f, 1.f },
                float cap_radius = 0.5f, float cap_half_height = 0.9f);

            // --- Methods ---
            // Note: Setters that change game state relevant for clients should set isDirty = true;

            void SetPosition(const Networking::Shared::Vec3& newPosition);
            void SetOrientation(const Networking::Shared::Quaternion& newOrientation);

            void SetWill(int32_t value); // Changed to int32_t for consistency
            void DeductWill(int32_t amount);
            void AddWill(int32_t amount);

            void SetHealth(int32_t value);
            void HealDamage(int32_t amount);
            // Returns actual damage taken after reductions
            int32_t TakeDamage(int32_t raw_damage_amount, Networking::Shared::DamageType damage_type);

            void SetAnimationState(Networking::Shared::AnimationState newState);
            void SetAnimationStateId(uint32_t newStateId); // Use this if AnimationState enum isn't directly used
            void SetMovementState(PlayerMovementState newState);

            bool IsAbilityOnCooldown(uint32_t abilityId) const;
            void StartAbilityCooldown(uint32_t abilityId, float base_duration_sec); // Takes base, applies modifiers
			void SetAbilityCooldown(uint32_t abilityId, float cooldown_sec) {
				StartAbilityCooldown(abilityId, cooldown_sec);
			} // Convenience method

            void UpdateActiveRiftStepDefinition(const RiftStepDefinition& new_definition);
            bool CanPerformRiftStep() const; // Check against current_rift_step_definition and cooldowns/resources
      
            RiftStepOutcome PrepareRiftStepOutcome(Networking::UDP::C2S::RiftStepDirectionalIntent directional_intent, ERiftStepType type_requested);

            void AddStatusEffects(const std::vector<Networking::Shared::StatusEffectCategory>& effects_to_add);
            void RemoveStatusEffects(const std::vector<Networking::Shared::StatusEffectCategory>& effects_to_remove);
            bool HasStatusEffect(Networking::Shared::StatusEffectCategory effect) const;

            void SetEquippedWeapon(uint32_t weapon_def_id, EquippedWeaponCategory category);
            Networking::Shared::Vec3 GetMuzzlePosition() const; // Example utility, might need more context

            // Helper to mark dirty
            void MarkDirty();
        };

    } // namespace GameLogic
} // namespace RiftForged