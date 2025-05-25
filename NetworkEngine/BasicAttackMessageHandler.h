// File: NetworkEngine/BasicAttackMessageHandler.h
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include "NetworkEndpoint.h"
#include "NetworkCommon.h" // For S2C_Response
#include <optional>

// Forward declare dependencies
namespace RiftForged {
    namespace GameLogic {
        class PlayerManager; // For PlayerManager
        struct ActivePlayer;  // For ActivePlayer
    }
    namespace Gameplay {
        class GameplayEngine; // For GameplayEngine
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
                    BasicAttackMessageHandler(
                        RiftForged::GameLogic::PlayerManager& playerManager,
                        RiftForged::Gameplay::GameplayEngine& gameplayEngine
                    );

                    // <<< MODIFIED Process method signature: Now takes ActivePlayer* >>>
                    std::optional<RiftForged::Networking::S2C_Response> Process(
                        const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* attacker, // <<< ADDED parameter
                        const RiftForged::Networking::UDP::C2S::C2S_BasicAttackIntentMsg* message
                    );

                private:
                    RiftForged::GameLogic::PlayerManager& m_playerManager; //
                    RiftForged::Gameplay::GameplayEngine& m_gameplayEngine; //
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged