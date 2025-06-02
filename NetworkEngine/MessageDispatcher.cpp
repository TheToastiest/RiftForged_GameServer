// File: UDPServer/PacketManagement/Handlers_C2S/MessageDispatcher.cpp (Corrected with FlatBuffer types)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "MessageDispatcher.h"
#include <optional>
#include "NetworkCommon.h" // For S2C_Response

// Generated FlatBuffer headers
// This includes the C2S_UDP_Payload enum and Root_C2S_UDP_Message,
// as well as individual message types like C2S_MovementInputMsg.
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
// Common types like Vec3 are included via the above header.

#include "../Gameplay/ActivePlayer.h" // For ActivePlayer struct
#include "../Utils/Logger.h"    // For RF_NETWORK_... macros

// Specific Message Handler includes (ensure these are correct)
#include "MovementMessageHandler.h"
#include "RiftStepMessageHandler.h"
#include "AbilityMessageHandler.h"
#include "PingMessageHandler.h"
#include "TurnMessageHandler.h"
#include "BasicAttackMessageHandler.h"

// GamePacketHeader.h (for MessageType enum) and NetworkEndpoint.h
// are included via MessageDispatcher.h

namespace RiftForged {
    namespace Networking {

        // Constructor (assuming it's the same as previously discussed, injecting handlers)
        MessageDispatcher::MessageDispatcher(
            UDP::C2S::MovementMessageHandler& movementHandler,
            UDP::C2S::RiftStepMessageHandler& riftStepHandler,
            UDP::C2S::AbilityMessageHandler& abilityHandler,
            UDP::C2S::PingMessageHandler& pingHandler,
            UDP::C2S::TurnMessageHandler& turnHandler,
            UDP::C2S::BasicAttackMessageHandler& basicAttackHandler)
            : m_movementHandler(movementHandler),
            m_riftStepHandler(riftStepHandler),
            m_abilityHandler(abilityHandler),
            m_pingHandler(pingHandler),
            m_turnHandler(turnHandler),
            m_basicAttackHandler(basicAttackHandler) {
            RF_NETWORK_INFO("MessageDispatcher: Initialized with all handlers.");
        }

        std::optional<S2C_Response> MessageDispatcher::DispatchC2SMessage(
            RiftForged::Networking::MessageType messageId,      // From GamePacketHeader
            const uint8_t* flatbuffer_payload_ptr,
            uint16_t flatbuffer_payload_size,
            const NetworkEndpoint& sender_endpoint,
            RiftForged::GameLogic::ActivePlayer* player) {

            if (!player) {
                RF_NETWORK_ERROR("MessageDispatcher: Null player object provided for dispatch from {}. MessageType: {}",
                    sender_endpoint.ToString(), EnumNameMessageType(messageId));
                return std::nullopt;
            }

            if (flatbuffer_payload_size == 0 && messageId != MessageType::C2S_Ping) {
                // C2S_Ping might have an empty table but still be valid if its presence is the message.
                // For other types, an empty payload for the Root_C2S_UDP_Message might be an issue.
                RF_NETWORK_WARN("MessageDispatcher: Zero payload size for MessageType {} ({}) from {}. Verifier will check validity.",
                    static_cast<int>(messageId), EnumNameMessageType(messageId), sender_endpoint.ToString());
            }

            flatbuffers::Verifier verifier(flatbuffer_payload_ptr, static_cast<size_t>(flatbuffer_payload_size));
            if (!RiftForged::Networking::UDP::C2S::VerifyRoot_C2S_UDP_MessageBuffer(verifier)) {
                RF_NETWORK_WARN("MessageDispatcher: Invalid Root_C2S_UDP_Message FlatBuffer from [{}]. Header indicates MessageType: {} ({}), Size: {}",
                    sender_endpoint.ToString(), static_cast<int>(messageId), EnumNameMessageType(messageId), flatbuffer_payload_size);
                return std::nullopt;
            }

            auto root_message = RiftForged::Networking::UDP::C2S::GetRoot_C2S_UDP_Message(flatbuffer_payload_ptr);
            if (!root_message || !root_message->payload()) {
                RF_NETWORK_WARN("MessageDispatcher: GetRoot_C2S_UDP_Message null or its payload union is null from [{}]. Header indicates MessageType: {} ({})",
                    sender_endpoint.ToString(), static_cast<int>(messageId), EnumNameMessageType(messageId));
                return std::nullopt;
            }

            // This is the type discriminator from within the FlatBuffer C2S root message union itself.
            UDP::C2S::C2S_UDP_Payload payload_type_from_union = root_message->payload_type();
            std::optional<S2C_Response> handler_response = std::nullopt;

            RF_NETWORK_TRACE("MessageDispatcher: Dispatching MessageType: {} ({}) / UnionType: {} ({}) from [{}]",
                static_cast<int>(messageId), EnumNameMessageType(messageId),
                static_cast<int>(payload_type_from_union), UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union),
                sender_endpoint.ToString());

            // Switch on the MessageType from the GamePacketHeader
            switch (messageId) {
            case MessageType::C2S_MovementInput:
                // Verify consistency: the outer header type should match the inner FlatBuffer union type
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_MovementInput) {
                    // Get the specific message type from the union
                    auto msg = root_message->payload_as_MovementInput();
                    if (msg) {
                        // Call the specific handler
                        handler_response = m_movementHandler.Process(sender_endpoint, player, msg);
                    }
                    else {
                        RF_NETWORK_WARN("Dispatcher: Payload_as_MovementInput failed (null message table) for [{}]", sender_endpoint.ToString());
                    }
                }
                else { goto type_mismatch_error; } // Use goto for common mismatch logging
                break;

            case MessageType::C2S_TurnIntent:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_TurnIntent) {
                    auto msg = root_message->payload_as_TurnIntent();
                    if (msg) { handler_response = m_turnHandler.Process(sender_endpoint, player, msg); }
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_TurnIntent failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch_error; }
                break;

            case MessageType::C2S_BasicAttackIntent:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_BasicAttackIntent) {
                    auto msg = root_message->payload_as_BasicAttackIntent();
                    if (msg) { handler_response = m_basicAttackHandler.Process(sender_endpoint, player, msg); }
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_BasicAttackIntent failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch_error; }
                break;

            case MessageType::C2S_RiftStepActivation:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_RiftStepActivation) {
                    auto msg = root_message->payload_as_RiftStepActivation();
                    if (msg) { handler_response = m_riftStepHandler.Process(sender_endpoint, player, msg); }
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_RiftStepActivation failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch_error; }
                break;

            case MessageType::C2S_UseAbility:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_UseAbility) {
                    auto msg = root_message->payload_as_UseAbility();
                    if (msg) { handler_response = m_abilityHandler.Process(sender_endpoint, player, msg); } // Ensure AbilityMessageHandler::Process takes player
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_UseAbility failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch_error; }
                break;

            case MessageType::C2S_Ping:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_Ping) {
                    auto msg = root_message->payload_as_Ping();
                    if (msg) { handler_response = m_pingHandler.Process(sender_endpoint, player, msg); } // PingMessageHandler::Process didn't take player in your example
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_Ping failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch_error; }
                break;

            default:
                RF_NETWORK_WARN("MessageDispatcher: Unknown or unhandled MessageType in dispatch: {} ({}) from [{}]",
                    static_cast<int>(messageId), EnumNameMessageType(messageId), sender_endpoint.ToString());
                break;
            }

            if (handler_response.has_value()) {
                RF_NETWORK_DEBUG("MessageDispatcher: Handler for MessageType {} returned S2C_Response. Recipient: [{}], Broadcast: {}, ResponseMsgType: {}",
                    EnumNameMessageType(messageId),
                    handler_response->specific_recipient.ToString(),
                    handler_response->broadcast,
                    EnumNameMessageType(handler_response->messageType));
            }
            else {
                RF_NETWORK_TRACE("MessageDispatcher: No S2C_Response from handler for MessageType {} from [{}]",
                    EnumNameMessageType(messageId), sender_endpoint.ToString());
            }
            return handler_response;

        type_mismatch_error: // Label for common type mismatch error logging
            RF_NETWORK_WARN("MessageDispatcher: Header/Payload type MISMATCH from [{}]. Header indicates MessageType: {} ({}), but FlatBuffer Union Type was: {} ({}). Discarding.",
                sender_endpoint.ToString(),
                static_cast<int>(messageId), EnumNameMessageType(messageId),
                static_cast<int>(payload_type_from_union), UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union)
            );
            return std::nullopt;
        }

    } // namespace Networking
} // namespace RiftForged