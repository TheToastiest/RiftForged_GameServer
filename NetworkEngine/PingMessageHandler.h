#pragma once
#include "NetworkEndpoint.h" // Adjusted path for likely structure
#include <optional>   // For std::optional
#include <vector>     // For S2C_Response's data (if we stick to that struct)
#include <memory>     // For flatbuffers::DetachedBuffer if used directly
#include "flatbuffers/flatbuffers.h" // For flatbuffers::FlatBufferBuilder
#include "../Gameplay/PlayerManager.h"
#include "NetworkCommon.h"

// Forward declare the FlatBuffer message type
namespace RiftForged { namespace Networking { namespace UDP { namespace C2S { struct C2S_PingMsg; } } } }

// Define S2C_Response struct (or include a header that defines it)
// This struct will carry the data the WorkerThread needs to send a response.
//namespace RiftForged {
//    namespace Networking {
//        // Forward declare MessageType if it's in GamePacketHeader.h and not included directly here
//        enum class MessageType : uint16_t;
//
//        struct S2C_Response {
//            flatbuffers::DetachedBuffer data;
//            MessageType messageType;
//            bool broadcast = false;
//            NetworkEndpoint specific_recipient;
//        };
//    }
//}


namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class PingMessageHandler {
                public:
                    PingMessageHandler(RiftForged::GameLogic::PlayerManager& playerManager); // Default constructor

                    // Process now returns an optional response to send
                    std::optional<S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
						RiftForged::GameLogic::ActivePlayer* player, // Ensure this is passed for context
                        const C2S_PingMsg* message
                    );
                    // NO m_udpSocket member anymore
                };

            }
        }
    }
}