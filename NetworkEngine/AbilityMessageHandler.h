// File: UDPServer/PacketManagement/Handlers_C2S/AbilityMessageHandler.h
#pragma once
#include "NetworkEndpoint.h"
#include "NetworkCommon.h"
#include "../Gameplay/PlayerManager.h"

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_UseAbilityMsg;
            }
        }
    }
}

// namespace RiftForged { namespace GameLogic { class AbilityExecutionService; } }


namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                class AbilityMessageHandler {
                public:
                    AbilityMessageHandler(RiftForged::GameLogic::PlayerManager& m_playerManager);
                    std::optional<RiftForged::Networking::S2C_Response> Process( // Return type changed
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        const C2S_UseAbilityMsg* message
                    );
                private:
			        // Uncomment if you need to use the AbilityExecutionService
					// RiftForged::GameLogic::AbilityExecutionService& m_abilityService; // Uncomment if needed
                        RiftForged::GameLogic::PlayerManager& m_playerManager;
                    
                };
            }
        }
    }
}