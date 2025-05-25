// File: PacketProcessor.h (Updated)
#pragma once

#include <cstdint>    // For uint8_t etc.
#include <optional>     // For std::optional

#include "NetworkCommon.h"    // For S2C_Response, NetworkEndpoint
#include "GamePacketHeader.h" // For GamePacketHeader

// Forward declarations
namespace RiftForged {
    namespace Networking {
        class MessageDispatcher; //
    }
    namespace Gameplay {         // <<< ADDED this forward declaration
        class GameplayEngine;    // Forward declare GameplayEngine
    }
    // Optional: If ProcessIncomingRawPacket or other methods in this header were to take ActivePlayer*
    // namespace GameLogic {
    //     class ActivePlayer;
    // }
}

namespace RiftForged {
    namespace Networking {

        class PacketProcessor {
        public:
            // <<< MODIFIED Constructor: Now also takes a GameplayEngine reference >>>
            PacketProcessor(MessageDispatcher& dispatcher,
                RiftForged::Gameplay::GameplayEngine& gameplayEngine); //

            // This is the main entry point for processing a raw packet.
            // It returns an optional S2C_Response.
            std::optional<RiftForged::Networking::S2C_Response> ProcessIncomingRawPacket( //
                const char* raw_buffer,
                int raw_length,
                const NetworkEndpoint& sender_endpoint
            );

        private:
            MessageDispatcher& m_messageDispatcher; //
            RiftForged::Gameplay::GameplayEngine& m_gameplayEngine; // <<< ADDED GameplayEngine reference member

            // Potentially a reference to your ReliabilityManager if ACK processing happens here:
            // ReliabilityManager& m_reliabilityManager; //
        };

    } // namespace Networking
} // namespace RiftForged