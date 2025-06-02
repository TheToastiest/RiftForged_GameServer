    // File: NetworkEngine/PacketProcessor.h (Ensure GameServerEngine dependency)
    #pragma once

    #include <cstdint>
    #include <optional>

    #include "NetworkCommon.h"
    #include "GamePacketHeader.h"
    #include "IMessageHandler.h"

    namespace RiftForged {
        namespace Networking { class MessageDispatcher; }
        namespace Server { class GameServerEngine; } // Forward declare
        // namespace Gameplay { class GameplayEngine; } // GameplayEngine no longer a direct dependency of PacketProcessor for this flow

        namespace Networking {
            class PacketProcessor : public IMessageHandler {
            public:
                PacketProcessor(MessageDispatcher& dispatcher,
                    RiftForged::Server::GameServerEngine& gameServerEngine); // GameServerEngine is key

                std::optional<S2C_Response> ProcessApplicationMessage(
                    const NetworkEndpoint& sender_endpoint,
                    MessageType messageId,
                    const uint8_t* flatbuffer_payload_ptr,
                    uint16_t flatbuffer_payload_size
                ) override;

            private:
                MessageDispatcher& m_messageDispatcher;
                RiftForged::Server::GameServerEngine& m_gameServerEngine; // Stores GameServerEngine
            };
        } // namespace Networking
    } // namespace RiftForged