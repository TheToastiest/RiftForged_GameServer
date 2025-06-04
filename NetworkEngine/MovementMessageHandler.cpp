// File: MovementMessageHandler.cpp
// RiftForged Development Team
// Copyright (c) 2023-2024 RiftForged Development Team

#include "MovementMessageHandler.h" // Includes NetworkCommon.h, NetworkEndpoint.h, and now ThreadPool.h

// FlatBuffers
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h" // For Vec3

#include "../Utils/Logger.h"
#include "../Gameplay/GameplayEngine.h"
#include "../Gameplay/ActivePlayer.h"
#include <chrono> // For std::this_thread::sleep_for for demonstration

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                // Combined constructor implementation
                MovementMessageHandler::MovementMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                    RiftForged::Utils::Threading::TaskThreadPool* taskPool) // Receive taskPool
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine),
                    m_taskThreadPool(taskPool) { // Initialize m_taskThreadPool
                    RF_NETWORK_INFO("MovementMessageHandler: Constructed.");
                    if (m_taskThreadPool) {
                        RF_NETWORK_INFO("MovementMessageHandler: TaskThreadPool provided.");
                    }
                    else {
                        RF_NETWORK_WARN("MovementMessageHandler: No TaskThreadPool provided.");
                    }
                }

                std::optional<RiftForged::Networking::S2C_Response> MovementMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* player,
                    const RiftForged::Networking::UDP::C2S::C2S_MovementInputMsg* message) {

                    if (!message) {
                        RF_NETWORK_WARN("MovementMessageHandler: Received null C2S_MovementInputMsg from {}", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    const RiftForged::Networking::Shared::Vec3* fb_local_dir_ptr = message->local_direction_intent();
                    if (!fb_local_dir_ptr) {
                        RF_NETWORK_WARN("MovementMessageHandler: C2S_MovementInputMsg from {} is missing local_direction_intent.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    if (!player) {
                        RF_NETWORK_WARN("MovementMessageHandler: Received null player pointer for endpoint {}. This should have been handled earlier.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    RiftForged::Networking::Shared::Vec3 native_local_dir(fb_local_dir_ptr->x(), fb_local_dir_ptr->y(), fb_local_dir_ptr->z());
                    bool is_sprinting = message->is_sprinting();

                    RF_NETWORK_TRACE("Player {} (endpoint: {}) sent MovementInput. LocalDir: ({:.2f},{:.2f},{:.2f}), Sprint: {}",
                        player->playerId, sender_endpoint.ToString(),
                        native_local_dir.x(), native_local_dir.y(), native_local_dir.z(), is_sprinting);

                    // TODO: The delta_time_sec should come from your server's main loop tick.
                    // This is critical for consistent simulation.
                    const float placeholder_delta_time_sec = 1.0f / 30.0f; // Example: Assuming a 30Hz tick rate for this placeholder

                    m_gameplayEngine.ProcessMovement(player, native_local_dir, is_sprinting, placeholder_delta_time_sec);

                    // --- Potential Thread Pool Usage (Hypothetical) ---
                    // While core movement updates are usually synchronous, the thread pool can be used
                    // for secondary, non-critical tasks related to movement.
                    // For example: complex logging, analytics, or background environmental checks.
                    if (m_taskThreadPool) {
                        uint64_t playerId_copy = player->playerId; // Capture ID by value for thread safety
                        RiftForged::Networking::Shared::Vec3 currentPos_copy = player->position; // Capture current position

                        m_taskThreadPool->enqueue([playerId_copy, currentPos_copy, native_local_dir, is_sprinting]() {
                            // This task runs on a worker thread from the pool.
                            // Simulate a background analytics or complex validation check.
                            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Simulate some work
                            RF_NETWORK_DEBUG("MovementMessageHandler (ThreadPool): Async analytics for Player {}. Pos: ({:.1f}, {:.1f}, {:.1f}), Intent: ({:.1f}, {:.1f}, {:.1f})",
                                playerId_copy, currentPos_copy.x(), currentPos_copy.y(), currentPos_copy.z(),
                                native_local_dir.x(), native_local_dir.y(), native_local_dir.z());
                            // Do NOT modify 'player' or 'm_gameplayEngine' directly from here unless they are thread-safe.
                            });
                    }

                    // Movement input typically doesn't generate an immediate S2C_Response.
                    // Player state updates are usually sent periodically by a separate system
                    // based on the 'isDirty' flag in ActivePlayer, or as part of a world state broadcast.
                    return std::nullopt;
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged