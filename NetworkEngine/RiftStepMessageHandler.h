#pragma once

#include "NetworkEndpoint.h" // Adjust path if needed
#include "NetworkCommon.h"   // For S2C_Response (ensure this includes <optional>)
#include "../Gameplay/PlayerManager.h"    // For ActivePlayer struct
#include "../Gameplay/ActivePlayer.h"
#include "flatbuffers/flatbuffers.h"

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
                struct C2S_RiftStepActivationMsg;
            }
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
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine
                    );

                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        const RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
                };

            }
        }
    }
}