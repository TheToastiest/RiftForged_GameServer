// File: UDPServer/PacketManagement/Handlers_C2S/MessageDispatcher.cpp
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "MessageDispatcher.h"
#include <optional>

// FlatBuffer related includes
#include "flatbuffers/flatbuffers.h" // For flatbuffers::Verifier, GetRoot, etc.
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
// This includes RiftForged::Networking::UDP::C2S::C2S_UDP_Payload,
// Root_C2S_UDP_Message, and specific C2S message types.

// Include the S2C FlatBuffers generated header to get S2C_UDP_Payload and its EnumName function.
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // Assuming this path

#include "NetworkCommon.h"          // For S2C_Response (now using FB S2C payload type)
#include "../Gameplay/ActivePlayer.h" // For ActivePlayer struct
#include "../Utils/Logger.h"        // For RF_NETWORK_... macros

// Specific Message Handler includes
#include "MovementMessageHandler.h"
#include "RiftStepMessageHandler.h"
#include "AbilityMessageHandler.h"
#include "PingMessageHandler.h"
#include "TurnMessageHandler.h"
#include "BasicAttackMessageHandler.h"
#include "JoinRequestMessageHandler.h"


namespace RiftForged {
    namespace Networking {

        // Constructor implementation
        MessageDispatcher::MessageDispatcher(
            UDP::C2S::MovementMessageHandler& movementHandler,
            UDP::C2S::RiftStepMessageHandler& riftStepHandler,
            UDP::C2S::AbilityMessageHandler& abilityHandler,
            UDP::C2S::PingMessageHandler& pingHandler,
            UDP::C2S::TurnMessageHandler& turnHandler,
            UDP::C2S::BasicAttackMessageHandler& basicAttackHandler,
            UDP::C2S::JoinRequestMessageHandler& joinRequestHandler,
            RiftForged::Utils::Threading::TaskThreadPool* taskPool
        )
            : m_movementHandler(movementHandler),
            m_riftStepHandler(riftStepHandler),
            m_abilityHandler(abilityHandler),
            m_pingHandler(pingHandler),
            m_turnHandler(turnHandler),
            m_basicAttackHandler(basicAttackHandler),
            m_joinRequestHandler(joinRequestHandler),
            m_taskThreadPool(taskPool)
        {
            RF_NETWORK_INFO("MessageDispatcher: Initialized with all handlers.");
        }

        // Dispatch an incoming C2S FlatBuffer message to the appropriate handler.
        std::optional<S2C_Response> MessageDispatcher::DispatchC2SMessage(
            const uint8_t* flatbuffer_payload_ptr,
            uint16_t flatbuffer_payload_size,
            const NetworkEndpoint& sender_endpoint,
            RiftForged::GameLogic::ActivePlayer* player) {

            // --- FlatBuffer Payload Validation ---
            // Perform a basic size check: a FlatBuffer should at least contain its size prefix (4 bytes)
            // and the root table's vtable offset (another 4 bytes).
            if (flatbuffer_payload_size < sizeof(uint32_t) * 2) {
                RF_NETWORK_WARN("MessageDispatcher: FlatBuffer payload size too small (%u bytes) from [%s]. Discarding.",
                    flatbuffer_payload_size, sender_endpoint.ToString());
                return std::nullopt;
            }

            // Use the FlatBuffer Verifier to ensure the buffer's integrity and validity.
            flatbuffers::Verifier verifier(flatbuffer_payload_ptr, static_cast<size_t>(flatbuffer_payload_size));
            if (!RiftForged::Networking::UDP::C2S::VerifyRoot_C2S_UDP_MessageBuffer(verifier)) {
                RF_NETWORK_WARN("MessageDispatcher: Invalid Root_C2S_UDP_Message FlatBuffer from [%s]. Size: %u. Discarding.",
                    sender_endpoint.ToString(), flatbuffer_payload_size);
                return std::nullopt;
            }

            // Get the root message. This provides a view into the FlatBuffer data without deserializing.
            auto root_message = RiftForged::Networking::UDP::C2S::GetRoot_C2S_UDP_Message(flatbuffer_payload_ptr);
            if (!root_message || !root_message->payload()) {
                // If GetRoot returned null, or the payload union itself is null (e.g., payload_type is NONE)
                RF_NETWORK_WARN("MessageDispatcher: Root_C2S_UDP_Message or its payload union is null from [%s]. Type: %s. Discarding.",
                    sender_endpoint.ToString(), UDP::C2S::EnumNameC2S_UDP_Payload(root_message ? root_message->payload_type() : UDP::C2S::C2S_UDP_Payload_NONE));
                return std::nullopt;
            }

            // The true message type is extracted from the FlatBuffer's internal union discriminator.
            UDP::C2S::C2S_UDP_Payload payload_type_from_union = root_message->payload_type();
            std::optional<S2C_Response> handler_response = std::nullopt;

            RF_NETWORK_TRACE("MessageDispatcher: Dispatching FlatBuffer UnionType: %s (%d) from [%s]",
                UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union),
                static_cast<int>(payload_type_from_union),
                sender_endpoint.ToString());

            // --- Special handling for JoinRequest: It's the only message type that might arrive before a player is established. ---
            if (payload_type_from_union == UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_JoinRequest) {
                auto msg = root_message->payload_as_JoinRequest();
                if (msg) {
                    // The JoinRequestMessageHandler is responsible for creating and associating the player.
                    // It can accept a nullptr 'player' and return an S2C_Response to indicate success/failure.
                    handler_response = m_joinRequestHandler.Process(sender_endpoint, player, msg);
                }
                else {
                    RF_NETWORK_WARN("Dispatcher: Payload_as_JoinRequest failed (null message table) for [%s]", sender_endpoint.ToString());
                }
                // Always return after handling JoinRequest, regardless of whether a player was associated yet.
                // The response (or lack thereof) will be handled by UDPPacketHandler.
                return handler_response;
            }

            // --- For all other message types, a player must exist. ---
            if (!player) {
                RF_NETWORK_ERROR("MessageDispatcher: Null player object provided for dispatch from %s for payload type %s. Discarding message.",
                    sender_endpoint.ToString(), UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union));
                return std::nullopt;
            }

            // Dispatch based on the FlatBuffer's payload type for messages requiring an active player.
            switch (payload_type_from_union) {
            case UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_MovementInput: {
                auto msg = root_message->payload_as_MovementInput();
                if (msg) {
                    handler_response = m_movementHandler.Process(sender_endpoint, player, msg);
                }
                else {
                    RF_NETWORK_WARN("Dispatcher: Payload_as_MovementInput failed (null message table) for [%s]", sender_endpoint.ToString());
                }
                break;
            }
            case UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_TurnIntent: {
                auto msg = root_message->payload_as_TurnIntent();
                if (msg) {
                    handler_response = m_turnHandler.Process(sender_endpoint, player, msg);
                }
                else {
                    RF_NETWORK_WARN("Dispatcher: Payload_as_TurnIntent failed for [%s]", sender_endpoint.ToString());
                }
                break;
            }
            case UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_BasicAttackIntent: {
                auto msg = root_message->payload_as_BasicAttackIntent();
                if (msg) {
                    handler_response = m_basicAttackHandler.Process(sender_endpoint, player, msg);
                }
                else {
                    RF_NETWORK_WARN("Dispatcher: Payload_as_BasicAttackIntent failed for [%s]", sender_endpoint.ToString());
                }
                break;
            }
            case UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_RiftStepActivation: {
                auto msg = root_message->payload_as_RiftStepActivation();
                if (msg) {
                    handler_response = m_riftStepHandler.Process(sender_endpoint, player, msg);
                }
                else {
                    RF_NETWORK_WARN("Dispatcher: Payload_as_RiftStepActivation failed for [%s]", sender_endpoint.ToString());
                }
                break;
            }
            case UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_UseAbility: {
                auto msg = root_message->payload_as_UseAbility();
                if (msg) {
                    handler_response = m_abilityHandler.Process(sender_endpoint, player, msg);
                }
                else {
                    RF_NETWORK_WARN("Dispatcher: Payload_as_UseAbility failed for [%s]", sender_endpoint.ToString());
                }
                break;
            }
            case UDP::C2S::C2S_UDP_Payload::C2S_UDP_Payload_Ping: {
                auto msg = root_message->payload_as_Ping();
                if (msg) {
                    handler_response = m_pingHandler.Process(sender_endpoint, player, msg);
                }
                else {
                    RF_NETWORK_WARN("Dispatcher: Payload_as_Ping failed for [%s]", sender_endpoint.ToString());
                }
                break;
            }
            case UDP::C2S::C2S_UDP_Payload_NONE: {
                // This case indicates that the FlatBuffer's root union is explicitly NONE.
                // This might happen if a client sends an empty Root_C2S_UDP_Message.
                RF_NETWORK_WARN("MessageDispatcher: FlatBuffer Root_C2S_UDP_Message payload type is NONE from [%s]. Discarding.",
                    sender_endpoint.ToString());
                break;
            }
            default:
                // Fallback for any unhandled FlatBuffer payload types.
                RF_NETWORK_WARN("MessageDispatcher: Unknown or unhandled FlatBuffer C2S_UDP_Payload type: %s (%d) from [%s]. Discarding.",
                    UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union),
                    static_cast<int>(payload_type_from_union), sender_endpoint.ToString());
                break;
            }

            // Log the response from the handler.
            if (handler_response.has_value()) {
                RF_NETWORK_DEBUG("MessageDispatcher: Handler for %s returned S2C_Response. Recipient: [%s], Broadcast: %s, ResponseMsgType: %s",
                    UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union), // Log the *incoming* C2S FlatBuffer type
                    handler_response->specific_recipient.ToString(),
                    handler_response->broadcast ? "true" : "false",
                    // Use the S2C FlatBuffers enum name for the *outgoing* response type.
                    UDP::S2C::EnumNameS2C_UDP_Payload(handler_response->flatbuffer_payload_type));
            }
            else {
                RF_NETWORK_TRACE("MessageDispatcher: No S2C_Response from handler for %s from [%s]",
                    UDP::C2S::EnumNameC2S_UDP_Payload(payload_type_from_union),
                    sender_endpoint.ToString());
            }
            return handler_response;
        }

    } // namespace Networking
} // namespace RiftForged