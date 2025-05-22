// File: UDPServer/PacketManagement/Handlers_C2S/RiftStepMessageHandler.cpp
#include "RiftStepMessageHandler.h" // Should bring in S2C_Response via NetworkCommon.h

// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.

#include "../Gameplay/PlayerManager.h"
#include "../Gameplay/GameplayEngine.h"
#include "../Gameplay/RiftStepLogic.h"
#include "GamePacketHeader.h" // For RiftForged::Networking::MessageType
#include <iostream> // Replace with your actual logging system
#include <vector>
#include <chrono> // For timestamps if needed
#include <map>
#include <string>

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                // Constructor (ensure this matches your .h file)
                RiftStepMessageHandler::RiftStepMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine)
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine) {
                    std::cout << "RiftStepMessageHandler: Constructed with PlayerManager and GameplayEngine." << std::endl;
                }

                // Helper function to build FlatBuffer effect vectors from GameplayEffectInstance
                // (This is the version from response #123, ensure it's present and correct, using int8_t for types)
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
                    // ... (implementation from response #123 to populate these vectors based on game_effects) ...
                    // Ensure it correctly uses static_cast<int8_t>(effect_instance.effect_payload_type)
                    // and creates the correct Effect_XYZData tables.
                    for (const auto& effect_instance : game_effects) {
                        flatbuffers::Offset<void> effect_table_offset;
                        switch (effect_instance.effect_payload_type) {
                        case RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaDamage:
                            effect_table_offset = RiftForged::Networking::UDP::S2C::CreateEffect_AreaDamageData(builder,
                                &effect_instance.center_position, effect_instance.radius, &effect_instance.damage).Union();
                            break;
                        case RiftForged::Networking::UDP::S2C::RiftStepEffectPayload_AreaStun:
                            effect_table_offset = RiftForged::Networking::UDP::S2C::CreateEffect_AreaStunData(builder,
                                &effect_instance.center_position, effect_instance.radius, &effect_instance.stun).Union();
                            break;
                            // TODO: Add other effect types
                        default: continue;
                        }
                        if (effect_table_offset.o != 0) {
                            effect_data_vector.push_back(effect_table_offset);
                            effect_types_int8_vector.push_back(static_cast<int8_t>(effect_instance.effect_payload_type));
                        }
                    }
                    if (!effect_types_int8_vector.empty()) {
                        out_fb_effect_types_offset = builder.CreateVector(effect_types_int8_vector);
                    }
                    if (!effect_data_vector.empty()) {
                        out_fb_effect_data_offset = builder.CreateVector(effect_data_vector);
                    }
                }


                std::optional<RiftForged::Networking::S2C_Response> RiftStepMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    const RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsg* message) {

                    if (!message) {
                        std::cerr << "RiftStepMessageHandler: Received null C2S_RiftStepActivationMsg from " << sender_endpoint.ToString() << std::endl;
                        return std::nullopt;
                    }

                    RiftForged::GameLogic::ActivePlayer* player = m_playerManager.GetOrCreatePlayer(sender_endpoint);
                    if (!player) {
                        std::cerr << "RiftStepMessageHandler: PlayerManager could not get/create player for " << sender_endpoint.ToString() << std::endl;
                        return std::nullopt;
                    }

                    RiftForged::Networking::UDP::C2S::RiftStepDirectionalIntent intent = message->directional_intent();

                    // std::cout << "RiftStepMessageHandler: Calling GameplayEngine for PlayerID: " << player->playerId << " with intent: " << static_cast<int>(intent) << std::endl;

                    // Call GameplayEngine to execute the RiftStep.
                    // GameplayEngine will update player->position instantly and set player->isDirty.
                    RiftForged::GameLogic::RiftStepOutcome outcome = m_gameplayEngine.ExecuteRiftStep(player, intent);

                    if (!outcome.success) {
                        std::cout << "RiftStepMessageHandler: GameplayEngine indicated RiftStep failed for PlayerID "
                            << player->playerId << ". Reason: " << outcome.failure_reason_code << std::endl;
                        // TODO: Optionally build and return an S2C_CommandFailureMsg
                        return std::nullopt;
                    }

                    std::cout << "RiftStepMessageHandler: GameplayEngine executed RiftStep for PlayerID " << player->playerId
                        << ". Start: (" << outcome.actual_start_position.x() << "," << outcome.actual_start_position.y() << "," << outcome.actual_start_position.z() << ")"
                        << ", Final Pos: (" << outcome.actual_final_position.x() << "," << outcome.actual_final_position.y() << "," << outcome.actual_final_position.z() << ")"
                        << ", Cosmetic Duration: " << outcome.travel_duration_sec << "s" << std::endl;

                    // --- Build S2C_RiftStepInitiatedMsg (or S2C_RiftStepExecutedMsg if you rename it) ---
                    // This message now signals an effectively instant teleport from the server's state perspective.
                    // The travel_duration_sec is for client-side visuals.
                    flatbuffers::FlatBufferBuilder builder(1024);

                    flatbuffers::Offset<flatbuffers::Vector<int8_t>> fb_entry_effects_types;
                    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<void>>> fb_entry_effects_data;
                    PopulateFlatBufferEffectsFromOutcome(builder, outcome.entry_effects_data, fb_entry_effects_types, fb_entry_effects_data);

                    flatbuffers::Offset<flatbuffers::Vector<int8_t>> fb_exit_effects_types;
                    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<void>>> fb_exit_effects_data;
                    // Since RiftStep is instant server-side, GameplayEngine can determine exit effects immediately.
                    PopulateFlatBufferEffectsFromOutcome(builder, outcome.exit_effects_data, fb_exit_effects_types, fb_exit_effects_data);

                    auto start_vfx_fb_str = outcome.start_vfx_id.empty() ? 0 : builder.CreateString(outcome.start_vfx_id);
                    auto travel_vfx_fb_str = outcome.travel_vfx_id.empty() ? 0 : builder.CreateString(outcome.travel_vfx_id);
                    auto end_vfx_fb_str = outcome.end_vfx_id.empty() ? 0 : builder.CreateString(outcome.end_vfx_id);

                    RiftForged::Networking::UDP::S2C::S2C_RiftStepInitiatedMsgBuilder msg_builder(builder);
                    msg_builder.add_instigator_entity_id(player->playerId);
                    msg_builder.add_actual_start_position(&outcome.actual_start_position);
                    // calculated_target_position might be the same as actual_final_position if no collision happened
                    msg_builder.add_calculated_target_position(&outcome.calculated_target_position);
                    msg_builder.add_actual_final_position(&outcome.actual_final_position);
                    msg_builder.add_cosmetic_travel_duration_sec(outcome.travel_duration_sec); // Client uses this for visuals

                    if (fb_entry_effects_data.o != 0) {
                        msg_builder.add_entry_effects_type(fb_entry_effects_types);
                        msg_builder.add_entry_effects(fb_entry_effects_data);
                    }
                    if (fb_exit_effects_data.o != 0) {
                        msg_builder.add_exit_effects_type(fb_exit_effects_types);
                        msg_builder.add_exit_effects(fb_exit_effects_data);
                    }

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
                    response_to_send.data = builder.Release();
                    response_to_send.messageType = RiftForged::Networking::MessageType::S2C_RiftStepInitiated;
                    response_to_send.broadcast = true; // RiftStep events are usually broadcast

                    std::cout << "RiftStepMessageHandler: S2C_RiftStepInitiatedMsg prepared for broadcast. PlayerID: " << player->playerId << std::endl;
                    return response_to_send;
                }

            }
        }
    }
}