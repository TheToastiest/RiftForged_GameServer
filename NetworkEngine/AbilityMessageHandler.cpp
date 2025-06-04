// File: UDPServer/PacketManagement/Handlers_C2S/AbilityMessageHandler.cpp
#include "AbilityMessageHandler.h"
// Generated FlatBuffer headers
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
#include "../Gameplay/PlayerManager.h" // Already included by AbilityMessageHandler.h
#include "../Gameplay/ActivePlayer.h"   // Already included by AbilityMessageHandler.h
#include "../Gameplay/GameplayEngine.h" // Already included by AbilityMessageHandler.h
#include "../Utils/Logger.h" // Use your logger instead of iostream
#include <chrono> // For std::this_thread::sleep_for for demonstration

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                AbilityMessageHandler::AbilityMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                    RiftForged::Utils::Threading::TaskThreadPool* taskPool) // New: Receive taskPool
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine),
                    m_taskThreadPool(taskPool) // New: Initialize m_taskThreadPool
                {
                    RF_NETWORK_INFO("AbilityMessageHandler: Constructed.");
                    if (m_taskThreadPool) {
                        RF_NETWORK_INFO("AbilityMessageHandler: TaskThreadPool provided.");
                    }
                    else {
                        RF_NETWORK_WARN("AbilityMessageHandler: No TaskThreadPool provided.");
                    }
                }

                std::optional<RiftForged::Networking::S2C_Response> AbilityMessageHandler::Process(
                    const NetworkEndpoint& sender_endpoint,
                    GameLogic::ActivePlayer* player, // Added ActivePlayer pointer
                    const C2S_UseAbilityMsg* message) {

                    if (!message) {
                        RF_NETWORK_WARN("AbilityMessageHandler: Null message from {}", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    if (!player) {
                        RF_NETWORK_WARN("AbilityMessageHandler: Null player object provided for ability use from {}. AbilityID: {}",
                            sender_endpoint.ToString(), message->ability_id());
                        return std::nullopt;
                    }

                    RF_NETWORK_INFO("AbilityMessageHandler: Player {} using ability {} from {}",
                        player->playerId, message->ability_id(), sender_endpoint.ToString());

                    // --- Core Ability Execution (Synchronous) ---
                    // The primary logic for using an ability (e.g., mana cost, cooldown, immediate impact)
                    // often needs to be synchronous and on the main simulation thread to maintain game state integrity.
                    // For example:
                    // bool canUse = m_gameplayEngine.CanPlayerUseAbility(player, message->ability_id());
                    // if (!canUse) {
                    //     RF_NETWORK_INFO("AbilityMessageHandler: Player {} failed to use ability {} (cannot use).", player->playerId, message->ability_id());
                    //     // Potentially send an S2C_AbilityFailed response here
                    //     return std::nullopt;
                    // }
                    // RiftForged::Gameplay::AbilityOutcome outcome = m_gameplayEngine.ExecutePlayerAbility(player, message->ability_id(), message->target_entity_id(), message->target_position());


                    // --- Potential Thread Pool Usage (Hypothetical for Complex Abilities) ---
                    // If the ability has a complex asynchronous component, like:
                    // - Large Area-of-Effect (AoE) calculations over many entities.
                    // - Complex raycasting or physics queries for target finding in dense environments.
                    // - Spawning many independent entities that require detailed setup not critical to the current tick.
                    // - Triggering long-running environmental effects.
                    // - Complex visual effect setup that can be pre-calculated.
                    if (m_taskThreadPool) {
                        // Capture necessary data by value to avoid race conditions with main thread data.
                        uint64_t playerId_copy = player->playerId;
                        uint32_t abilityId_copy = message->ability_id();
                        std::optional<RiftForged::Networking::Shared::Vec3> targetPos_copy;
                        if (message->target_position()) {
                            targetPos_copy = RiftForged::Networking::Shared::Vec3(
                                message->target_position()->x(),
                                message->target_position()->y(),
                                message->target_position()->z()
                            );
                        }
                        // Add other necessary data copies from 'outcome' if 'ExecutePlayerAbility' was called synchronously above.

                        m_taskThreadPool->enqueue([playerId_copy, abilityId_copy, targetPos_copy]() {
                            // This code runs on a worker thread.
                            // Example: Simulate a complex AOE calculation or environmental impact.
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            RF_NETWORK_DEBUG("AbilityMessageHandler (ThreadPool): Async processing for Player {} using Ability {}. Target Pos provided: {}",
                                playerId_copy, abilityId_copy, targetPos_copy.has_value() ? "Yes" : "No");

                            // Results from this async task might need to be re-queued to the main simulation thread
                            // (e.g., via a main thread command queue) for safe game state modification.
                            // For example, if this calculation results in damage, you'd enqueue a "ApplyDamageCommand"
                            // to the main simulation loop's command queue.
                            });
                    }

                    // Abilities might or might not send an immediate S2C_Response.
                    // If an ability fails, you might send an S2C_AbilityFailed message.
                    // If it succeeds and has immediate visible effects, those might be part of the general
                    // S2C state updates or a specific S2C message broadcast from the main loop or a
                    // dedicated system.
                    return std::nullopt;
                }
            }
        }
    }
}