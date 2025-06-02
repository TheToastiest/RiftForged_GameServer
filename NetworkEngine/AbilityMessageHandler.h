// File: UDPServer/PacketManagement/Handlers_C2S/AbilityMessageHandler.h
#pragma once
#include "NetworkEndpoint.h"
#include "NetworkCommon.h"
#include "../Gameplay/PlayerManager.h"
#include "../PhysicsEngine/PhysicsEngine.h"

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_UseAbilityMsg;
            }
        }
    }
}

// Forward Declaration Headers
namespace RiftForged {
    namespace GameLogic { class PlayerManager; }
    namespace Gameplay { class GameplayEngine; }
}

// Forward declare the C2S FlatBuffer message type
namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_UseAbilityMsg;
            }
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                class AbilityMessageHandler {
                public:
                    AbilityMessageHandler(RiftForged::GameLogic::PlayerManager& m_playerManager, RiftForged::Gameplay::GameplayEngine& m_gameplayEngine);
                    std::optional<RiftForged::Networking::S2C_Response> Process( // Return type changed
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
						RiftForged::GameLogic::ActivePlayer* player, // Added ActivePlayer pointer
                        const C2S_UseAbilityMsg* message
                    );
                private:
			        // Uncomment if you need to use the AbilityExecutionService
					// RiftForged::GameLogic::AbilityExecutionService& m_abilityService; // Uncomment if needed
                        RiftForged::GameLogic::PlayerManager& m_playerManager;
                        RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
                    
                };
            }
        }
    }
}