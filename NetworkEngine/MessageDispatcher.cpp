// File: UDPServer/PacketManagement/Handlers_C2S/MessageDispatcher.cpp
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "MessageDispatcher.h"
#include <iostream> // Replace with your actual logging system
#include <optional>
#include "NetworkCommon.h"

// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.
// #include "riftforged_common_types_generated.h" // is included by riftforged_udp_messages_generated.h

#include "../Gameplay/ActivePlayer.h" // Adjust path as needed

// Specific Message Handler includes (ensure paths are correct)
#include "MovementMessageHandler.h"
#include "RiftStepMessageHandler.h"
#include "AbilityMessageHandler.h"
#include "PingMessageHandler.h"
#include "TurnMessageHandler.h"
#include "BasicAttackMessageHandler.h"
#include "../Utils/Logger.h"

// Already included via MessageDispatcher.h, but good for clarity if working here:
// #include "../Headers/GamePacketHeader.h"
// #include "../../Networking/Clients/NetworkEndpoint.h"


namespace RiftForged {
    namespace Networking {

        MessageDispatcher::MessageDispatcher(
            UDP::C2S::MovementMessageHandler& movementHandler,
            UDP::C2S::RiftStepMessageHandler& riftStepHandler,
            UDP::C2S::AbilityMessageHandler& abilityHandler,
            UDP::C2S::PingMessageHandler& pingHandler,
            UDP::C2S::TurnMessageHandler& turnHandler, // Add new handler
			UDP::C2S::BasicAttackMessageHandler& basicAttackHandler) // Added for BasicAttack
            : m_movementHandler(movementHandler),
            m_riftStepHandler(riftStepHandler),
            m_abilityHandler(abilityHandler),
            m_pingHandler(pingHandler),
            m_turnHandler(turnHandler), // Initialize new handler
			m_basicAttackHandler(basicAttackHandler) // Added for BasicAttack
        {
            RF_NETWORK_INFO("MessageDispatcher: Initialized with all handlers.");
        }

		// DispatchC2SMessage now takes an ActivePlayer* parameter
        std::optional<S2C_Response> MessageDispatcher::DispatchC2SMessage(
            const GamePacketHeader& header,
            const uint8_t* flatbuffer_payload_ptr,
            int flatbuffer_payload_size,
            const NetworkEndpoint& sender_endpoint,
            RiftForged::GameLogic::ActivePlayer* player) { // <<< player pointer received
                
            if (!player) {
                RF_NETWORK_ERROR("MessageDispatcher: Null player object provided for dispatch from {}. MessageType: {}",
                    sender_endpoint.ToString(), static_cast<int>(header.messageType));
                return std::nullopt;
            }

            if (flatbuffer_payload_size <= 0 &&
                header.messageType != MessageType::C2S_Ping) { // Ping might be okay with minimal or no specific payload data if just using header
                RF_NETWORK_WARN("MessageDispatcher: Zero or negative payload size for MessageType {} from {}.",
                    static_cast<int>(header.messageType), sender_endpoint.ToString());
                // Depending on the message, this might be an error or expected.
                // For now, let specific handlers decide if they need a payload.
            }

            flatbuffers::Verifier verifier(flatbuffer_payload_ptr, static_cast<size_t>(flatbuffer_payload_size));
            if (!RiftForged::Networking::UDP::C2S::VerifyRoot_C2S_UDP_MessageBuffer(verifier)) {
                RF_NETWORK_WARN("MessageDispatcher: Invalid Root_C2S_UDP_Message FlatBuffer from [{}]. HeaderType: {}, Size: {}",
                    sender_endpoint.ToString(), static_cast<int>(header.messageType), flatbuffer_payload_size);
                return std::nullopt;
            }

            auto root_message = RiftForged::Networking::UDP::C2S::GetRoot_C2S_UDP_Message(flatbuffer_payload_ptr);
            if (!root_message || !root_message->payload()) { // Also check if payload itself is null
                RF_NETWORK_WARN("MessageDispatcher: GetRoot_C2S_UDP_Message null or payload null from [{}]. HeaderType: {}",
                    sender_endpoint.ToString(), static_cast<int>(header.messageType));
                return std::nullopt;
            }

            UDP::C2S::C2S_UDP_Payload payload_type_from_union = root_message->payload_type();
            std::optional<S2C_Response> handler_response = std::nullopt;

            // Log the dispatch attempt
            RF_NETWORK_TRACE("Dispatching C2S MsgType: {} (Header) / {} (Union) from [{}]",
                static_cast<int>(header.messageType),
                static_cast<int>(payload_type_from_union),
                sender_endpoint.ToString());

            switch (header.messageType) {
            case MessageType::C2S_MovementInput:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload_MovementInput) {
                    auto msg = root_message->payload_as_MovementInput();
                    if (msg) { handler_response = m_movementHandler.Process(sender_endpoint, player, msg); }
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_MovementInput failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch; } // Use goto for common mismatch logging
                break;

            case MessageType::C2S_TurnIntent: // New Case
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload_TurnIntent) {
                    auto msg = root_message->payload_as_TurnIntent();
                    if (msg) { handler_response = m_turnHandler.Process(sender_endpoint, player, msg); }
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_TurnIntent failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch; }
                break;

            case MessageType::C2S_BasicAttackIntent:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload_BasicAttackIntent) {
                    auto msg = root_message->payload_as_BasicAttackIntent();
                    if (msg) {
                        handler_response = m_basicAttackHandler.Process(sender_endpoint, player, msg);
                    }
                    else {
                        RF_NETWORK_WARN("Dispatcher: Payload_as_BasicAttackIntent failed for [{}]", sender_endpoint.ToString());
                    }
                }
                else {
                    // Log type mismatch using the detailed RF_NETWORK_WARN from response #133
                    RF_NETWORK_WARN("MessageDispatcher: Header/Payload type mismatch for C2S_BasicAttackIntent from {}. HeaderType: {} ({}), UnionType: {} ({})",
                        sender_endpoint.ToString(),
                        static_cast<int>(header.messageType), RiftForged::Networking::EnumNameMessageType(header.messageType),
                        static_cast<int>(payload_type_from_union), RiftForged::Networking::UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union));
                }
                break;

            case RiftForged::Networking::MessageType::C2S_RiftStepActivation: // Use the MessageType for the TABLE
                // Check if the payload type in the FlatBuffer union matches the expected TABLE type
                if (payload_type_from_union == RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_RiftStepActivation) {
                    // Get the C2S_RiftStepActivationMsg table from the union
                    auto msg_table = root_message->payload_as_RiftStepActivation();

                    if (msg_table) {
                        // Now that you have the msg_table, you can pass it to the handler.
                        // The handler's Process method should be defined to take:
                        // const RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsg* message
                        handler_response = m_riftStepHandler.Process(sender_endpoint, player, msg_table);
                    }
                    else {
                        RF_NETWORK_WARN("Dispatcher: payload_as_RiftStepActivation() returned null even though types matched for C2S_RiftStepActivation from {}", sender_endpoint.ToString());
                    }
                }
                else {
                    // This is the "Header/Payload type mismatch" log you were seeing.
                    // It means header.messageType said C2S_RiftStepActivation, 
                    // but the FlatBuffer union's payload_type was something else.
                    RF_NETWORK_WARN("MessageDispatcher: Header/Payload type mismatch for MessageType::C2S_RiftStepActivation from [{}]. Header indicates {}, but FlatBuffer Union Type was {} (expected {} for C2S_UDP_Payload_RiftStepActivation).",
                        sender_endpoint.ToString(),
                        RiftForged::Networking::EnumNameMessageType(header.messageType), // Helper for your enum
                        RiftForged::Networking::UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union), // flatc generated
                        RiftForged::Networking::UDP::C2S::EnumNameC2S_UDP_Payload(RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_RiftStepActivation)
                    );
                }
                break;

            case MessageType::C2S_UseAbility:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload_UseAbility) {
                    auto msg = root_message->payload_as_UseAbility();
                    if (msg) { handler_response = m_abilityHandler.Process(sender_endpoint, msg); }
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_UseAbility failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch; }
                break;

            case MessageType::C2S_Ping:
                if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload_Ping) {
                    auto msg = root_message->payload_as_Ping();
                    if (msg) { handler_response = m_pingHandler.Process(sender_endpoint, msg); }
                    else { RF_NETWORK_WARN("Dispatcher: Payload_as_Ping failed for [{}]", sender_endpoint.ToString()); }
                }
                else { goto type_mismatch; }
                break;

            default:
                RF_NETWORK_WARN("MessageDispatcher: Unknown or unhandled MessageType in header: {} from [{}]",
                    static_cast<int>(header.messageType), sender_endpoint.ToString());
                break;
            }

            // Log the outcome of the dispatch (what specific_recipient is set to by the handler)
            if (handler_response.has_value()) {
                RF_NETWORK_DEBUG("MessageDispatcher: Handler returned S2C_Response. Recipient: [{}], Broadcast: {}, MsgType: {}",
                    handler_response->specific_recipient.ToString(),
                    handler_response->broadcast,
                    static_cast<int>(handler_response->messageType));
            }
            else {
                // This will log if a handler intentionally returned nullopt (e.g. MovementHandler)
                // or if there was a type mismatch / unhandled message type.
                RF_NETWORK_TRACE("MessageDispatcher: No S2C_Response returned from handler for HeaderType {} from [{}]",
                    static_cast<int>(header.messageType), sender_endpoint.ToString());
            }
            return handler_response;

        type_mismatch: // Common logging for type mismatches
            RF_NETWORK_WARN("MessageDispatcher: Header/Payload type mismatch for MsgType {} from [{}]. Header indicates {}, but FlatBuffer Union was {}.",
                static_cast<int>(header.messageType),
                sender_endpoint.ToString(),
                RiftForged::Networking::EnumNameMessageType(header.messageType), // Assuming you have EnumNameMessageType
                RiftForged::Networking::UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union)
            );
            return std::nullopt;
        }

    } // namespace Networking
} // namespace RiftForged