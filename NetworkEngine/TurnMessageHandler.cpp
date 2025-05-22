#include "TurnMessageHandler.h"

// Adjust paths as necessary for your project structure
#include "..//Gameplay/PlayerManager.h"
#include "../Gameplay/ActivePlayer.h"
#include "../Gameplay/GameplayEngine.h" 
#include "../Utils/Logger.h" // For SpdLog

// Adjust path to your SharedProtocols/Generated/ folder
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h" // For C2S_TurnIntentMsg
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C_EntityStateUpdateMsg

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                TurnMessageHandler::TurnMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine)
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine) {
                    RF_GAMEPLAY_INFO("TurnMessageHandler: Constructed with PlayerManager and GameplayEngine.");
                }

                std::optional<RiftForged::Networking::S2C_Response> TurnMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    const RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsg* message) {

                    if (!message) {
                        RF_NETWORK_WARN("TurnMessageHandler: Received null C2S_TurnIntentMsg from {}", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    RiftForged::GameLogic::ActivePlayer* player = m_playerManager.GetOrCreatePlayer(sender_endpoint);
                    if (!player) {
                        RF_NETWORK_WARN("TurnMessageHandler: PlayerManager could not get/create player for {}. No turn action taken.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    float turn_delta = message->turn_delta_degrees();
                    // uint64_t client_ts = message->client_timestamp_ms(); // Available if needed for logic or anti-cheat

                    RF_GAMEPLAY_DEBUG("Player {} (endpoint: {}) sent TurnIntent: {} degrees.",
                        player->playerId, sender_endpoint.ToString(), turn_delta);

                    // Call GameplayEngine to process the turn
                    m_gameplayEngine.TurnPlayer(player, turn_delta);
                    // GameplayEngine::TurnPlayer directly updates player->orientation and sets player->isDirty = true.
                    // The GameServerEngine::SimulationTick will pick up the dirty player and send S2C_EntityStateUpdateMsg
                    // with the new orientation.

                    // Turning usually doesn't have a direct, unique S2C response from its handler.
                    // The result is seen via the S2C_EntityStateUpdateMsg.
                    return std::nullopt;
                }

            }
        }
    }
}