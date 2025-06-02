// File: NetworkEngine/PacketProcessor.cpp (Fully Refactored with Join Fixes)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "PacketProcessor.h"
#include "MessageDispatcher.h"
#include "../GameServer/GameServerEngine.h"
#include "../Gameplay/PlayerManager.h" 
#include "../Gameplay/ActivePlayer.h"  
#include "../Utils/Logger.h"   
#include "../NetworkEngine/GamePacketHeader.h"

// Include C2S messages header for parsing the C2S_JoinRequestMsg
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" 

namespace RiftForged {
    namespace Networking {

        PacketProcessor::PacketProcessor(MessageDispatcher& dispatcher, RiftForged::Server::GameServerEngine& gameServerEngine)
            : m_messageDispatcher(dispatcher),
            m_gameServerEngine(gameServerEngine) {
            RF_NETWORK_INFO("PacketProcessor (MessageHandler): Initialized.");
        }

        std::optional<S2C_Response> PacketProcessor::ProcessApplicationMessage(
            const NetworkEndpoint& sender_endpoint,
            MessageType messageId,
            const uint8_t* flatbuffer_payload_ptr,
            uint16_t flatbuffer_payload_size) {

            RF_NETWORK_TRACE("PacketProcessor: Processing MessageType: {} from {}, Payload Size: {}",
                EnumNameMessageType(messageId), sender_endpoint.ToString(), flatbuffer_payload_size);

            GameLogic::ActivePlayer* playerContext = nullptr;
            uint64_t playerId = m_gameServerEngine.GetPlayerIdForEndpoint(sender_endpoint);

            if (playerId != 0) {
                playerContext = m_gameServerEngine.GetPlayerManager().FindPlayerById(playerId);
                if (!playerContext) {
                    RF_NETWORK_ERROR("PacketProcessor: PlayerId {} was mapped for endpoint {} but ActivePlayer not found in PlayerManager! Desynchronized session. Cleaning up stale GameServerEngine mapping.",
                        playerId, sender_endpoint.ToString());
                    m_gameServerEngine.OnClientDisconnected(sender_endpoint);
                    return std::nullopt;
                }
                RF_NETWORK_TRACE("PacketProcessor: Existing PlayerId {} found for endpoint {}.", playerId, sender_endpoint.ToString());
            }
            else {
                if (messageId == RiftForged::Networking::MessageType::C2S_JoinRequest) {
                    RF_NETWORK_INFO("PacketProcessor: Received C2S_JoinRequest from new endpoint {}. Attempting to process join...",
                        sender_endpoint.ToString());

                    std::string characterToLoad = "";
                    if (flatbuffer_payload_ptr && flatbuffer_payload_size > 0) {
                        // Verify and parse the FlatBuffer
                        flatbuffers::Verifier verifier(flatbuffer_payload_ptr, flatbuffer_payload_size);
                        if (RiftForged::Networking::UDP::C2S::VerifyRoot_C2S_UDP_MessageBuffer(verifier)) {
                            auto rootMsg = RiftForged::Networking::UDP::C2S::GetRoot_C2S_UDP_Message(flatbuffer_payload_ptr);
                            if (rootMsg && rootMsg->payload_type() == RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_JoinRequest) {
                                auto joinPayload = rootMsg->payload_as_JoinRequest();
                                if (joinPayload && joinPayload->character_id_to_load()) {
                                    characterToLoad = joinPayload->character_id_to_load()->str();
                                    RF_NETWORK_INFO("PacketProcessor: Extracted Character ID '{}' from JoinRequest for endpoint {}.", characterToLoad, sender_endpoint.ToString());
                                }
                                else {
                                    RF_NETWORK_WARN("PacketProcessor: JoinRequest payload did not contain a character_id_to_load for endpoint {}.", sender_endpoint.ToString());
                                }
                            }
                            else {
                                RF_NETWORK_WARN("PacketProcessor: JoinRequest for endpoint {} had incorrect FlatBuffer payload type: Expected JoinRequest, Got {}.",
                                    sender_endpoint.ToString(),
                                    rootMsg ? RiftForged::Networking::UDP::C2S::EnumNameC2S_UDP_Payload(rootMsg->payload_type()) : "Unknown/Null Root");
                                // Still attempt join with empty charID, GameServerEngine might have defaults or reject.
                            }
                        }
                        else {
                            RF_NETWORK_WARN("PacketProcessor: JoinRequest FlatBuffer verification failed for endpoint {}.", sender_endpoint.ToString());
                            // GameServerEngine will likely send JoinFailed if it can't proceed.
                        }
                    }
                    else {
                        RF_NETWORK_WARN("PacketProcessor: JoinRequest for endpoint {} received with no payload.", sender_endpoint.ToString());
                    }

                    // GameServerEngine handles sending S2C_JoinSuccess or S2C_JoinFailed
                    uint64_t newPlayerId = m_gameServerEngine.OnClientAuthenticatedAndJoining(sender_endpoint, characterToLoad);

                    if (newPlayerId != 0) {
                        RF_NETWORK_INFO("PacketProcessor: New player session {} successfully initiated by GameServerEngine for endpoint {}.",
                            newPlayerId, sender_endpoint.ToString());
                        // Join request is fully handled by GameServerEngine, no further dispatch needed for this message.
                        return std::nullopt;
                    }
                    else {
                        RF_NETWORK_ERROR("PacketProcessor: GameServerEngine failed to process join request for endpoint {}. GameServerEngine should have sent S2C_JoinFailed.",
                            sender_endpoint.ToString());
                        return std::nullopt; // Join failed.
                    }
                }
                else {
                    RF_NETWORK_WARN("PacketProcessor: Dropping MessageType {} from unassociated endpoint {} (not a C2S_JoinRequest).",
                        EnumNameMessageType(messageId), sender_endpoint.ToString());
                    return std::nullopt;
                }
            }

            if (!playerContext) {
                RF_NETWORK_WARN("PacketProcessor: No valid player context obtained for endpoint {}. MessageType {} (numeric: {}) not dispatched.",
                    sender_endpoint.ToString(), EnumNameMessageType(messageId), static_cast<int>(messageId));
                return std::nullopt;
            }

            // For all other messages that require an existing player session:
            return m_messageDispatcher.DispatchC2SMessage(
                messageId,
                flatbuffer_payload_ptr,
                flatbuffer_payload_size,
                sender_endpoint,
                playerContext
            );
        }

    } // namespace Networking
} // namespace RiftForged