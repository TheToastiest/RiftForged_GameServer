#pragma once

#include "NetworkEndpoint.h" // Adjust path if needed
#include "NetworkCommon.h"   // For S2C_Response (ensure this includes <optional>)
#include "../Gameplay/PlayerManager.h"    // For ActivePlayer struct
#include "../Gameplay/ActivePlayer.h"
#include "flatbuffers/flatbuffers.h"

// Forward declarations
namespace RiftForged {
    namespace GameLogic {
        class PlayerManager;    //
        struct ActivePlayer;     //
    }
    namespace Gameplay {
        class GameplayEngine;   //
    }
    namespace Networking {
        namespace UDP {
            namespace C2S {
                struct C2S_RiftStepActivationMsg; //
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
                        RiftForged::GameLogic::PlayerManager& playerManager,    //
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine  //
                    );

                    // <<< MODIFIED Process method signature: Now takes ActivePlayer* >>>
                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player, // <<< ADDED parameter
                        const RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;    //
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;  //
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged