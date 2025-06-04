// File: UDPServer/PacketManagement/Handlers_C2S/AbilityMessageHandler.h
#pragma once
#include "NetworkEndpoint.h"
#include "NetworkCommon.h"
#include "../Gameplay/PlayerManager.h"
#include "../PhysicsEngine/PhysicsEngine.h" // Assuming this might be used by ability logic
#include "../Utils/ThreadPool.h" // New: Include ThreadPool header

// Forward Declaration Headers
namespace RiftForged {
    namespace GameLogic {
        class PlayerManager;
        struct ActivePlayer; // Ensure ActivePlayer is forward declared if needed
        // class AbilityExecutionService; // Uncomment if you uncomment the member below
    }
    namespace Gameplay {
        class GameplayEngine;
    }
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_UseAbilityMsg;
            }
        }
    }
    namespace Utils { // New: For ThreadPool forward declaration
        namespace Threading {
            class TaskThreadPool;
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                class AbilityMessageHandler {
                public:
                    // Updated constructor to include TaskThreadPool*
                    AbilityMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                        RiftForged::Utils::Threading::TaskThreadPool* taskPool = nullptr // Optional TaskThreadPool pointer
                    );

                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player, // Added ActivePlayer pointer
                        const C2S_UseAbilityMsg* message
                    );

                private:
                    // Uncomment if you need to use the AbilityExecutionService
                    // RiftForged::GameLogic::AbilityExecutionService& m_abilityService;
                    RiftForged::GameLogic::PlayerManager& m_playerManager;
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
                    RiftForged::Utils::Threading::TaskThreadPool* m_taskThreadPool; // New: Member to hold the thread pool pointer

                };
            }
        }
    }
}
