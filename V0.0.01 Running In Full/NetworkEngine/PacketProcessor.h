#pragma once

#include <cstdint>  // For uint8_t etc.
#include <optional>   // For std::optional

// Assuming NetworkCommon.h is in a path findable like this
// (e.g., if PacketManagement and Networking are sibling directories)
// Adjust relative path as needed for your actual structure.
#include "NetworkCommon.h" // For S2C_Response, NetworkEndpoint
#include "GamePacketHeader.h"     // For GamePacketHeader

// Forward declaration for MessageDispatcher
namespace RiftForged {
    namespace Networking {
        class MessageDispatcher; // Defined in Dispatch/MessageDispatcher.h
    }
}

namespace RiftForged {
    namespace Networking {

        class PacketProcessor {
        public:
            // Constructor takes a reference to the MessageDispatcher
            PacketProcessor(MessageDispatcher& dispatcher);

            // This is the main entry point for processing a raw packet.
            // It now returns an optional S2C_Response.
            std::optional<RiftForged::Networking::S2C_Response> ProcessIncomingRawPacket(
                const char* raw_buffer,
                int raw_length,
                const NetworkEndpoint& sender_endpoint
            );

        private:
            MessageDispatcher& m_messageDispatcher;
            // Potentially a reference to your ReliabilityManager if ACK processing happens here:
            // ReliabilityManager& m_reliabilityManager; 
        };

    } // namespace Networking
} // namespace RiftForged