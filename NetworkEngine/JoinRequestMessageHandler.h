// File: UDPServer/PacketManagement/Handlers_C2S/JoinRequestMessageHandler.h
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include "NetworkEndpoint.h"          // For RiftForged::Networking::NetworkEndpoint
#include "NetworkCommon.h"            // For RiftForged::Networking::S2C_Response
#include "../../Gameplay/ActivePlayer.h" // For RiftForged::GameLogic::ActivePlayer

// Include the FlatBuffers generated header that contains C2S_JoinRequestMsg
#include "../../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
// Include S2C FlatBuffers for building responses
#include "../../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_JoinSuccessMsg, S2C_JoinFailedMsg

#include <optional>

// Forward declaration for GameServerEngine (dependency injection)
namespace RiftForged {
    namespace Server {
        class GameServerEngine; // Declare GameServerEngine
    }
}

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                class JoinRequestMessageHandler {
                public:
                    /**
                     * @brief Constructor for JoinRequestMessageHandler.
                     * @param gameServerEngine Reference to the GameServerEngine for managing player sessions.
                     * (Now passed to OnClientAuthenticatedAndJoining, not for direct SendJoinFailedResponse)
                     */
                    explicit JoinRequestMessageHandler(RiftForged::Server::GameServerEngine& gameServerEngine);

                    /**
                     * @brief Processes an incoming C2S_JoinRequestMsg.
                     * This handler is unique as it expects a nullptr 'player' object, as a player
                     * is not yet established for a new join request. It's responsible for interacting
                     * with the GameServerEngine to attempt authentication and player creation, and
                     * returning an S2C_Response (success/failure) for the PacketProcessor to send.
                     *
                     * @param sender_endpoint The network endpoint of the client sending the request.
                     * @param player A pointer to the ActivePlayer object. EXPECTED TO BE nullptr for new join requests.
                     * @param message The FlatBuffer C2S_JoinRequestMsg.
                     * @return An optional S2C_Response to be sent back to the client (success/failure).
                     */
                    std::optional<S2C_Response> Process(
                        const NetworkEndpoint& sender_endpoint,
                        RiftForged::GameLogic::ActivePlayer* player, // EXPECTED TO BE nullptr
                        const C2S_JoinRequestMsg* message
                    );

                private:
                    RiftForged::Server::GameServerEngine& m_gameServerEngine; // Still needed for OnClientAuthenticatedAndJoining
                };

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged