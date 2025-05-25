// File: MovementMessageHandler.h (Assumed to be in a dir the compiler searches directly)
#pragma once

#include "NetworkEndpoint.h"   // For RiftForged::Networking::NetworkEndpoint
#include "NetworkCommon.h"     // For RiftForged::Networking::S2C_Response & std::optional
#include "../Gameplay/PlayerManager.h"
#include <optional> // should be included in NetworkCommon.h if S2C_Response uses it, or here.

// Forward declarations
namespace RiftForged {
    namespace GameLogic {
        class PlayerManager;    //
        struct ActivePlayer;     // <<< ADDED: Forward declaration for ActivePlayer
    }
    namespace Gameplay {
        class GameplayEngine;   //
    }
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_MovementInputMsg; //
            }
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class MovementMessageHandler {
                public:
                    MovementMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,       //
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine     //
                    );

                    // <<< MODIFIED Process method signature: Now takes ActivePlayer* >>>
                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player, // <<< ADDED parameter
                        const RiftForged::Networking::UDP::C2S::C2S_MovementInputMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;    //
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;  //
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged