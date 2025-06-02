// File: GameplayEngine/GameplayStubs.h
// Purpose: Provides Gameplay stubs for testing
// Copyright 2025 RiftForged Game Development Team

#pragma once

#include <string>       // For std::string in TempWeaponProperties
#include <vector>       // Potentially for future stubs, good to have common utilities
#include <cstdlib>      // For rand() in GetStubbedWeaponProperties
#include <stdexcept>    // For std::invalid_argument (if any error checks throw)

#include "../Gameplay/ActivePlayer.h" // For GameLogic::ActivePlayer and likely GameLogic::EquippedWeaponCategory
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h" // For Networking::Shared::DamageInstance and Networking::Shared::DamageType
#include "../FlatBuffers/V0.0.4/riftforged_item_definitions_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"

namespace RiftForged {
    namespace Stubs {
        struct TempWeaponProperties {
            bool isMelee;
            float range;
            float attackCooldownSec;
            RiftForged::Networking::Shared::DamageInstance baseDamageInstance; // From common_types_generated.h
            float projectileSpeed;
            std::string projectileVfxTag;
        };

        // This static helper function provides stubbed weapon properties.
        // It's static to this file due to the anonymous namespace.
        inline TempWeaponProperties GetStubbedWeaponProperties(GameLogic::ActivePlayer* attacker) {
            using namespace RiftForged::Networking::Shared; // For DamageInstance, DamageType
            // Assuming EquippedWeaponCategory is in RiftForged::GameLogic or accessible
            using RiftForged::GameLogic::EquippedWeaponCategory;

            float base_player_attack_cooldown = attacker ? attacker->base_basic_attack_cooldown_sec : 1.0f;
            EquippedWeaponCategory category = attacker ? attacker->current_weapon_category : EquippedWeaponCategory::Unarmed;

            auto create_dmg_inst = [&](int min_dmg, int max_dmg, DamageType type) {
                int amount = min_dmg + (max_dmg > min_dmg ? (rand() % (max_dmg - min_dmg + 1)) : 0);
                return DamageInstance(amount, type, false); // is_crit is false by default from stub
                };

            switch (category) {
            case EquippedWeaponCategory::Generic_Melee_Sword: // Ensure these enum values match your definition
            case EquippedWeaponCategory::Generic_Melee_Axe:
                return { true, 2.5f, base_player_attack_cooldown, create_dmg_inst(10, 15, DamageType_Physical), 0.f, "" };
            case EquippedWeaponCategory::Generic_Melee_Maul:
                return { true, 3.0f, base_player_attack_cooldown * 1.2f, create_dmg_inst(15, 25, DamageType_Physical), 0.f, "" };
            case EquippedWeaponCategory::Generic_Ranged_Bow:
                return { false, 30.0f, base_player_attack_cooldown, create_dmg_inst(12, 18, DamageType_Physical), 40.f, "VFX_Projectile_Arrow" };
            case EquippedWeaponCategory::Generic_Ranged_Gun:
                return { false, 25.0f, base_player_attack_cooldown * 0.8f, create_dmg_inst(8, 12, DamageType_Physical), 50.f, "VFX_Projectile_Bullet" };
            case EquippedWeaponCategory::Generic_Magic_Staff:
                return { false, 20.0f, base_player_attack_cooldown, create_dmg_inst(10, 16, DamageType_Radiant), 30.f, "VFX_Magic_Bolt_Staff" };
            case EquippedWeaponCategory::Generic_Magic_Wand:
                return { false, 18.0f, base_player_attack_cooldown * 0.7f, create_dmg_inst(7, 11, DamageType_Cosmic), 35.f, "VFX_Magic_Bolt_Wand" };
            case EquippedWeaponCategory::Unarmed:
            default:
                return { true, 1.5f, base_player_attack_cooldown, create_dmg_inst(1, 3, DamageType_Physical), 0.f, "" };
            }
        }
    }
}
