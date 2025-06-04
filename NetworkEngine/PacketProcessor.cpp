// File: NetworkEngine/PacketProcessor.cpp (Fully Refactored with Join Fixes)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "PacketProcessor.h"
#include "MessageDispatcher.h"
#include "../GameServer/GameServerEngine.h"
#include "../Gameplay/PlayerManager.h"    
#include "../Gameplay/ActivePlayer.h"     
#include "../Utils/Logger.h"    
// #include "../NetworkEngine/GamePacketHeader.h" // Not directly needed by PacketProcessor for MessageType or header size now.
                                                // Only UDPPacketHandler deals with GamePacketHeader.

// Include C2S messages header for parsing the C2S_JoinRequest
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" 
// Include S2C messages header for EnumNameS2C_UDP_Payload in logging
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"


namespace RiftForged {
    namespace Networking {

        PacketProcessor::PacketProcessor(MessageDispatcher& dispatcher, RiftForged::Server::GameServerEngine& gameServerEngine)
            : m_messageDispatcher(dispatcher),
            m_gameServerEngine(gameServerEngine) {
            RF_NETWORK_INFO("PacketProcessor (MessageHandler): Initialized.");
        }

        // Updated signature: Now receives 'player' directly from UDPPacketHandler
        std::optional<S2C_Response> PacketProcessor::ProcessApplicationMessage(
            const NetworkEndpoint& sender_endpoint,
            const uint8_t* flatbuffer_payload_ptr,
            uint16_t flatbuffer_payload_size,
            RiftForged::GameLogic::ActivePlayer* player) { // 'player' is now a direct parameter

            // Before anything else, try to get the FlatBuffer's actual payload type for logging and initial dispatch.
            UDP::C2S::C2S_UDP_Payload current_payload_type = UDP::C2S::C2S_UDP_Payload_NONE; // Default to NONE
            const UDP::C2S::Root_C2S_UDP_Message* root_message = nullptr;

            // Basic size check for FlatBuffer
            if (flatbuffer_payload_ptr && flatbuffer_payload_size >= sizeof(uint32_t) * 2) {
                flatbuffers::Verifier verifier(flatbuffer_payload_ptr, flatbuffer_payload_size);
                if (UDP::C2S::VerifyRoot_C2S_UDP_MessageBuffer(verifier)) {
                    root_message = UDP::C2S::GetRoot_C2S_UDP_Message(flatbuffer_payload_ptr);
                    if (root_message && root_message->payload()) {
                        current_payload_type = root_message->payload_type();
                    }
                }
                else {
                    RF_NETWORK_WARN("PacketProcessor: Incoming FlatBuffer from %s failed verification. Size: %u. Discarding.",
                        sender_endpoint.ToString(), flatbuffer_payload_size);
                    return std::nullopt; // Invalid FlatBuffer, discard early.
                }
            }
            else {
                RF_NETWORK_WARN("PacketProcessor: Incoming FlatBuffer from %s has invalid size %u. Discarding.",
                    sender_endpoint.ToString(), flatbuffer_payload_size);
                return std::nullopt; // Empty or too small payload
            }

            RF_NETWORK_TRACE("PacketProcessor: Processing FlatBuffer Type: %s from %s, Payload Size: %u",
                UDP::C2S::EnumNameC2S_UDP_Payload(current_payload_type), sender_endpoint.ToString(), flatbuffer_payload_size);

            // The 'player' parameter is now passed directly from UDPPacketHandler.
            // PacketProcessor's role is not to look it up, but to use it or check if it's valid for the message.

            // Special handling for JoinRequest: It's the only message type that might arrive before a player is established.
            // We pass a 'nullptr' player if the message is a JoinRequest, as the dispatcher's handler will create/manage it.
            if (current_payload_type == RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_JoinRequest) {
                RF_NETWORK_INFO("PacketProcessor: Received C2S_JoinRequest from new endpoint %s. Attempting to process join via dispatcher...",
                    sender_endpoint.ToString());

                // For JoinRequest, always pass nullptr as the player context to the dispatcher.
                // The JoinRequestMessageHandler (via MessageDispatcher) is responsible for player creation/association.
                return m_messageDispatcher.DispatchC2SMessage(
                    flatbuffer_payload_ptr,
                    flatbuffer_payload_size,
                    sender_endpoint,
                    nullptr // Explicitly pass nullptr for player context for a JoinRequest
                );
            }

            // For all other messages, a player must exist.
            if (!player) { // Use the 'player' parameter directly
                RF_NETWORK_WARN("PacketProcessor: Dropping FlatBuffer Type %s from unassociated endpoint %s (not a C2S_JoinRequest and no active player session).",
                    UDP::C2S::EnumNameC2S_UDP_Payload(current_payload_type), sender_endpoint.ToString());
                return std::nullopt;
            }

            // For all other messages that require an existing player session:
            return m_messageDispatcher.DispatchC2SMessage(
                flatbuffer_payload_ptr,
                flatbuffer_payload_size,
                sender_endpoint,
                player // Pass the valid 'player' parameter
            );
        }

    } // namespace Networking
} // namespace RiftForged