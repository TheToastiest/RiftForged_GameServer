// File: MovementMessageHandler.h (Assumed to be in a dir the compiler searches directly)
#pragma once

#include "NetworkEndpoint.h"   // For RiftForged::Networking::NetworkEndpoint
#include "NetworkCommon.h"     // For RiftForged::Networking::S2C_Response & std::optional
#include "../Gameplay/PlayerManager.h"
#include <optional> // should be included in NetworkCommon.h if S2C_Response uses it, or here.

// Forward declare PlayerManager and GameplayEngine
namespace RiftForged {
    namespace GameLogic { class PlayerManager; }
    namespace Gameplay { class GameplayEngine; }
}
// Forward declare the FlatBuffer message type
namespace RiftForged { namespace Networking { namespace UDP { namespace C2S { struct C2S_MovementInputMsg; } } } }

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class MovementMessageHandler {
                public:
                    MovementMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine
                    );

                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        const RiftForged::Networking::UDP::C2S::C2S_MovementInputMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
                };

            }
        }
    }
}