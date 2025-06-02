// File: GameplayEngine/AbilityLogic.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
// Description: Defines structures for active ability definitions and related logic.
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map> // If needed for complex variant data, though we're using distinct definitions for Option A

// Assuming ItemType.h is where EItemType (for weapon context) is defined
// Adjust path if EItemType is namespaced differently or in another file accessible here.
#include "ItemType.h" // For Items::EItemType 

// Assuming RiftStepLogic.h is where GameplayEffectInstance is defined
// This allows abilities to reuse the same effect structure as RiftStep outcomes.
#include "RiftStepLogic.h" // For GameLogic::GameplayEffectInstance

// Include common FlatBuffer types if directly used by definitions (e.g. StatusEffectCategory for direct application)
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h" // For Shared::StatusEffectCategory etc.


namespace RiftForged {
    namespace GameLogic {
        // Forward declare from ItemType.h if not directly including or for namespacing
        // namespace Items { enum class EItemType : uint16_t; }

        // Category of the ability for classification, UI, or game logic
        enum class EAbilityCategory : uint8_t {
            None,
            Damage_Direct,          // Deals immediate damage
            Damage_DoT,             // Applies a damage-over-time effect
            Heal_Direct,            // Provides immediate healing
            Heal_HoT,               // Applies a heal-over-time effect
            Buff_Self,              // Applies a beneficial status effect to the caster
            Buff_Target_Ally,       // Applies a beneficial status effect to an ally
            Debuff_Target_Enemy,    // Applies a detrimental status effect to an enemy
            Control_Hard,           // e.g., Stun, Freeze, Root (prevents actions/movement)
            Control_Soft,           // e.g., Slow, Blind (hinders but doesn't fully prevent)
            Shield_Self,            // Applies a damage absorption shield to self
            Shield_Target_Ally,     // Applies a shield to an ally
            Shield_AoE_Ally,        // Applies a shield to allies in an area
            Mobility,               // Primarily for movement (e.g., a dash, blink - distinct from core RiftStep)
            Utility,                // Other non-combat or specialized effects
            Summon                  // Summons a pet or temporary entity
        };

        // How the ability targets entities or locations
        enum class EAbilityTargetType : uint8_t {
            None,
            Self,                   // Affects only the caster
            Single_Enemy,           // Requires a single enemy target
            Single_Ally,            // Requires a single friendly target (excluding self unless specified)
            Single_Ally_Or_Self,    // Can target an ally or the caster
            Ground_AoE,             // Player targets a point on the ground for an Area of Effect
            Point_Blank_AoE_Self,   // AoE centered on and originating from the caster
            Cone_AoE_Caster_Forward,// AoE in a cone in front of the caster
            Projectile_Single_Target_Enemy, // Fires a projectile that affects the first enemy hit
            Projectile_Ground_AoE,  // Fires a projectile that creates an AoE on impact with ground/target
            // Chain_Multiple_Enemies // For abilities like Rift Jolt - primary target, then chains
        };

        // Parameters for Area of Effect abilities
        struct AoEParams {
            float radius_meters = 0.0f;
            float angle_degrees = 0.0f;  // For cone AoEs (e.g., 90 for a 90-degree cone)
            // Potentially: enum class AoEShape { Circle, Cone, Box_Forward };
            // AoEShape shape = AoEShape::Circle;
            uint32_t max_targets = 0; // 0 means unlimited targets within the AoE

            AoEParams() = default;
            AoEParams(float r, float angle = 0.0f, uint32_t max_t = 0) : radius_meters(r), angle_degrees(angle), max_targets(max_t) {}
        };

        // Structure to define a single active ability's static data
        // Each "weapon-variable" version of an ability will have its own ActiveAbilityDefinition.
        struct ActiveAbilityDefinition {
            uint32_t ability_id = 0;            // Unique ID for this specific ability (and its weapon variant)
            std::string dev_name_tag;           // e.g., "SolarBurst_Sword", "UnstableStrike_Gun"

            std::string display_name_sid;       // SID for localization
            std::string description_sid;        // SID for localization
            std::string icon_id;                // Resource ID for UI icon

            EAbilityCategory category = EAbilityCategory::None;
            EAbilityTargetType target_type = EAbilityTargetType::None;

            float cast_time_sec = 0.0f;         // 0 for instant cast (GCD might still apply)
            float cooldown_sec = 1.5f;          // Base cooldown
            int32_t will_cost = 0;              // Resource cost

            float range_meters = 0.0f;          // Max range for targeting or projectile launch
            AoEParams aoe_params;               // Details if it's an AoE (radius, angle, max_targets)

            // Effects this ability applies.
            // These are applied based on the ability's targeting and successful execution.
            // For a projectile, these effects are typically what the projectile applies on hit.
            std::vector<GameplayEffectInstance> effects_on_primary_target; // For single target abilities or main target of AoE
            std::vector<GameplayEffectInstance> effects_on_caster_self;    // For self-buffs or self-applied effects
            std::vector<GameplayEffectInstance> effects_on_aoe_secondary_targets; // For other targets in an AoE
            std::vector<GameplayEffectInstance> effects_at_target_location; // For ground-targeted AoEs that create persistent areas or ground effects

            // For weapon-variable abilities (Option A: distinct definitions)
            uint32_t conceptual_ability_group_id = 0; // All variants of "Solar Burst" share this ID
            // This ID is what player "learns" at a class level.
            Items::EItemType weapon_context_type = Items::EItemType::NONE; // e.g., WEAPON_SWORD, WEAPON_BOW, WEAPON_STAFF
            // Or a broader category like EWeaponArchetype (Melee, RangedPhysical, Magic)
            // if variations are per archetype rather than specific EItemType.
            // For now, EItemType provides max specificity.

// Animation & VFX - Tags for the client to use
            std::string animation_tag_caster;       // e.g., "Cast_SolarBurst_Sword"
            std::string vfx_caster_self_tag;        // e.g., "VFX_Casting_Solar_Energy" (on self during cast)
            std::string vfx_projectile_tag;         // If it spawns a unique projectile (e.g., "VFX_Projectile_SolarArrow")
            std::string vfx_target_impact_tag;      // e.g., "VFX_Impact_SolarBurst_Hit"
            std::string vfx_aoe_area_tag;           // For the visual of the AoE itself (e.g., "VFX_AoE_SolarFlare_Ground")

            // Sound Effects - Tags
            std::string sfx_cast_tag;
            std::string sfx_projectile_travel_tag;  // If applicable
            std::string sfx_impact_tag;

            // Requirements
            uint8_t required_class_level = 1;       // Class level at which this conceptual ability is learned
            // uint32_t required_class_id_for_variant; // e.g., SOLAR_CLASS_ID, GLACIAL_CLASS_ID, if definitions are global
                                                    // Or implied if definitions are stored per-class.

            // Flags
            bool requires_line_of_sight = true;
            bool can_be_cast_while_moving = true;    // For most instant casts
            // bool consumes_basic_attack_cooldown = false; // If using this ability also triggers basic attack CD

            // Projectile specific (if this ability definition IS a projectile's properties, or launches one)
            float projectile_speed_mps = 0.0f; // If this ability directly defines a projectile it launches
            // bool projectile_pierces_targets = false;
            // uint32_t projectile_max_pierces = 0;

            ActiveAbilityDefinition() = default;
        };

    } // namespace GameLogic
} // namespace RiftForged