// File: NetworkEngine/BasicAttackMessageHandler.h
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include "NetworkEndpoint.h"
#include "NetworkCommon.h" // For S2C_Response
#include "../Utils/ThreadPool.h" // New: Include ThreadPool header
#include <optional>

// Forward declare dependencies
namespace RiftForged {
    namespace GameLogic {
        class PlayerManager; // For PlayerManager
        struct ActivePlayer; // For ActivePlayer
    }
    namespace Gameplay {
        class GameplayEngine; // For GameplayEngine
    }
    namespace Utils { // New: For ThreadPool forward declaration
        namespace Threading {
            class TaskThreadPool;
        }
    }
}

// Forward declare the C2S FlatBuffer message type
namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_BasicAttackIntentMsg;
            }
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class BasicAttackMessageHandler {
                public:
                    // Updated constructor to include TaskThreadPool*
                    BasicAttackMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                        RiftForged::Utils::Threading::TaskThreadPool* taskPool = nullptr // Optional TaskThreadPool pointer
                    );

                    // Process method signature remains the same
                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* attacker,
                        const RiftForged::Networking::UDP::C2S::C2S_BasicAttackIntentMsg* message
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