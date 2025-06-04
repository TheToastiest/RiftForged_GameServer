// File: MovementMessageHandler.h
#pragma once

#include "NetworkEndpoint.h"    // For RiftForged::Networking::NetworkEndpoint
#include "NetworkCommon.h"      // For RiftForged::Networking::S2C_Response & std::optional
#include "../Gameplay/PlayerManager.h"
#include "../Utils/Logger.h" // For RF_CORE_INFO etc.
#include "../Utils/ThreadPool.h" // For TaskThreadPool
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S_MovementInputMsg

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
                struct C2S_MovementInputMsg;
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

                class MovementMessageHandler {
                public:
                    // Combined constructor: takes all necessary dependencies
                    MovementMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                        RiftForged::Utils::Threading::TaskThreadPool* taskPool = nullptr // Now an optional parameter in the single constructor
                    );

                    // Process method signature remains the same
                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player,
                        const RiftForged::Networking::UDP::C2S::C2S_MovementInputMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
                    RiftForged::Utils::Threading::TaskThreadPool* m_taskThreadPool; // Member to hold the thread pool pointer
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged