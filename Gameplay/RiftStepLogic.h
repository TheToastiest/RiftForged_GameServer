// File: GameplayEngine/RiftStepLogic.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
// Description: Defines the core logic, types, and data structures for the RiftStep ability.
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

// Assuming V0.0.3 generated headers are in this path structure
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"
// C2S include not strictly needed for these definitions, but good for context
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"

namespace RiftForged {
    namespace GameLogic {

        // GameplayEffectInstance struct (defines a specific gameplay effect that can be triggered)
        struct GameplayEffectInstance {
            RiftForged::Networking::UDP::S2C::RiftStepEffectPayload effect_payload_type;
            RiftForged::Networking::Shared::Vec3 center_position; // Using generated Vec3 constructor
            float radius = 0.0f;
            uint32_t duration_ms = 0;
            RiftForged::Networking::Shared::DamageInstance damage; // Uses generated DamageInstance
            RiftForged::Networking::Shared::StunInstance stun;     // Uses generated StunInstance
            RiftForged::Networking::Shared::StatusEffectCategory buff_debuff_to_apply;
            std::string visual_effect_tag;
            std::optional<std::vector<Networking::Shared::StatusEffectCategory>> persistent_area_applied_effects;


            GameplayEffectInstance() :
                effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_NONE),
                center_position(0.0f, 0.0f, 0.0f), // Default constructor for Vec3
                radius(0.0f),
                duration_ms(0),
                // For damage and stun, we rely on their default constructors from generated code
                // and then set members if needed, or use their specific constructors.
                // The generated struct might have a default constructor. If not, direct init needed.
                // Assuming DamageInstance() and StunInstance() exist and default to 0/None.
                // If not, we initialize them directly as shown below.
                buff_debuff_to_apply(RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None),
                visual_effect_tag("") {
                // Explicitly default initialize members of FlatBuffers structs if their default constructors are minimal
                // Using ::DamageType_None as per generated header [cite: 2]
                damage = RiftForged::Networking::Shared::DamageInstance(0, RiftForged::Networking::Shared::DamageType::DamageType_MIN, false);
                // Using ::StunSeverity_None as per generated header [cite: 2]
                stun = RiftForged::Networking::Shared::StunInstance(RiftForged::Networking::Shared::StunSeverity::StunSeverity_MIN, 0);
            }

            // Convenience constructor for an Area Damage effect
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad,
                const RiftForged::Networking::Shared::DamageInstance& dmg_instance)
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload::RiftStepEffectPayload_AreaDamage),
                center_position(center), radius(rad), damage(dmg_instance),
                duration_ms(0),
                buff_debuff_to_apply(RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None),
                visual_effect_tag("") {
                stun = RiftForged::Networking::Shared::StunInstance(RiftForged::Networking::Shared::StunSeverity::StunSeverity_MIN, 0);
            }

            // Convenience constructor for an Area Stun effect
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad,
                const RiftForged::Networking::Shared::StunInstance& stun_instance)
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload::RiftStepEffectPayload_AreaStun),
                center_position(center), radius(rad), stun(stun_instance),
                duration_ms(0),
                buff_debuff_to_apply(RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None),
                visual_effect_tag("") {
                damage = RiftForged::Networking::Shared::DamageInstance(0, RiftForged::Networking::Shared::DamageType::DamageType_MIN, false);
            }

            // Convenience constructor for ApplyBuffDebuff effect (Area of Effect)
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad, uint32_t effect_duration_ms,
                RiftForged::Networking::Shared::StatusEffectCategory effect_to_apply, const std::string& vfx_tag = "")
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload::RiftStepEffectPayload_ApplyBuff),
                center_position(center), radius(rad), duration_ms(effect_duration_ms),
                buff_debuff_to_apply(effect_to_apply),
                visual_effect_tag(vfx_tag) {
                damage = RiftForged::Networking::Shared::DamageInstance(0, RiftForged::Networking::Shared::DamageType::DamageType_MIN, false);
                stun = RiftForged::Networking::Shared::StunInstance(RiftForged::Networking::Shared::StunSeverity::StunSeverity_MIN, 0);
            }

            // Convenience constructor for PersistentArea effect
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad, uint32_t area_duration_ms,
                const std::string& persistent_vfx_tag,
                RiftForged::Networking::Shared::DamageInstance periodic_damage_instance = RiftForged::Networking::Shared::DamageInstance(0, RiftForged::Networking::Shared::DamageType::DamageType_MIN, false),
                RiftForged::Networking::Shared::StatusEffectCategory periodic_effect_to_apply = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None)
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload::RiftStepEffectPayload_PersistentArea),
                center_position(center), radius(rad), duration_ms(area_duration_ms),
                damage(periodic_damage_instance),
                buff_debuff_to_apply(periodic_effect_to_apply),
                visual_effect_tag(persistent_vfx_tag) {
                stun = RiftForged::Networking::Shared::StunInstance(RiftForged::Networking::Shared::StunSeverity::StunSeverity_MIN, 0);
            }
        };

        // Defines the different types/styles of RiftStep abilities
        enum class ERiftStepType : uint8_t {
            None = 0,
            Basic,                          // Standard high-speed/short-distance movement
            SolarExplosionExit,             // SolarWatcher variant
            SolarFlareBlindEntrance,        // SolarWatcher variant
            GlacialFrozenAttackerEntrance,  // Glacial Order variant
            GlacialChilledGroundExit,       // Glacial Order variant
            RootingVinesEntrance,           // Verdant Pact variant
            NatureShieldExit,               // Verdant Pact: Shield + optional minor healing aura [cite: 1]
            Rapid연속이동,                   // Riftborn Covenant: Rapid Consecutive Movement
            StealthEntrance,                // Stealth variant
            GravityWarpEntrance ,            // Advanced: Modifies gravity
            TimeDilationExit,               // Advanced: Modifies time flow
        };

        // Definition/Template for a RiftStep ability's static properties
        struct RiftStepDefinition {
            ERiftStepType type = ERiftStepType::Basic;
            std::string name_tag; // For debugging or UI hints e.g., "Basic RiftStep"

            float max_travel_distance = 15.0f;  // Base maximum travel distance
            float base_cooldown_sec = 1.25f;    // RiftStep is Will-free

            // --- Specific parameters for different ERiftStepTypes ---
            struct SolarExplosionParams {
                RiftForged::Networking::Shared::DamageInstance damage_on_exit{ 0, RiftForged::Networking::Shared::DamageType::DamageType_Radiant, false }; // Example element
                float explosion_radius = 5.0f;
            } solar_explosion_props;

            struct SolarBlindParams {
                RiftForged::Networking::Shared::StatusEffectCategory blind_effect = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_Debuff_AwarenessReduced; // Example, check exact GDD/StatusEffectCategory
                uint32_t blind_duration_ms = 2000;
                float blind_radius = 5.0f;
            } solar_blind_props;

            struct GlacialFreezeParams {
                RiftForged::Networking::Shared::StunInstance freeze_stun_on_entrance{ RiftForged::Networking::Shared::StunSeverity::StunSeverity_Medium, 1500 }; // Example freeze
                float freeze_radius = 3.0f;
            } glacial_freeze_props;

            struct GlacialChilledGroundParams {
                float chilled_ground_radius = 4.0f;
                uint32_t chilled_ground_duration_ms = 5000;
                RiftForged::Networking::Shared::StatusEffectCategory slow_effect = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_Slow_Movement;
                std::string chilled_ground_vfx_tag = "vfx_glacial_chill_ground";
            } glacial_chill_props;

            struct RootingVinesParams {
                RiftForged::Networking::Shared::StatusEffectCategory root_effect = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_Root_Generic;
                uint32_t root_duration_ms = 2500;
                float root_radius = 3.0f;
            } rooting_vines_props;

            // Parameters for NatureShieldExit, combining shield and optional healing [cite: 1]
            struct NaturePactEffectParams {
                bool apply_shield_on_exit = true;
                float shield_percent_of_max_health = 0.05f; // 5% or 7.5% [cite: 1]
                uint32_t shield_duration_ms = 5000;

                bool apply_minor_healing_aura = false; // Can be true for some variants [cite: 1]
                float healing_aura_amount_per_tick = 5.0f; // Example
                uint32_t healing_aura_duration_ms = 3000;  // Example
                uint32_t healing_aura_tick_interval_ms = 1000; // Example
                float healing_aura_radius = 3.0f; // Example AoE
            } nature_pact_props;

            struct RapidConsecutiveParams {
                int max_additional_steps = 1;
                float subsequent_step_cooldown_sec = 0.25f;
                float subsequent_step_distance_multiplier = 0.75f;
                uint32_t activation_window_ms = 1000;
            } rapid_consecutive_props;

            struct StealthParams {
                uint32_t stealth_duration_ms = 3000;
                RiftForged::Networking::Shared::StatusEffectCategory stealth_buff_category = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_Buff_Stealth;
            } stealth_props;

            // Default VFX/SFX (can be overridden by specific definitions)
            std::string default_start_vfx_id;
            std::string default_travel_vfx_id;
            std::string default_end_vfx_id;

            RiftStepDefinition() { // Default constructor to initialize all members
                type = ERiftStepType::None;
                name_tag = "Uninitialized RiftStep";
                max_travel_distance = 0.0f;
                base_cooldown_sec = 999.0f;
                // Initialize all nested structs to their defaults
                solar_explosion_props = {};
                solar_blind_props = {};
                glacial_freeze_props = {};
                glacial_chill_props = {};
                rooting_vines_props = {};
                nature_pact_props = {};
                rapid_consecutive_props = {};
                stealth_props = {};
                default_start_vfx_id = "";
                default_travel_vfx_id = "";
                default_end_vfx_id = "";
            }

            // Static factory method for the Basic RiftStep
            static RiftStepDefinition CreateBasicRiftStep() {
                RiftStepDefinition def;
                def.type = ERiftStepType::Basic;
                def.name_tag = "Basic RiftStep";
                def.max_travel_distance = 15.0f;    // Standard distance
                def.base_cooldown_sec = 1.25f;      // Standard cooldown
                def.default_start_vfx_id = "vfx_riftstep_basic_start";
                def.default_travel_vfx_id = "vfx_riftstep_basic_travel";
                def.default_end_vfx_id = "vfx_riftstep_basic_end";
                // Note: Basic RiftStep doesn't utilize the specific effect parameter structs like solar_explosion_props.
                // They remain default-initialized (e.g., damage_on_exit amount would be 0).
                return def;
            }

            // TODO: Add more static factory methods or a data-driven way to load these definitions
            // for each ERiftStepType (e.g., CreateSolarExplosionExitStep(), CreateNatureShieldStep())
            // These factory methods would populate the relevant specific param structs.
        };

        // Defines the outcome of a RiftStep attempt
        struct RiftStepOutcome {
            bool success = false;
            std::string failure_reason_code; // e.g., "ON_COOLDOWN", "INVALID_TARGET", "OBSTRUCTED"
            ERiftStepType type_executed = ERiftStepType::None;

            uint64_t instigator_entity_id = 0; // <<< ADDED/CONFIRMED THIS MEMBER

            RiftForged::Networking::Shared::Vec3 actual_start_position{ 0.0f, 0.0f, 0.0f };
            RiftForged::Networking::Shared::Vec3 intended_target_position{ 0.0f, 0.0f, 0.0f };
            RiftForged::Networking::Shared::Vec3 calculated_target_position{ 0.0f, 0.0f, 0.0f };
            RiftForged::Networking::Shared::Vec3 actual_final_position{ 0.0f, 0.0f, 0.0f };

            float travel_duration_sec = 0.05f; // Client-side cosmetic, very short for "high speed" feel

            std::vector<GameplayEffectInstance> entry_effects_data;
            std::vector<GameplayEffectInstance> exit_effects_data;

            std::string start_vfx_id;
            std::string travel_vfx_id;
            std::string end_vfx_id;
            // std::string start_sfx_id;
            // std::string travel_sfx_id;
            // std::string end_sfx_id;
        };

    } // namespace GameLogic
} // namespace RiftForged