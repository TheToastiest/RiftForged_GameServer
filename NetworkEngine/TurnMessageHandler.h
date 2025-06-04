// File: UDPServer/PacketManagement/Handlers_C2S/TurnMessageHandler.h
#pragma once

#include "NetworkEndpoint.h"
#include "NetworkCommon.h" // For S2C_Response
#include "../Utils/ThreadPool.h" // New: Include ThreadPool header
#include <optional>

#include "flatbuffers/flatbuffers.h"

#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S_TurnIntentMsg
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_EntityStateUpdateMsg
#include "../Gameplay/PlayerManager.h"

// Forward declarations
namespace RiftForged {
    namespace GameLogic {
        class PlayerManager;
        struct ActivePlayer;
    }
    namespace Gameplay {
        class GameplayEngine;
    }
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_TurnIntentMsg;
            }
        }
    }
    namespace Utils { // Added for ThreadPool forward declaration
        namespace Threading {
            class TaskThreadPool;
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class TurnMessageHandler {
                public:
                    // Updated constructor to include TaskThreadPool*
                    TurnMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                        RiftForged::Utils::Threading::TaskThreadPool* taskPool = nullptr // New: Optional TaskThreadPool pointer
                    );

                    // Process method signature remains the same
                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player,
                        const RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
                    RiftForged::Utils::Threading::TaskThreadPool* m_taskThreadPool; // New: Member to hold the thread pool pointer
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged