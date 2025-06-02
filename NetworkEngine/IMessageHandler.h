// File: IMessageHandler.h
// RiftForged Game Development
// Purpose: Defines the interface for processing application-level messages
//          received from the network. This layer is responsible for
//          deserializing and acting upon game-specific commands and data.

#pragma once

#include "NetworkEndpoint.h"  // For NetworkEndpoint (sender information)
#include "GamePacketHeader.h" // For MessageType enum (defines what kind of message this is)
#include <vector>             // For std::vector (though payload is raw pointer)
#include <cstdint>            // For uint8_t, uint16_t
#include <optional>           // For std::optional (to return a potential response)

// Forward declaration for the S2C_Response structure.
// Ensure S2C_Response is defined in a way that's accessible when this is compiled,
// often in a shared header like "NetworkCommon.h" or similar.
// If it's simple enough, it could also be defined directly in a shared scope.
namespace RiftForged {
    namespace Networking {
        // This struct would define what information is needed for the PacketHandler
        // to send a response. Example:
        // struct S2C_Response {
        //     bool broadcast; // True if this response goes to all clients
        //     NetworkEndpoint specific_recipient; // Valid if broadcast is false
        //     MessageType messageType; // The type of the S2C message to send
        //     std::vector<uint8_t> data; // The payload of the S2C message
        // };
        struct S2C_Response; // Assuming it's defined elsewhere, e.g., NetworkCommon.h
    } // namespace Networking
} // namespace RiftForged


namespace RiftForged {
    namespace Networking {

        class IMessageHandler {
        public:
            // Virtual destructor is important for interfaces to ensure proper cleanup
            // when objects are deleted via a base class pointer.
            virtual ~IMessageHandler() = default;

            /**
             * @brief Processes a fully validated application-level message payload.
             * This method is called by the PacketHandler after it has processed
             * network packet headers and reliability information.
             *
             * @param sender The network endpoint from which the message originated.
             * @param messageId The type of the message, as defined in your MessageType enum
             * (from GamePacketHeader.h). This tells the MessageHandler
             * how to interpret the flatbufferData.
             * @param flatbufferData Pointer to the start of the FlatBuffer data, which is
             * the actual application-level payload.
             * @param flatbufferSize Size of the FlatBuffer data in bytes.
             * @return std::optional<S2C_Response> - If the processing of this message
             * requires a direct response to be sent back (either to the sender
             * or broadcast), this structure should be populated and returned.
             * If no direct response is needed, std::nullopt can be returned.
             * The PacketHandler will be responsible for taking this S2C_Response
             * and using its own send methods to dispatch the network packet(s).
             */
            virtual std::optional<S2C_Response> ProcessApplicationMessage(
                const NetworkEndpoint& sender,
                MessageType messageId,      // Using the MessageType enum from GamePacketHeader.h
                const uint8_t* flatbufferData,
                uint16_t flatbufferSize) = 0;
        };

    } // namespace Networking
} // namespace RiftForged