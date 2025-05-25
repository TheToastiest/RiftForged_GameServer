// File: Gameplay/ItemStaticData.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
// Description: Defines the static properties for all item definitions in RiftForged.
#pragma once

#include "ItemType.h" // Includes our EItemType enum from Gameplay/ItemType.h

// Assuming riftforged_common_types.fbs (and its generated C++ header) is available
// and will define Shared::DamageType, Shared::StatusEffectCategory etc.
// Update this include path to where your generated FlatBuffers C++ headers reside.
#include "../FlatBuffers/v0.0.3/riftforged_common_types_generated.h"
#include "../FlatBuffers/v0.0.3/riftforged_s2c_udp_messages_generated.h" // For S2C::RiftStepEffectPayload enum
// Note: The above include paths are examples. Adjust them to your project structure.

// ^^ Note: I've assumed a typical generation path "FlatBuffers/Namespace1/Namespace2/file_generated.h"
// Adjust if your output structure is different.

#include <string>
#include <vector> // Included for completeness, though not used in these structs directly yet
#include <cstdint>

namespace RiftForged {
    namespace GameLogic {
        namespace Items {

            // Item Rarity Tiers (as defined by user for v0.0.3+)
            enum class EItemRarity : uint8_t {
                NORMAL,
                SHARP,
                MAGIC,
                RARE,       // Craftable through Vision Level
                LEGENDARY,  // Raid drops
                EPIC,       // End-game crafting from found recipes
                MYTHIC,     // End-game crafting, world-known recipe, 1 craft per server
                META        // Uses Mythic items, game-changing, class tree altering
            };

            // Equipment Slots (accessories deferred for post-Alpha/Beta)
            enum class EEquipmentSlot : uint8_t {
                NONE,
                HEAD,
                CHEST,
                HANDS,
                FEET,
                BELT,
                MAIN_HAND,
                OFF_HAND,   // Can be shield or dual-wielded weapon
                // AMULET,  // Deferred
                // RING_1,  // Deferred
                // RING_2,  // Deferred
            };

            // Material Tiers (Refined by user)
            enum class EItemMaterialTier : uint8_t {
                NONE,
                TIER_0_BASE,        // Basic starting/crafted
                TIER_1,             // e.g., Copper equivalent
                TIER_2,             // e.g., Iron equivalent
                TIER_3,             // e.g., Steel equivalent
                TIER_4,             // e.g., Mithril equivalent
                TIER_LEGENDARY_BASE,
                TIER_EPIC_BASE,
                TIER_MYTHIC_BASE,
                TIER_ARTIFACT,
                TIER_TRANSCENDENT,
                TIER_META_BASE
            };

            // --- Type-Specific Property Struct Definitions ---

            struct ArmorStaticProps {
                float base_defense = 0.0f;
                float will_regeneration_penalty_percent = 0.0f; // For Body Armor (L/M/H)
                float movement_speed_reduction_percent = 0.0f;  // For Shields
                float block_damage_reduction_percent = 0.0f;    // For Shields
                float block_stamina_cost = 0.0f;                // For Shields (if applicable)
                ArmorStaticProps() = default;
            };

            struct WeaponStaticProps {
                float base_physical_damage_min = 0.0f;
                float base_physical_damage_max = 0.0f;
                float attack_speed = 1.0f;
                float range_meters = 1.5f;
                float critical_hit_chance_base_percent = 5.0f;
                float critical_hit_damage_base_multiplier = 2.0f; //
                bool is_gildable = true;
                WeaponStaticProps() { // Sensible defaults for a basic weapon
                    base_physical_damage_min = 5.0f; base_physical_damage_max = 10.0f;
                    attack_speed = 1.0f; range_meters = 1.5f;
                    critical_hit_chance_base_percent = 5.0f; critical_hit_damage_base_multiplier = 2.0f;
                    is_gildable = true;
                }
            };

            struct ConsumableStaticProps {
                enum class EEffectType : uint8_t { NONE, RESTORE_HEALTH, RESTORE_WILL, APPLY_STATUS_EFFECT };
                EEffectType effect_type = EEffectType::NONE;
                float restore_amount_flat = 0.0f;
                float restore_amount_percent_max = 0.0f;
                RiftForged::Networking::Shared::StatusEffectCategory status_effect_to_apply = RiftForged::Networking::Shared::StatusEffectCategory::StatusEffectCategory_None; // From common_types
                uint32_t effect_duration_ms = 0;
                float usage_cooldown_sec = 1.0f;
                ConsumableStaticProps() = default;
            };

            struct GildKitStaticProps {
                RiftForged::Networking::Shared::DamageType element_type = RiftForged::Networking::Shared::DamageType::DamageType_Radiant; // Example default, ensure DamageType_None if possible
                enum class EGildTarget : uint8_t { WEAPON, ARMOR } target_type = EGildTarget::WEAPON;
                uint8_t gild_tier = 1; // Determines % elemental damage (weapons) or % resistance (armor)
                GildKitStaticProps() = default;
            };

            struct StarMapPointUpgradeStaticProps {
                uint32_t points_granted = 0; // How many Star Map points this item grants
                StarMapPointUpgradeStaticProps() = default;
            };

            struct ClassUnlockTokenStaticProps {
                std::string unlocks_target_id; // e.g., "SPEC_ASTRAL", "META_CLASS_DRAGONHEART_PATH"
                ClassUnlockTokenStaticProps() = default;
            };

            // AccessoryStaticProps would be defined here when Artificers are implemented
            // struct AccessoryStaticProps { /* ... */ };


            // --- Main Item Static Data Struct ---
            struct ItemStaticData {
                // --- Basic Common Properties ---
                uint32_t definition_id = 0;         // Unique ID for this item definition
                EItemType item_type = EItemType::NONE; // From Gameplay/ItemType.h
                std::string dev_name_tag;           // Internal unique development name (e.g., "wpn_sword_copper_t1")
                std::string display_name_sid;       // String ID for localization
                std::string description_sid;        // String ID for localization
                EItemRarity rarity = EItemRarity::NORMAL;
                uint32_t max_stack_size = 1;
                bool is_unique = false;
                bool is_quest_item = false;
                bool can_be_sold_to_vendor = true;
                uint32_t vendor_sell_price_shimmer = 0;
                uint32_t vendor_buy_price_shimmer = 0;
                bool is_tradable = true;            // Default true, set to false for basic elemental materials
                std::string icon_resource_id;
                std::string model_resource_id;

                // --- Equippable Properties ---
                EEquipmentSlot equip_slot = EEquipmentSlot::NONE;
                uint32_t required_level = 1;

                // --- Material Tier & Durability ---
                EItemMaterialTier material_tier = EItemMaterialTier::NONE;
                uint32_t base_durability = 0; // Placeholder, 0 means no durability system active

                // --- Type-Specific Property Blocks ---
                // Only one of these will be relevant for any given item_definition_id.
                // The game logic will check item_type to know which props block to access.
                WeaponStaticProps weapon_props;
                ArmorStaticProps armor_props;
                ConsumableStaticProps consumable_props;
                GildKitStaticProps gild_kit_props;
                StarMapPointUpgradeStaticProps star_map_point_props;
                ClassUnlockTokenStaticProps class_unlock_props;
                // AccessoryStaticProps accessory_props; // When implemented

                ItemStaticData() = default;
            };

        } // namespace Items
    } // namespace GameLogic
} // namespace RiftForged