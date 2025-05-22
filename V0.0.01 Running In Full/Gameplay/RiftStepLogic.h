#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Shared::Vec3
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C::RiftStepEffectPayload enum
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h" // For C2S::RiftStepMessageHandler

namespace RiftForged {
    namespace GameLogic { // Keeping these core logic structs in GameLogic

        // This C++ struct holds the data for a specific gameplay effect instance.
        // GameplayEngine determines this, and the MessageHandler will use it
        // to build the appropriate FlatBuffer Effect_XYZData table for an S2C message.
        struct GameplayEffectInstance {
            RiftForged::Networking::UDP::S2C::RiftStepEffectPayload effect_payload_type;

            RiftForged::Networking::Shared::Vec3 center_position;
            float radius = 0.0f;
            uint32_t duration_ms = 0; // For effects like PersistentArea or ApplyBuffDebuff
            RiftForged::Networking::Shared::DamageInstance damage;
            RiftForged::Networking::Shared::StunInstance stun;
            RiftForged::Networking::Shared::StatusEffectCategory buff_debuff_to_apply;
            std::string visual_effect_tag; // For persistent area visuals or specific effect markers
            // uint64_t target_entity_id_for_buff_debuff; // If Effect_ApplyBuffDebuffData needs it

            // Default constructor - ensures members have reasonable defaults
            GameplayEffectInstance() :
                effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_NONE), // Default to NONE
                center_position(0.0f, 0.0f, 0.0f),
                radius(0.0f),
                duration_ms(0),
                damage(0, RiftForged::Networking::Shared::DamageType_Physical),
                stun({ RiftForged::Networking::Shared::StunSeverity_Light, 0 }), // Use designated initializer syntax for structs if C++20, else direct member init
                buff_debuff_to_apply(RiftForged::Networking::Shared::StatusEffectCategory_None),
                visual_effect_tag("")
                // target_entity_id_for_buff_debuff(0)
            {
            }

            // Convenience constructor for an Area Damage effect
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad,
                const RiftForged::Networking::Shared::DamageInstance& dmg)
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaDamage),
                center_position(center), radius(rad), damage(dmg),
                duration_ms(0), stun({ RiftForged::Networking::Shared::StunSeverity_Light, 0 }),
                buff_debuff_to_apply(RiftForged::Networking::Shared::StatusEffectCategory_None), visual_effect_tag("") {
            }

            // Convenience constructor for an Area Stun effect
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad,
                const RiftForged::Networking::Shared::StunInstance& stn)
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaStun),
                center_position(center), radius(rad), stun(stn),
                duration_ms(0), damage(0, RiftForged::Networking::Shared::DamageType_Physical),
                buff_debuff_to_apply(RiftForged::Networking::Shared::StatusEffectCategory_None), visual_effect_tag("") {
            }

            // TODO: Add constructors for Effect_ApplyBuffDebuffData, Effect_PersistentAreaData as needed
        };

        struct RiftStepOutcome {
            bool success = false;
            std::string failure_reason_code; // e.g., "ON_COOLDOWN", "INVALID_PLAYER_STATE"

            RiftForged::Networking::Shared::Vec3 actual_start_position;
            RiftForged::Networking::Shared::Vec3 calculated_target_position; // Where RiftStep aimed before collision
            RiftForged::Networking::Shared::Vec3 actual_final_position;      // Where player actually lands (after server-side collision)

            // travel_duration_sec is for client cosmetic visuals. Server resolves position instantly.
            float travel_duration_sec = 0.0f;

            std::vector<GameplayEffectInstance> entry_effects_data;  // Effects at the start point (e.g., Solar stun)
            std::vector<GameplayEffectInstance> exit_effects_data;   // Effects at the end point (e.g., Solar damage)

            std::string start_vfx_id;    // For initiation visuals
            std::string travel_vfx_id;   // Trail/phase effect (can be very short if travel_duration is small)
            std::string end_vfx_id;      // For arrival/impact visuals
            // Consider adding SFX IDs: start_sfx_id, travel_sfx_id, end_sfx_id
        };

    }
} // namespace RiftForged::GameLogic