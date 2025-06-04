// File: NetworkEngine/UDP/C2S/PingMessageHandler.cpp
#include "PingMessageHandler.h"
// Generated FlatBuffer headers
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
#include "GamePacketHeader.h"      // For GamePacketHeader, MessageType, GetGamePacketHeaderSize()
#include "../Gameplay/ActivePlayer.h" // For ActivePlayer definition (if needed for player context)
#include "../Utils/Logger.h"      // Use your logger instead of iostream
#include <chrono>

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                PingMessageHandler::PingMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager, // Keep existing dependency
                    RiftForged::Utils::Threading::TaskThreadPool* taskPool) // New: Receive taskPool
                    : m_playerManager(playerManager), // Initialize existing dependency
                    m_taskThreadPool(taskPool) {      // New: Initialize m_taskThreadPool
                    RF_NETWORK_INFO("PingMessageHandler: Constructed.");
                    if (m_taskThreadPool) {
                        RF_NETWORK_INFO("PingMessageHandler: TaskThreadPool provided (not typically used by this handler).");
                    }
                    else {
                        RF_NETWORK_WARN("PingMessageHandler: No TaskThreadPool provided.");
                    }
                }

                std::optional<S2C_Response> PingMessageHandler::Process(
                    const NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* player, // Ensure this is passed for context
                    const C2S_PingMsg* message) {

                    if (!message) {
                        RF_NETWORK_WARN("PingMessageHandler: Received null C2S_PingMsg from {}.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    // Logging using your RF_NETWORK_INFO macro
                    RF_NETWORK_INFO("PingMessageHandler: Received Ping from {}. Client Timestamp: {}. Player ID: {}.",
                        sender_endpoint.ToString(), message->client_timestamp_ms(), player ? player->playerId : 0);

                    flatbuffers::FlatBufferBuilder builder(256); // Keep builder local to where message is built
                    uint64_t server_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();

                    // --- FIX APPLIED HERE: Removed playerId_to_send argument ---
                    auto pong_payload_offset = RiftForged::Networking::UDP::S2C::CreateS2C_PongMsg(builder,
                        message->client_timestamp_ms(),
                        server_timestamp); // Only two uint64_t arguments for timestamp data

                    RiftForged::Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                    root_builder.add_payload_type(RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_Pong);
                    root_builder.add_payload(pong_payload_offset.Union());
                    auto root_offset = root_builder.Finish();
                    builder.Finish(root_offset);

                    // --- Thread Pool Usage (Not typically needed) ---
                    // Ping/Pong operations are extremely lightweight and latency-sensitive.
                    // Offloading this to a thread pool would introduce unnecessary overhead
                    // and could potentially increase the perceived latency for ping measurements.
                    // Therefore, the TaskThreadPool is typically NOT used by this handler.
                    // If, hypothetically, there was a *very* heavy, non-critical logging or analytics
                    // task to perform *after* the pong response is prepared, it *could* be offloaded:
                    // if (m_taskThreadPool) {
                    //     uint64_t client_ts_copy = message->client_timestamp_ms();
                    //     uint64_t server_ts_copy = server_timestamp;
                    //     std::string sender_str_copy = sender_endpoint.ToString();
                    //     m_taskThreadPool->enqueue([client_ts_copy, server_ts_copy, sender_str_copy]() {
                    //         std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    //         RF_NETWORK_DEBUG("PingMessageHandler (ThreadPool): Async heavy ping analytics for {} (Client TS: {}, Server TS: {})",
                    //             sender_str_copy, client_ts_copy, server_ts_copy);
                    //     });
                    // }


                    // Prepare the S2C_Response struct to be returned
                    S2C_Response response_data_to_send;
                    response_data_to_send.data = builder.Release(); // Transfer ownership of the buffer
                    response_data_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_Pong;
                    response_data_to_send.broadcast = false;
                    response_data_to_send.specific_recipient = sender_endpoint; // Send back to original sender

                    RF_NETWORK_INFO("PingMessageHandler: S2C_PongMsg prepared for {}.", sender_endpoint.ToString());
                    return response_data_to_send;
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged