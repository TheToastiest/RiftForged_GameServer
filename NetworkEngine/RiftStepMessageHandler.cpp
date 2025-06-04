// File: NetworkEngine/UDP/C2S/RiftStepMessageHandler.cpp
#include "RiftStepMessageHandler.h"

// FlatBuffers
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"

#include "../Gameplay/ActivePlayer.h"
#include "../Gameplay/GameplayEngine.h"
#include "../Gameplay/RiftStepLogic.h"
#include "../Utils/Logger.h"
#include "GamePacketHeader.h"

#include <vector>
#include <string>
#include <optional>

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                // Helper function (as provided by you, no change needed here)
                static void PopulateFlatBufferEffectsFromOutcome(
                    flatbuffers::FlatBufferBuilder& builder,
                    const std::vector<RiftForged::GameLogic::GameplayEffectInstance>& game_effects,
                    flatbuffers::Offset<flatbuffers::Vector<int8_t>>& out_fb_effect_types_offset,
                    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<void>>>& out_fb_effect_data_offset)
                {
                    if (game_effects.empty()) {
                        out_fb_effect_types_offset = flatbuffers::Offset<flatbuffers::Vector<int8_t>>();
                        out_fb_effect_data_offset = flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<void>>>();
                        return;
                    }

                    std::vector<int8_t> effect_types_int8_vector;
                    std::vector<flatbuffers::Offset<void>> effect_data_vector;
                    effect_types_int8_vector.reserve(game_effects.size());
                    effect_data_vector.reserve(game_effects.size());

                    for (const auto& effect_instance : game_effects) {
                        flatbuffers::Offset<void> effect_table_offset;
                        switch (effect_instance.effect_payload_type) {
                        case RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaDamage:
                            effect_table_offset = RiftForged::Networking::UDP::S2C::CreateEffect_AreaDamageData(builder,
                                &effect_instance.center_position,
                                effect_instance.radius,
                                &effect_instance.damage
                            ).Union();
                            break;
                        case RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaStun:
                            effect_table_offset = RiftForged::Networking::UDP::S2C::CreateEffect_AreaStunData(builder,
                                &effect_instance.center_position,
                                effect_instance.radius,
                                &effect_instance.stun
                            ).Union();
                            break;
                        case RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_ApplyBuff:
                            effect_table_offset = RiftForged::Networking::UDP::S2C::CreateEffect_ApplyBuffDebuffData(builder,
                                effect_instance.buff_debuff_to_apply,
                                effect_instance.duration_ms
                            ).Union();
                            break;
                        case RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_PersistentArea:
                        {
                            flatbuffers::Offset<flatbuffers::String> fb_visual_effect_tag;
                            if (!effect_instance.visual_effect_tag.empty()) {
                                fb_visual_effect_tag = builder.CreateString(effect_instance.visual_effect_tag);
                            }

                            flatbuffers::Offset<flatbuffers::Vector<uint32_t>> fb_applied_effects_on_contact_offset;
                            if (effect_instance.persistent_area_applied_effects.has_value() &&
                                !effect_instance.persistent_area_applied_effects.value().empty()) {

                                std::vector<uint32_t> effects_as_uints;
                                effects_as_uints.reserve(effect_instance.persistent_area_applied_effects.value().size());
                                for (const auto& cat_enum : effect_instance.persistent_area_applied_effects.value()) {
                                    effects_as_uints.push_back(static_cast<uint32_t>(cat_enum));
                                }
                                if (!effects_as_uints.empty()) {
                                    fb_applied_effects_on_contact_offset = builder.CreateVector(effects_as_uints);
                                }
                            }

                            effect_table_offset = RiftForged::Networking::UDP::S2C::CreateEffect_PersistentAreaData(
                                builder,
                                &effect_instance.center_position,
                                effect_instance.radius,
                                effect_instance.duration_ms,
                                fb_visual_effect_tag,
                                fb_applied_effects_on_contact_offset
                            ).Union();
                        }
                        break;
                        case RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_NONE:
                            continue;
                        default:
                            RF_NETWORK_WARN("PopulateFlatBufferEffectsFromOutcome: Unknown effect_payload_type in GameplayEffectInstance: {}",
                                static_cast<int>(effect_instance.effect_payload_type));
                            continue;
                        }
                        if (effect_table_offset.o != 0) {
                            effect_data_vector.push_back(effect_table_offset);
                            effect_types_int8_vector.push_back(static_cast<int8_t>(effect_instance.effect_payload_type));
                        }
                    }

                    if (!effect_types_int8_vector.empty()) {
                        out_fb_effect_types_offset = builder.CreateVector(effect_types_int8_vector);
                    }
                    else {
                        out_fb_effect_types_offset = flatbuffers::Offset<flatbuffers::Vector<int8_t>>();
                    }
                    if (!effect_data_vector.empty()) {
                        out_fb_effect_data_offset = builder.CreateVector(effect_data_vector);
                    }
                    else {
                        out_fb_effect_data_offset = flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<void>>>();
                    }
                }


                RiftStepMessageHandler::RiftStepMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                    RiftForged::Utils::Threading::TaskThreadPool* taskPool) // New: Receive taskPool
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine),
                    m_taskThreadPool(taskPool) { // New: Initialize m_taskThreadPool
                    RF_NETWORK_INFO("RiftStepMessageHandler: Constructed.");
                    if (m_taskThreadPool) {
                        RF_NETWORK_INFO("RiftStepMessageHandler: TaskThreadPool provided.");
                    }
                    else {
                        RF_NETWORK_WARN("RiftStepMessageHandler: No TaskThreadPool provided.");
                    }
                }

                std::optional<RiftForged::Networking::S2C_Response> RiftStepMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* player,
                    const RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsg* message) {

                    if (!message) {
                        RF_NETWORK_WARN("RiftStepMessageHandler: Received null C2S_RiftStepActivationMsg from {}", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    if (!player) {
                        RF_NETWORK_WARN("RiftStepMessageHandler: Null player pointer received for endpoint {}. This should be caught earlier.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent = message->directional_intent();

                    RF_NETWORK_DEBUG("RiftStepMessageHandler: Calling GameplayEngine for PlayerID: {} with intent: {} ({})",
                        player->playerId,
                        RiftForged::Networking::UDP::C2S::EnumNameRiftStepDirectionalIntent(intent),
                        static_cast<int>(intent));

                    // The core game logic (ExecuteRiftStep) is synchronous and crucial for immediate simulation
                    // updates. We don't offload this directly to the thread pool as it needs to run on
                    // the main simulation thread to maintain consistency.
                    RiftForged::GameLogic::RiftStepOutcome outcome = m_gameplayEngine.ExecuteRiftStep(player, intent);

                    if (!outcome.success) {
                        RF_NETWORK_INFO("RiftStepMessageHandler: GameplayEngine indicated RiftStep failed for PlayerID {}. Reason: {}",
                            player->playerId, outcome.failure_reason_code);
                        return std::nullopt;
                    }

                    RF_NETWORK_INFO("RiftStepMessageHandler: GameplayEngine SUCCESS for PlayerID: {}. Start:({:.1f},{:.1f},{:.1f}), CalcTarget:({:.1f},{:.1f},{:.1f}), FinalPos:({:.1f},{:.1f},{:.1f}), Travel:{}s",
                        player->playerId,
                        outcome.actual_start_position.x(), outcome.actual_start_position.y(), outcome.actual_start_position.z(),
                        outcome.calculated_target_position.x(), outcome.calculated_target_position.y(), outcome.calculated_target_position.z(),
                        outcome.actual_final_position.x(), outcome.actual_final_position.y(), outcome.actual_final_position.z(),
                        outcome.travel_duration_sec);

                    // --- Potential Thread Pool Usage (Hypothetical) ---
                    // If, for example, RiftStep could trigger a very complex, non-blocking environmental check
                    // or a deep logging operation that doesn't need to block the response, you could offload it.
                    // Important: Any data accessed by the enqueued task must be thread-safe or immutable.
                    if (m_taskThreadPool) {
                        // Example: A complex asynchronous check after the RiftStep is processed.
                        // Capture data needed by the task. Be careful with 'player' if it's mutable and
                        // only accessible on the main thread. Here, we're capturing its ID for a log.
                        uint64_t playerId_copy = player->playerId;
                        RiftForged::Networking::Shared::Vec3 finalPos_copy = outcome.actual_final_position;

                        m_taskThreadPool->enqueue([playerId_copy, finalPos_copy]() {
                            // This task runs on a worker thread from the pool.
                            // Simulate a heavy, non-blocking check:
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                            RF_NETWORK_DEBUG("RiftStepMessageHandler (ThreadPool): Performed async post-RiftStep check for Player {}. Final position: ({:.1f}, {:.1f}, {:.1f})",
                                playerId_copy, finalPos_copy.x(), finalPos_copy.y(), finalPos_copy.z());
                            // Do NOT modify 'player' directly from here unless it's thread-safe.
                            });
                    }

                    // Construct S2C_RiftStepInitiatedMsg (as per your existing logic)
                    flatbuffers::FlatBufferBuilder builder(1024);

                    flatbuffers::Offset<flatbuffers::Vector<int8_t>> fb_entry_effects_types;
                    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<void>>> fb_entry_effects_data;
                    PopulateFlatBufferEffectsFromOutcome(builder, outcome.entry_effects_data, fb_entry_effects_types, fb_entry_effects_data);

                    flatbuffers::Offset<flatbuffers::Vector<int8_t>> fb_exit_effects_types;
                    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<void>>> fb_exit_effects_data;
                    PopulateFlatBufferEffectsFromOutcome(builder, outcome.exit_effects_data, fb_exit_effects_types, fb_exit_effects_data);

                    auto start_vfx_fb_str = outcome.start_vfx_id.empty() ? flatbuffers::Offset<flatbuffers::String>() : builder.CreateString(outcome.start_vfx_id);
                    auto travel_vfx_fb_str = outcome.travel_vfx_id.empty() ? flatbuffers::Offset<flatbuffers::String>() : builder.CreateString(outcome.travel_vfx_id);
                    auto end_vfx_fb_str = outcome.end_vfx_id.empty() ? flatbuffers::Offset<flatbuffers::String>() : builder.CreateString(outcome.end_vfx_id);

                    RiftForged::Networking::UDP::S2C::S2C_RiftStepInitiatedMsgBuilder msg_builder(builder);
                    msg_builder.add_instigator_entity_id(player->playerId);
                    msg_builder.add_actual_start_position(&outcome.actual_start_position);
                    msg_builder.add_calculated_target_position(&outcome.calculated_target_position);
                    msg_builder.add_actual_final_position(&outcome.actual_final_position);
                    msg_builder.add_cosmetic_travel_duration_sec(outcome.travel_duration_sec);

                    if (fb_entry_effects_types.o != 0) msg_builder.add_entry_effects_type(fb_entry_effects_types);
                    if (fb_entry_effects_data.o != 0) msg_builder.add_entry_effects(fb_entry_effects_data);

                    if (fb_exit_effects_types.o != 0) msg_builder.add_exit_effects_type(fb_exit_effects_types);
                    if (fb_exit_effects_data.o != 0) msg_builder.add_exit_effects(fb_exit_effects_data);

                    if (start_vfx_fb_str.o != 0) msg_builder.add_start_vfx_id(start_vfx_fb_str);
                    if (travel_vfx_fb_str.o != 0) msg_builder.add_travel_vfx_id(travel_vfx_fb_str);
                    if (end_vfx_fb_str.o != 0) msg_builder.add_end_vfx_id(end_vfx_fb_str);

                    auto s2c_payload_offset = msg_builder.Finish();

                    RiftForged::Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                    root_builder.add_payload_type(RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_RiftStepInitiated);
                    root_builder.add_payload(s2c_payload_offset.Union());
                    auto root_offset = root_builder.Finish();
                    builder.Finish(root_offset);

                    S2C_Response response_to_send;
                    response_to_send.data = builder.Release(); // Assumes S2C_Response.data is a FlatBuffer builder::Release() type (e.g., FlatBuffer's `DetachedBuffer`)
                    response_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_RiftStepInitiated;
                    response_to_send.broadcast = true;
                    // response_to_send.specific_recipient remains default/empty for broadcast

                    RF_NETWORK_INFO("RiftStepMessageHandler: S2C_RiftStepInitiatedMsg prepared for broadcast. PlayerID: {}. Originator: [{}]",
                        player->playerId, sender_endpoint.ToString());
                    return response_to_send;
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged