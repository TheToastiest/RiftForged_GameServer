// File: GameplayEngine/CombatData.h
// Copyright (c) 2023-2025 RiftForged Game Development Team
// Description: Defines structures for combat outcomes and related logic.
#pragma once
//Add to Repo
#include <string>
#include <vector>
#include <cstdint>

// Ensure these paths point to your V0.0.3 generated FlatBuffers headers
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h" // For DamageType, DamageInstance, Vec3, StunInstance etc.
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C::CombatEventType
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S::CombatEventType

namespace RiftForged {
    namespace GameLogic {
            
        // Details for a single instance of damage being applied to a target
        struct DamageApplicationDetails {
            uint64_t target_id = 0;
			uint64_t source_id = 0; // ID of the entity that dealt the damage (e.g., player, NPC, environment)
            int32_t final_damage_dealt = 0; // Actual damage after mitigation (if calculated here, or pre-mitigation if target applies its own)
            RiftForged::Networking::Shared::DamageType damage_type = RiftForged::Networking::Shared::DamageType::DamageType_MIN; // Default to None
            bool was_crit = false;
            bool was_kill = false;
            // RiftForged::Networking::Shared::Vec3 impact_point; // Optional: if needed for VFX precision

            DamageApplicationDetails() :
                target_id(0),
                source_id(0), // Initialize
                damage_type(RiftForged::Networking::Shared::DamageType_None), // Use actual enumerator
                final_damage_dealt(0),
                was_crit(false),
                was_kill(false)
            {
            }
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
            uint64_t projectile_owner_id = 0;   // <<< ENSURE THIS MEMBER IS PRESENT
            RiftForged::Networking::Shared::Vec3 projectile_start_position;
            RiftForged::Networking::Shared::Vec3 projectile_direction; // Normalized
            float projectile_speed = 0.0f;
            float projectile_max_range = 0.0f;
            std::string projectile_vfx_tag;
            RiftForged::Networking::Shared::DamageInstance projectile_damage_on_hit; // ADDED: Damage details projectile carries

            AttackOutcome() : // Default constructor
                success(false),
                simulated_combat_event_type(RiftForged::Networking::UDP::S2C::CombatEventType_None),
                is_basic_attack(false),
                spawned_projectile(false),
                projectile_id(0),
                projectile_owner_id(0), // Initialize
                projectile_start_position(0.f, 0.f, 0.f),
                projectile_direction(0.f, 1.f, 0.f), // Default forward
                projectile_speed(0.f),
                projectile_max_range(0.f)
                // projectile_damage_on_hit default constructed
            {
            }
        };

    } // namespace GameLogic
} // namespace RiftForged