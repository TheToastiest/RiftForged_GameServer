// File: NetworkEngine/PacketProcessor.h (Ensure GameServerEngine dependency)
#pragma once

#include <cstdint>
#include <optional>

#include "NetworkCommon.h"
// #include "GamePacketHeader.h" // REMOVED: MessageType enum is no longer part of this interface
#include "IMessageHandler.h"

namespace RiftForged {
    namespace Networking { class MessageDispatcher; }
    namespace Server { class GameServerEngine; } // Forward declare

    namespace Networking {
        class PacketProcessor : public IMessageHandler {
        public:
            PacketProcessor(MessageDispatcher& dispatcher,
                RiftForged::Server::GameServerEngine& gameServerEngine);

            // UPDATED: Removed MessageType messageId parameter
            std::optional<S2C_Response> ProcessApplicationMessage(
                const NetworkEndpoint& sender_endpoint,
                const uint8_t* flatbuffer_payload_ptr,
                uint16_t flatbuffer_payload_size,
                RiftForged::GameLogic::ActivePlayer* player // Added ActivePlayer parameter
            ) override;

        private:
            MessageDispatcher& m_messageDispatcher;
            RiftForged::Server::GameServerEngine& m_gameServerEngine;
        };
    } // namespace Networking
} // namespace RiftForged