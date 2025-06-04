// File: NetworkEngine/UDP/C2S/PingMessageHandler.h
#pragma once
#include "NetworkEndpoint.h" // Adjusted path for likely structure
#include <optional>    // For std::optional
#include <vector>      // For S2C_Response's data (if we stick to that struct)
#include <memory>      // For flatbuffers::DetachedBuffer if used directly
#include "flatbuffers/flatbuffers.h" // For flatbuffers::FlatBufferBuilder
#include "../Gameplay/PlayerManager.h"
#include "NetworkCommon.h"
#include "../Utils/ThreadPool.h" // New: Include ThreadPool header

// Forward declare the FlatBuffer message type
namespace RiftForged { namespace Networking { namespace UDP { namespace C2S { struct C2S_PingMsg; } } } }

// Forward declare ActivePlayer for context
namespace RiftForged { namespace GameLogic { struct ActivePlayer; } }

// Forward declare TaskThreadPool
namespace RiftForged {
    namespace Utils {
        namespace Threading {
            class TaskThreadPool;
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class PingMessageHandler {
                public:
                    // Updated constructor to include TaskThreadPool*
                    PingMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager, // Keep existing dependency
                        RiftForged::Utils::Threading::TaskThreadPool* taskPool = nullptr // New: Optional TaskThreadPool pointer
                    );

                    // Process now returns an optional response to send
                    std::optional<S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player, // Ensure this is passed for context
                        const C2S_PingMsg* message
                    );
                    // NO m_udpSocket member anymore
                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager; // Keep existing member
                    RiftForged::Utils::Threading::TaskThreadPool* m_taskThreadPool; // New: Member to hold the thread pool pointer
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged