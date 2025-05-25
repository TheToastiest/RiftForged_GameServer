#include "PingMessageHandler.h"
// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.
#include "GamePacketHeader.h"     // For GamePacketHeader, MessageType, GetGamePacketHeaderSize()
#include <iostream>
#include <chrono>
#include <vector>   // For constructing send_buffer if done at a higher level
#include <cstring>  // For memcpy if constructing send_buffer at a higher level

namespace RiftForged {  
    namespace Networking {
        namespace UDP {
            namespace C2S {

                PingMessageHandler::PingMessageHandler(RiftForged::GameLogic::PlayerManager& playerManager) {
                    std::cout << "PingMessageHandler: Constructed (default)." << std::endl;
                }

                std::optional<S2C_Response> PingMessageHandler::Process(
                    const NetworkEndpoint& sender_endpoint,
                    const C2S_PingMsg* message) {

                    if (!message) {
                        std::cerr << "PingMessageHandler: Received null C2S_PingMsg." << std::endl;
                        return std::nullopt;
                    }

                    std::cout << "PingMessageHandler: Received Ping from " << sender_endpoint.ToString()
                        << " Client Timestamp: " << message->client_timestamp_ms() << std::endl;

                    flatbuffers::FlatBufferBuilder builder(256); // Keep builder local to where message is built
                    uint64_t server_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();

                    auto pong_payload_offset = RiftForged::Networking::UDP::S2C::CreateS2C_PongMsg(builder,
                        message->client_timestamp_ms(),
                        server_timestamp);

                    RiftForged::Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                    root_builder.add_payload_type(RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_Pong);
                    root_builder.add_payload(pong_payload_offset.Union());
                    auto root_offset = root_builder.Finish();
                    builder.Finish(root_offset);

                    // Prepare the S2C_Response struct to be returned
                    S2C_Response response_data_to_send;
                    response_data_to_send.data = builder.Release(); // Transfer ownership of the buffer
                    response_data_to_send.messageType = RiftForged::Networking::MessageType::S2C_Pong;
                    response_data_to_send.broadcast = false;
                    response_data_to_send.specific_recipient = sender_endpoint; // Send back to original sender

                    std::cout << "PingMessageHandler: S2C_PongMsg prepared for " << sender_endpoint.ToString() << std::endl;
                    return response_data_to_send;
                }

            }
        }
    }
}