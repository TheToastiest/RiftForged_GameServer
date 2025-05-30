// File: GameplayEngine/CombatData.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
// Description: Defines structures for combat outcomes and related logic.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Ensure these paths point to your V0.0.3 generated FlatBuffers headers
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h" // For DamageType, DamageInstance, Vec3, StunInstance etc.
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h" // For S2C::CombatEventType
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h" // For C2S::CombatEventType

namespace RiftForged {
    namespace GameLogic {
            
        // Details for a single instance of damage being applied to a target
        struct DamageApplicationDetails {
            uint64_t target_id = 0;
            int32_t final_damage_dealt = 0; // Actual damage after mitigation (if calculated here, or pre-mitigation if target applies its own)
            RiftForged::Networking::Shared::DamageType damage_type = RiftForged::Networking::Shared::DamageType::DamageType_MIN; // Default to None
            bool was_crit = false;
            bool was_kill = false;
            // RiftForged::Networking::Shared::Vec3 impact_point; // Optional: if needed for VFX precision

            DamageApplicationDetails() = default;
        };

        // Outcome of a basic attack or combat ability execution
        struct AttackOutcome {
            bool success = true; // Default to true; set to false if pre-checks fail (cooldown, resources, state)
            std::string failure_reason_code;

            bool is_basic_attack = false; // True if this outcome is for a basic attack
            RiftForged::Networking::UDP::S2C::CombatEventType simulated_combat_event_type = RiftForged::Networking::UDP::S2C::CombatEventType::CombatEventType_MIN; // More descriptive default

            std::string attack_animation_tag_for_caster; // e.g., "Swing_Sword_Basic_01"

            // For direct hits (melee) or immediate AoE effects
            std::vector<DamageApplicationDetails> damage_events;

            // For Ranged Projectile Basic Attacks / Abilities
            bool spawned_projectile = false;
            uint64_t projectile_id = 0; // Unique ID for the spawned projectile
            RiftForged::Networking::Shared::Vec3 projectile_start_position;
            RiftForged::Networking::Shared::Vec3 projectile_direction; // Normalized
            float projectile_speed = 0.0f;
            float projectile_max_range = 0.0f;
            std::string projectile_vfx_tag;
            RiftForged::Networking::Shared::DamageInstance projectile_damage_on_hit; // ADDED: Damage details projectile carries

            AttackOutcome() {
                // Ensure DamageInstance within projectile_damage_on_hit is properly defaulted
                projectile_damage_on_hit = RiftForged::Networking::Shared::DamageInstance(
                    0,
                    RiftForged::Networking::Shared::DamageType::DamageType_MIN,
                    false
                );
            }
        };

    } // namespace GameLogic
} // namespace RiftForged