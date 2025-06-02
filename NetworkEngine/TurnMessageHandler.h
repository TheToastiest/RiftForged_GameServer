#pragma once

#include "NetworkEndpoint.h"
#include "NetworkCommon.h" // For S2C_Response
#include <optional>

#include "flatbuffers/flatbuffers.h"

#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S_TurnIntentMsg
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_EntityStateUpdateMsg
#include "../Gameplay/PlayerManager.h"

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
                struct C2S_TurnIntentMsg; //
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
                        RiftForged::GameLogic::PlayerManager& playerManager,    //
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine  //
                    );

                    // <<< MODIFIED Process method signature: Now takes ActivePlayer* >>>
                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player, // <<< ADDED parameter
                        const RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager;    //
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;  //
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged