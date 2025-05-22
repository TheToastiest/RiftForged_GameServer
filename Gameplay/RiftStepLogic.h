#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Shared::Vec3
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C::RiftStepEffectPayload enum
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h" // For C2S::RiftStepMessageHandler

namespace RiftForged {
    namespace GameLogic { // Keeping these core logic structs in GameLogic

        // C++ struct to hold data for a specific gameplay effect instance,
        // determined by GameplayEngine. MessageHandler translates this to FlatBuffers.
        struct GameplayEffectInstance {
            RiftForged::Networking::UDP::S2C::RiftStepEffectPayload effect_payload_type;

            RiftForged::Networking::Shared::Vec3 center_position;
            float radius = 0.0f;
            uint32_t duration_ms = 0;
            RiftForged::Networking::Shared::DamageInstance damage;
            RiftForged::Networking::Shared::StunInstance stun;
            RiftForged::Networking::Shared::StatusEffectCategory buff_debuff_to_apply;
            std::string visual_effect_tag;
            // Add other common fields as needed for more effect types in your union

            // Default constructor
            GameplayEffectInstance() :
                effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_NONE),
                center_position(0.0f, 0.0f, 0.0f),
                radius(0.0f),
                duration_ms(0),
                damage(0, RiftForged::Networking::Shared::DamageType_Physical),
                stun(RiftForged::Networking::Shared::StunSeverity_Light, 0),
                buff_debuff_to_apply(RiftForged::Networking::Shared::StatusEffectCategory_None)
            {
            }

            // Convenience constructors for common effects (examples)
            // For Area Damage
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad,
                const RiftForged::Networking::Shared::DamageInstance& dmg)
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaDamage),
                center_position(center), radius(rad), damage(dmg) {
            }

            // For Area Stun
            GameplayEffectInstance(const RiftForged::Networking::Shared::Vec3& center, float rad,
                const RiftForged::Networking::Shared::StunInstance& stn)
                : effect_payload_type(RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaStun),
                center_position(center), radius(rad), stun(stn) {
            }
        };

        struct RiftStepOutcome {
            bool success = false;
            std::string failure_reason_code; // e.g., "ON_COOLDOWN", "INVALID_STATE"

            RiftForged::Networking::Shared::Vec3 actual_start_position;
            RiftForged::Networking::Shared::Vec3 calculated_target_position; // Where the rift aims before potential collision
            RiftForged::Networking::Shared::Vec3 actual_final_position;      // Where player lands (after server-side collision if any)

            // travel_duration_sec is primarily for client visuals if server resolution is instant.
            // Set to 0.0f or a small cosmetic value (e.g., 0.1s - 0.25s).
            float travel_duration_sec = 0.0f;

            std::vector<GameplayEffectInstance> entry_effects_data;  // Effects applied at the start of the rift
            std::vector<GameplayEffectInstance> exit_effects_data;   // Effects applied upon arrival/impact (since it's instant server-side)

            std::string start_vfx_id;    // For initiation visuals
            std::string travel_vfx_id;   // The trail/phase effect (can be short if travel_duration is small)
            std::string end_vfx_id;      // For arrival/impact visuals
            // Consider adding SFX IDs too
        };

    }
} // namespace RiftForged::GameLogic