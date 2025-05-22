#include "MessageDispatcher.h"
#include <iostream> // Replace with your actual logging system
#include <optional>
#include "NetworkCommon.h"

// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.
// #include "riftforged_common_types_generated.h" // is included by riftforged_udp_messages_generated.h

// Specific Message Handler includes (ensure paths are correct)
#include "MovementMessageHandler.h"
#include "RiftStepMessageHandler.h"
#include "AbilityMessageHandler.h"
#include "PingMessageHandler.h"

// Already included via MessageDispatcher.h, but good for clarity if working here:
// #include "../Headers/GamePacketHeader.h"
// #include "../../Networking/Clients/NetworkEndpoint.h"


namespace RiftForged {
    namespace Networking {

        // Constructor remains the same
        MessageDispatcher::MessageDispatcher(
            UDP::C2S::MovementMessageHandler& movementHandler,
            UDP::C2S::RiftStepMessageHandler& riftStepHandler,
            UDP::C2S::AbilityMessageHandler& abilityHandler,
            UDP::C2S::PingMessageHandler& pingHandler)
            : m_movementHandler(movementHandler),
            m_riftStepHandler(riftStepHandler),
            m_abilityHandler(abilityHandler),
            m_pingHandler(pingHandler) {
            std::cout << "MessageDispatcher: Initialized." << std::endl;
        }

        std::optional<S2C_Response> MessageDispatcher::DispatchC2SMessage(
            const GamePacketHeader& header,
            const uint8_t* flatbuffer_payload_ptr,
            int flatbuffer_payload_size,
            const NetworkEndpoint& sender_endpoint) {

            flatbuffers::Verifier verifier(flatbuffer_payload_ptr, static_cast<size_t>(flatbuffer_payload_size));
            if (!RiftForged::Networking::UDP::C2S::VerifyRoot_C2S_UDP_MessageBuffer(verifier)) {
                std::cerr << "MessageDispatcher: PID(0) IP(" << sender_endpoint.ToString()
                    << ") - Invalid Root_C2S_UDP_Message FlatBuffer. MessageType from header: "
                    << static_cast<int>(header.messageType) << ". Size: " << flatbuffer_payload_size << std::endl;
                return std::nullopt;
            }

            auto root_message = RiftForged::Networking::UDP::C2S::GetRoot_C2S_UDP_Message(flatbuffer_payload_ptr);
            if (!root_message) {
                std::cerr << "MessageDispatcher: PID(0) IP(" << sender_endpoint.ToString()
                    << ") - GetRoot_C2S_UDP_Message returned null. MessageType from header: "
                    << static_cast<int>(header.messageType) << std::endl;
                return std::nullopt;
            }

            RiftForged::Networking::UDP::C2S::C2S_UDP_Payload payload_type_from_union = root_message->payload_type();

            switch (header.messageType) {
            case MessageType::C2S_MovementInput:
                if (payload_type_from_union == RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_MovementInput) {
                    auto msg = root_message->payload_as_MovementInput();
                    return m_movementHandler.Process(sender_endpoint, msg);
                }
                else {
                    std::cerr << "MessageDispatcher: Header/Payload type mismatch for C2S_MovementInput from " << sender_endpoint.ToString() << std::endl;
                }
                break;

            case MessageType::C2S_RiftStepActivation: // <<<< UPDATED ENUM MEMBER NAME
                if (payload_type_from_union == RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_RiftStepActivation) { // <<<< UPDATED UNION TYPE
                    auto msg = root_message->payload_as_RiftStepActivation(); // <<<< UPDATED ACCESSOR
                    return m_riftStepHandler.Process(sender_endpoint, msg);
                }
                else {
                    std::cerr << "MessageDispatcher: Header/Payload type mismatch for C2S_RiftStepActivation from " << sender_endpoint.ToString() << std::endl;
                }
                break;

            case MessageType::C2S_UseAbility:
                if (payload_type_from_union == RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_UseAbility) {
                    auto msg = root_message->payload_as_UseAbility();
                    return m_abilityHandler.Process(sender_endpoint, msg);
                }
                else {
                    std::cerr << "MessageDispatcher: Header/Payload type mismatch for C2S_UseAbility from " << sender_endpoint.ToString() << std::endl;
                }
                break;

            case MessageType::C2S_Ping:
                if (payload_type_from_union == RiftForged::Networking::UDP::C2S::C2S_UDP_Payload_Ping) {
                    auto msg = root_message->payload_as_Ping();
                    return m_pingHandler.Process(sender_endpoint, msg);
                }
                else {
                    std::cerr << "MessageDispatcher: Header/Payload type mismatch for C2S_Ping from " << sender_endpoint.ToString() << std::endl;
                }
                break;

            default:
                std::cerr << "MessageDispatcher: Unknown or unhandled MessageType in header: "
                    << static_cast<int>(header.messageType) << " from " << sender_endpoint.ToString() << std::endl;
                break;
            }
            return std::nullopt; // Default if a case didn't return or type mismatch
        }

    } // namespace Networking
} // namespace RiftForged