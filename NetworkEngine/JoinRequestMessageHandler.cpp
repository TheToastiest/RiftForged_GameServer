// File: UDPServer/PacketManagement/Handlers_C2S/JoinRequestMessageHandler.cpp
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "JoinRequestMessageHandler.h"
#include "../../Utils/Logger.h" // For RF_NETWORK_... macros
#include "../../GameServer/GameServerEngine.h" // For GameServerEngine methods (like OnClientAuthenticatedAndJoining)
#include "flatbuffers/flatbuffers.h" // For FlatBufferBuilder, DetachedBuffer

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                // Constructor implementation
                JoinRequestMessageHandler::JoinRequestMessageHandler(RiftForged::Server::GameServerEngine& gameServerEngine)
                    : m_gameServerEngine(gameServerEngine) {
                    RF_NETWORK_INFO("JoinRequestMessageHandler: Initialized.");
                }

                std::optional<S2C_Response> JoinRequestMessageHandler::Process(
                    const NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* player, // This will be nullptr for new join requests
                    const C2S_JoinRequestMsg* message) {

                    // Basic null check for the FlatBuffer message pointer
                    if (!message) {
                        RF_NETWORK_ERROR("JoinRequestMessageHandler: Received null C2S_JoinRequestMsg from %s. Building JoinFailed response.",
                            sender_endpoint.ToString());

                        flatbuffers::FlatBufferBuilder builder(128);
                        auto reason_offset = builder.CreateString("Malformed join request received.");
                        auto join_failed_payload = RiftForged::Networking::UDP::S2C::CreateS2C_JoinFailedMsg(builder, reason_offset, -1); // -1 for general malformed

                        auto root_s2c_message = RiftForged::Networking::UDP::S2C::CreateRoot_S2C_UDP_Message(builder,
                            RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinFailedMsg,
                            join_failed_payload.Union()
                        );
                        builder.Finish(root_s2c_message);

                        S2C_Response response_to_send;
                        response_to_send.specific_recipient = sender_endpoint;
                        response_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinFailedMsg;
                        response_to_send.data = builder.Release(); // Move the DetachedBuffer
                        response_to_send.broadcast = false; // Explicitly set to false

                        return response_to_send;
                    }

                    if (player != nullptr) {
                        // This scenario indicates a client is trying to join again while already in a session.
                        RF_NETWORK_WARN("JoinRequestMessageHandler: Received JoinRequest from %s for existing player. Player ID: %llu. Building JoinFailed (already logged in) response.",
                            sender_endpoint.ToString(), player->playerId); // CHANGED: player->GetPlayerId() to player->playerId

                        flatbuffers::FlatBufferBuilder builder(128);
                        auto reason_offset = builder.CreateString("You are already logged in.");
                        auto join_failed_payload = RiftForged::Networking::UDP::S2C::CreateS2C_JoinFailedMsg(builder, reason_offset, 1); // Code 1 for already logged in

                        auto root_s2c_message = RiftForged::Networking::UDP::S2C::CreateRoot_S2C_UDP_Message(builder,
                            RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinFailedMsg,
                            join_failed_payload.Union()
                        );
                        builder.Finish(root_s2c_message);

                        S2C_Response response_to_send;
                        response_to_send.specific_recipient = sender_endpoint;
                        response_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinFailedMsg;
                        response_to_send.data = builder.Release(); // Move the DetachedBuffer
                        response_to_send.broadcast = false; // Explicitly set to false

                        return response_to_send;
                    }

                    std::string characterIdToLoad = "";
                    if (message->character_id_to_load()) {
                        characterIdToLoad = message->character_id_to_load()->str();
                    }

                    RF_NETWORK_INFO("JoinRequestMessageHandler: Processing new JoinRequest from %s with character ID: '%s'. Delegating to GameServerEngine for core logic.",
                        sender_endpoint.ToString(), characterIdToLoad.c_str());

                    // Delegate the actual join process (authentication, player creation, session mapping)
                    // to GameServerEngine. GameServerEngine will manage the game state aspect.
                    uint64_t newPlayerId = m_gameServerEngine.OnClientAuthenticatedAndJoining(
                        sender_endpoint, characterIdToLoad
                    );

                    // Now, based on the result from GameServerEngine, build the appropriate S2C_Response.
                    flatbuffers::FlatBufferBuilder builder(256); // Use a fresh builder for the response

                    if (newPlayerId != 0) { // Assuming 0 indicates failure in OnClientAuthenticatedAndJoining
                        RF_NETWORK_INFO("JoinRequestMessageHandler: Join request for %s successful. Assigned Player ID: %llu. Building JoinSuccess response.",
                            sender_endpoint.ToString(), newPlayerId);

                        auto welcome_message_offset = builder.CreateString("Welcome to RiftForged!");
                        // Assuming your GameServerEngine provides the tick rate
                        auto join_success_payload = RiftForged::Networking::UDP::S2C::CreateS2C_JoinSuccessMsg(builder,
                            newPlayerId,
                            welcome_message_offset,
                            m_gameServerEngine.GetServerTickRateHz() // Assuming this method exists
                        );

                        auto root_s2c_message = RiftForged::Networking::UDP::S2C::CreateRoot_S2C_UDP_Message(builder,
                            RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinSuccessMsg,
                            join_success_payload.Union()
                        );
                        builder.Finish(root_s2c_message);

                        S2C_Response response_to_send;
                        response_to_send.specific_recipient = sender_endpoint;
                        response_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinSuccessMsg;
                        response_to_send.data = builder.Release(); // Move the DetachedBuffer
                        response_to_send.broadcast = false; // Explicitly set to false

                        return response_to_send;
                    }
                    else {
                        // GameServerEngine's OnClientAuthenticatedAndJoining failed.
                        // The specific reason might need to be passed back from GSE.
                        // For simplicity, providing a generic failure reason here.
                        RF_NETWORK_ERROR("JoinRequestMessageHandler: GameServerEngine failed to process join request for %s. Building JoinFailed response.",
                            sender_endpoint.ToString());

                        auto reason_offset = builder.CreateString("Server failed to process your join request.");
                        auto join_failed_payload = RiftForged::Networking::UDP::S2C::CreateS2C_JoinFailedMsg(builder, reason_offset, 2); // Code 2 for server-side error

                        auto root_s2c_message = RiftForged::Networking::UDP::S2C::CreateRoot_S2C_UDP_Message(builder,
                            RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinFailedMsg,
                            join_failed_payload.Union()
                        );
                        builder.Finish(root_s2c_message);

                        S2C_Response response_to_send;
                        response_to_send.specific_recipient = sender_endpoint;
                        response_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_S2C_JoinFailedMsg;
                        response_to_send.data = builder.Release(); // Move the DetachedBuffer
                        response_to_send.broadcast = false; // Explicitly set to false

                        return response_to_send;
                    }
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged