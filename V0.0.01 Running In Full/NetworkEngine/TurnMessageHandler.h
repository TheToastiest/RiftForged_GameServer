#pragma once

#include "NetworkEndpoint.h"
#include "NetworkCommon.h" // For S2C_Response
#include <optional>

// Forward declare PlayerManager and GameplayEngine
namespace RiftForged {
    namespace GameLogic { class PlayerManager; }
    namespace Gameplay { class GameplayEngine; }
}

// Forward declare the C2S FlatBuffer message type
namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_TurnIntentMsg;
            }
        }
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class TurnMessageHandler {
                public:
                    TurnMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine
                    );

                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        const RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
                };

            }
        }
    }
}