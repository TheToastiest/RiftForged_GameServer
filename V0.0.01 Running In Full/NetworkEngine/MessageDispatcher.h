#pragma once

#include <cstdint>  // For uint8_t
#include <optional> // For std::optional

// Include necessary definitions for types used in the public interface.
// Assuming your flat file structure where these are findable by the compiler:
#include "GamePacketHeader.h" // Defines RiftForged::Networking::GamePacketHeader and MessageType
#include "NetworkEndpoint.h"  // Defines RiftForged::Networking::NetworkEndpoint
#include "NetworkCommon.h"    // Defines RiftForged::Networking::S2C_Response and includes <optional>

// Forward declare the specific C2S message handler classes
// These classes are only used as references in the constructor and as members,
// so forward declarations are fine here. Their full definitions will be needed in MessageDispatcher.cpp.
namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                class MovementMessageHandler;
                class RiftStepMessageHandler;
                class AbilityMessageHandler;
                class PingMessageHandler;
                class TurnMessageHandler;
            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged


namespace RiftForged {
    namespace Networking {

        class MessageDispatcher {
        public:
            MessageDispatcher(
                UDP::C2S::MovementMessageHandler& movementHandler,
                UDP::C2S::RiftStepMessageHandler& riftStepHandler,
                UDP::C2S::AbilityMessageHandler& abilityHandler,
                UDP::C2S::PingMessageHandler& pingHandler,
				UDP::C2S::TurnMessageHandler& turnHandler
            );

            // DispatchC2SMessage now has all its parameter and return types fully defined
            // via the includes above.
            std::optional<RiftForged::Networking::S2C_Response> DispatchC2SMessage(
                const GamePacketHeader& header,
                const uint8_t* flatbuffer_payload_ptr,
                int flatbuffer_payload_size,
                const NetworkEndpoint& sender_endpoint
            );

        private:
            UDP::C2S::MovementMessageHandler& m_movementHandler;
            UDP::C2S::RiftStepMessageHandler& m_riftStepHandler;
            UDP::C2S::AbilityMessageHandler& m_abilityHandler;
            UDP::C2S::PingMessageHandler& m_pingHandler;
			UDP::C2S::TurnMessageHandler& m_turnHandler;
        };

    } // namespace Networking
} // namespace RiftForged