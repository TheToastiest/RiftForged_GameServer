// File: IMessageHandler.h
// RiftForged Game Development
// Purpose: Defines the interface for processing application-level messages
//          received from the network. This layer is responsible for
//          deserializing and acting upon game-specific commands and data.

#pragma once

#include "NetworkEndpoint.h"    // For NetworkEndpoint (sender information)
// REMOVED: #include "GamePacketHeader.h" // No longer defines application-level MessageType.

#include <cstdint>              // For uint8_t, uint16_t
#include <optional>             // For std::optional (to return a potential response)

// Include the header that defines S2C_Response.
// This struct now holds the FlatBuffer S2C payload type and serialized data.
#include "NetworkCommon.h" // Assuming S2C_Response is defined here.

// Forward declaration for the ActivePlayer class, as MessageHandler operates on game state.
namespace RiftForged {
    namespace GameLogic {
        struct ActivePlayer;
    }
}

namespace RiftForged {
    namespace Networking {

        // IMessageHandler is an abstract interface that concrete message processing classes
        // (like MessageDispatcher) must implement. It provides the entry point for
        // application-level data from the network layer.
        class IMessageHandler {
        public:
            // A virtual destructor is essential for base classes in C++ to ensure that
            // derived class destructors are called correctly when deleting objects
            // through a base class pointer.
            virtual ~IMessageHandler() = default;

            /**
             * @brief Processes a raw, validated application-level message payload.
             * This method is called by the UDPPacketHandler after it has handled
             * network packet headers and reliability concerns. The payload is expected
             * to be a FlatBuffer.
             *
             * @param sender The network endpoint from which the message originated.
             * @param flatbuffer_payload_ptr Pointer to the start of the raw FlatBuffer data.
             * @param flatbuffer_payload_size Size of the raw FlatBuffer data in bytes.
             * @param player A pointer to the ActivePlayer associated with the sender.
             * This can be `nullptr` for initial connection messages (like a `JoinRequest`)
             * if the `ActivePlayer` object is created *after* processing that specific message.
             * @return `std::optional<S2C_Response>` - If processing this message requires a direct response
             * to be sent back (either to the sender or as a broadcast), this structure should be
             * populated and returned. If no direct response is needed, `std::nullopt` is returned.
             * The `UDPPacketHandler` will be responsible for taking this `S2C_Response`
             * and dispatching it over the network.
             */
            virtual std::optional<S2C_Response> ProcessApplicationMessage(
                const NetworkEndpoint& sender,
                const uint8_t* flatbuffer_payload_ptr,
                uint16_t flatbuffer_payload_size,
                RiftForged::GameLogic::ActivePlayer* player
            ) = 0; // Declared as a pure virtual function, making IMessageHandler an abstract class.
        };

    } // namespace Networking
} // namespace RiftForged