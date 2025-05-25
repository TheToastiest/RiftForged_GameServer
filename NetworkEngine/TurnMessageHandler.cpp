// File: TurnMessageHandler.cpp (Updated)
#include "TurnMessageHandler.h"

// Adjust paths as necessary and ensure V0.0.3 for FlatBuffers
#include "../Gameplay/ActivePlayer.h"   // <<< ADDED: For ActivePlayer full definition
#include "../Gameplay/GameplayEngine.h" //
#include "../Utils/Logger.h"            //
// NetworkEndpoint.h is included via TurnMessageHandler.h -> NetworkCommon.h -> NetworkEndpoint.h
// PlayerManager.h is likely included via GameplayEngine.h or TurnMessageHandler.h constructor

// FlatBuffers (V0.0.3)
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h" // For C2S_TurnIntentMsg
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h" // Not strictly needed here unless creating S2C messages directly

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                TurnMessageHandler::TurnMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine)
                    : m_playerManager(playerManager),       //
                    m_gameplayEngine(gameplayEngine) {    //
                    RF_NETWORK_INFO("TurnMessageHandler: Constructed with PlayerManager and GameplayEngine."); //
                }

                // <<< MODIFIED Process method signature and implementation >>>
                std::optional<RiftForged::Networking::S2C_Response> TurnMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* player, // <<< Using passed-in player
                    const RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsg* message) { //

                    if (!message) { //
                        RF_NETWORK_WARN("TurnMessageHandler: Received null C2S_TurnIntentMsg from {}", sender_endpoint.ToString()); //
                        return std::nullopt;
                    }

                    // The 'player' pointer is now passed in from MessageDispatcher.
                    if (!player) {
                        RF_NETWORK_WARN("TurnMessageHandler: Null player pointer received for endpoint {}. This should be caught earlier.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    // REMOVED: The call to m_playerManager.GetOrCreatePlayer(sender_endpoint);
                    // RiftForged::GameLogic::ActivePlayer* player = m_playerManager.GetOrCreatePlayer(sender_endpoint/*, bool& out_was_newly_created*/);

                    float turn_delta = message->turn_delta_degrees(); //

                    RF_NETWORK_TRACE("Player {} (endpoint: {}) sent TurnIntent: {} degrees.", //
                        player->playerId, sender_endpoint.ToString(), turn_delta); //

                    // Call GameplayEngine to process the turn
                    m_gameplayEngine.TurnPlayer(player, turn_delta); //
                    // GameplayEngine::TurnPlayer directly updates player->orientation, sets player->isDirty = true,
                    // and calls PhysicsEngine to update the PxController's actor orientation.
                    // State updates (like S2C_EntityStateUpdateMsg) are typically sent by a separate game loop mechanism.

                    return std::nullopt; // Turning doesn't typically generate a direct S2C response from its handler.
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged