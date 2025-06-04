// File: TurnMessageHandler.cpp
#include "TurnMessageHandler.h"

// Adjust paths as necessary and ensure V0.0.3 for FlatBuffers
#include "../Gameplay/ActivePlayer.h"
#include "../Gameplay/GameplayEngine.h"
#include "../Utils/Logger.h"
// No FlatBuffers S2C includes needed if not creating S2C messages directly

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                TurnMessageHandler::TurnMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                    RiftForged::Utils::Threading::TaskThreadPool* taskPool) // New: Receive taskPool
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine),
                    m_taskThreadPool(taskPool) { // New: Initialize m_taskThreadPool
                    RF_NETWORK_INFO("TurnMessageHandler: Constructed.");
                    if (m_taskThreadPool) {
                        RF_NETWORK_INFO("TurnMessageHandler: TaskThreadPool provided (though unlikely to be used here).");
                    }
                    else {
                        RF_NETWORK_WARN("TurnMessageHandler: No TaskThreadPool provided.");
                    }
                }

                std::optional<RiftForged::Networking::S2C_Response> TurnMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* player,
                    const RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsg* message) {

                    if (!message) {
                        RF_NETWORK_WARN("TurnMessageHandler: Received null C2S_TurnIntentMsg from {}", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    if (!player) {
                        RF_NETWORK_WARN("TurnMessageHandler: Null player pointer received for endpoint {}. This should be caught earlier.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    float turn_delta = message->turn_delta_degrees();

                    RF_NETWORK_TRACE("Player {} (endpoint: {}) sent TurnIntent: {:.2f} degrees.",
                        player->playerId, sender_endpoint.ToString(), turn_delta);

                    // Call GameplayEngine to process the turn.
                    // This is a direct state modification and should typically occur synchronously
                    // on the main simulation thread for consistency and immediate impact.
                    m_gameplayEngine.TurnPlayer(player, turn_delta);

                    // --- Thread Pool Usage (Not typically needed for turning) ---
                    // While the m_taskThreadPool is available, turning logic is generally
                    // lightweight and critical for real-time player control, so it's best
                    // handled synchronously. There are typically no complex secondary tasks
                    // that would warrant offloading to a thread pool for a simple turn.
                    // If you envision a scenario where a turn triggers something heavy and non-blocking,
                    // you could enqueue a task here (e.g., complex environmental scan based on new facing).
                    // Example (if needed, but generally avoid for simple turn):
                    // if (m_taskThreadPool) {
                    //     uint64_t playerId_copy = player->playerId;
                    //     // Capture immutable data needed by the async task
                    //     m_taskThreadPool->enqueue([playerId_copy]() {
                    //         // Perform some heavy, non-blocking background task
                    //         std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    //         RF_NETWORK_DEBUG("TurnMessageHandler (ThreadPool): Async task related to turn for Player {}", playerId_copy);
                    //     });
                    // }

                    return std::nullopt; // Turning doesn't typically generate a direct S2C response from its handler.
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged