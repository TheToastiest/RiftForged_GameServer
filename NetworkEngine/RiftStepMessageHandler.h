// File: NetworkEngine/UDP/C2S/RiftStepMessageHandler.h
#pragma once

#include "NetworkEndpoint.h"    // For RiftForged::Networking::NetworkEndpoint
#include "NetworkCommon.h"      // For RiftForged::Networking::S2C_Response & std::optional
#include "../Gameplay/PlayerManager.h"
#include "../Gameplay/ActivePlayer.h"
#include "../Utils/ThreadPool.h" // Include the ThreadPool header
#include "flatbuffers/flatbuffers.h"

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
                struct C2S_RiftStepActivationMsg;
            }
        }
    }
    namespace Utils {
        namespace Threading {
            class TaskThreadPool; // Forward declare TaskThreadPool
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class RiftStepMessageHandler {
                public:
                    RiftStepMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                        RiftForged::Utils::Threading::TaskThreadPool* taskPool = nullptr // New: Optional TaskThreadPool pointer
                    );

                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player,
                        const RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsg* message
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