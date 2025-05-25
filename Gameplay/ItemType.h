// File: Gameplay/ItemType.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
// Description: Defines the primary types for all items in RiftForged.
#pragma once

#include <cstdint>

namespace RiftForged {
    namespace GameLogic {
        namespace Items {

            // Finalized Item Type Enumeration for v0.0.3
            enum class EItemType : uint16_t {
                NONE = 0,

                // == CURRENCY ==
                CURRENCY_SHIMMER,                           // Primary in-game currency

                // == WEAPONS (Simplified Base Types for 0.0.3) ==
                // Handedness: Sword/Axe = 1H default, Maul = 2H default for these base types.
                // Guns are explicitly 1H/2H. Other properties in ItemStaticData.
                WEAPON_SWORD,                               // Default 1H Physical Damage
                WEAPON_AXE,                                 // Default 1H Physical Damage
                WEAPON_MAUL,                                // Default 2H Physical Damage
                WEAPON_BOW,                                 // Physical Damage
                WEAPON_GUN_1H,                              // Physical Damage
                WEAPON_GUN_2H,                              // Physical Damage
                WEAPON_WAND,                                // Physical Damage (or very low base magic if wands shoot projectiles)
                WEAPON_STAFF,                               // Physical Damage (or very low base magic if staves shoot projectiles)

                // == ARMOR (Categorized by Light/Medium/Heavy affecting Will Regen) ==
                // -- Head --
                ARMOR_HEAD_LIGHT,                           // Low defense, no Will penalty
                ARMOR_HEAD_MEDIUM,                          // Medium defense, 10% Will penalty
                ARMOR_HEAD_HEAVY,                           // High defense, 20% Will penalty
                // -- Chest --
                ARMOR_CHEST_LIGHT,
                ARMOR_CHEST_MEDIUM,
                ARMOR_CHEST_HEAVY,
                // -- Hands --
                ARMOR_HANDS_LIGHT,
                ARMOR_HANDS_MEDIUM,
                ARMOR_HANDS_HEAVY,
                // -- Feet --
                ARMOR_FEET_LIGHT,
                ARMOR_FEET_MEDIUM,
                ARMOR_FEET_HEAVY,
                // -- Belt (Also follows L/M/H for Will penalty if it grants significant defenses) --
                ARMOR_BELT_LIGHT,
                ARMOR_BELT_MEDIUM,
                ARMOR_BELT_HEAVY,
                // -- Shield (Assumed not to directly apply L/M/H Will Regen Penalties for now) --
                ARMOR_SHIELD_LIGHT,                         // Lower defense/block stats, potentially less stamina use/movement penalty
                ARMOR_SHIELD_HEAVY,                         // Higher defense/block stats, potentially more stamina use/movement penalty

                // == ACCESSORIES (Crafted by Artificers, provide passive enhancements) ==
                ACCESSORY_AMULET,
                ACCESSORY_RING,
                // ACCESSORY_TRINKET, // Can be added if a third generic accessory slot is planned

                // == CONSUMABLES (Craftable by all players via Intrinsic Alchemy) ==
                CONSUMABLE_POTION,                          // Specific effect (Health, Will, Stat) via ItemStaticData
                CONSUMABLE_ELIXIR,                          // Typically longer-term buffs or unique effects
                CONSUMABLE_FOOD,                            // Buffs, regeneration
                CONSUMABLE_RIFT_TONIC,                      // Unique Rift-related consumables
                CONSUMABLE_SCROLL,                          // Utility or combat effect scrolls

                // == CRAFTING MATERIALS ==
                // Elemental resources collected by players
                MATERIAL_ELEMENTAL_AIR_SOLIDIFIED,
                MATERIAL_ELEMENTAL_WATER_SOLIDIFIED,
                MATERIAL_ELEMENTAL_FIRE_SOLIDIFIED,
                MATERIAL_ELEMENTAL_EARTH_SOLIDIFIED,

                // Metal bars crafted from elemental resources; tier defined by specific item ID/StaticData
                MATERIAL_METAL_BAR,

                MATERIAL_WOOD,                              // For bows, staves, handles, etc.
                MATERIAL_LEATHER,                           // For light/medium armors, grips, etc.
                MATERIAL_CLOTH,                             // For light armors, robes, etc.

                // Tiered "Dust" primarily for Gilding Components
                MATERIAL_DUST_GILDING_EARLY_GAME,
                MATERIAL_DUST_GILDING_MID_GAME,
                MATERIAL_DUST_GILDING_LATE_GAME,
                MATERIAL_DUST_GILDING_END_GAME,

                // Tiered "General Essences" for broader Class Crafting recipes (Option C)
                MATERIAL_ESSENCE_CLASS_CRAFTING_GENERAL_EARLY_GAME,
                MATERIAL_ESSENCE_CLASS_CRAFTING_GENERAL_MID_GAME,
                MATERIAL_ESSENCE_CLASS_CRAFTING_GENERAL_LATE_GAME,
                MATERIAL_ESSENCE_CLASS_CRAFTING_GENERAL_END_GAME,
                // Specific, named Essences for unique/endgame Meta Classes would be added here later if needed.

                MATERIAL_COMPONENT_GEM,                     // Raw/cut gems for stats, recipes, Artificer accessory crafting
                MATERIAL_COMPONENT_SPECIAL,                 // Unique parts (monster drops, boss items) for high-tier crafts

                // == SPECIALIZED CRAFTED ITEMS ==
                // Gilding Kits crafted by Weapon/Armor Crafters from Dust & other components
                ITEM_GILD_KIT_ELEMENTAL,                    // Specific element (Fire, Water etc.) defined by ItemStaticData

                // Star Map Tree Point Items (Craftable by All Players for Resonance System)
                // Each +N item (e.g., "+1 Points", "+2 Points") is a distinct definition using this type.
                // ItemStaticData specifies points granted & its role in the chained crafting recipe.
                ITEM_STAR_MAP_POINT_UPGRADE,

                // == TOKENS / MISCELLANEOUS ==
                // For crafted class breakthroughs (craftable by all). Specific token via ItemStaticData.
                TOKEN_CLASS_UNLOCK,
                TOKEN_FACTION_REPUTATION,
                ITEM_QUEST,                                 // Items specifically for quests, often unique
                ITEM_KEY,                                   // For doors, chests, specific areas
                ITEM_RECIPE,                                // Teaches a player a new crafting recipe
                ITEM_JUNK,                                  // Low value, primarily for selling to vendors
            };
        } // namespace Items
    } // namespace GameLogic
} // namespace RiftForged