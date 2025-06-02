// File: MovementMessageHandler.cpp (Updated)
// RiftForged Development Team
// Copyright (c) 2023-2024 RiftForged Development Team


#include "MovementMessageHandler.h" // Includes NetworkCommon.h, NetworkEndpoint.h

// <<< MODIFIED: Ensure these are V0.0.3 and paths are correct >>>
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"  // Defines C2S_MovementInputMsg
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"    // For Vec3
// #include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // Not strictly needed here unless creating S2C messages directly in this handler

#include "../Utils/Logger.h"                                           //
#include "../Gameplay/GameplayEngine.h"                                // For m_gameplayEngine
#include "../Gameplay/ActivePlayer.h"                                  // <<< ADDED: For ActivePlayer definition
// PlayerManager.h is likely included via GameplayEngine.h or MovementMessageHandler.h if constructor needs full type

// <iostream> was for placeholder logging, now using Logger.h

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                MovementMessageHandler::MovementMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine)
                    : m_playerManager(playerManager),       //
                    m_gameplayEngine(gameplayEngine) {    //
                    RF_NETWORK_INFO("MovementMessageHandler: Constructed with PlayerManager and GameplayEngine."); //
                }

                // <<< MODIFIED Process method signature and implementation >>>
                std::optional<RiftForged::Networking::S2C_Response> MovementMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* player, // <<< Using passed-in player
                    const RiftForged::Networking::UDP::C2S::C2S_MovementInputMsg* message) { //

                    if (!message) { //
                        RF_NETWORK_WARN("MovementMessageHandler: Received null C2S_MovementInputMsg from {}", sender_endpoint.ToString()); //
                        return std::nullopt;
                    }

                    const RiftForged::Networking::Shared::Vec3* fb_local_dir_ptr = message->local_direction_intent(); //
                    if (!fb_local_dir_ptr) { //
                        RF_NETWORK_WARN("MovementMessageHandler: C2S_MovementInputMsg from {} is missing local_direction_intent.", sender_endpoint.ToString()); //
                        return std::nullopt;
                    }

                    // The 'player' pointer is now passed in from MessageDispatcher, which got it from PacketProcessor.
                    // PacketProcessor already handled GetOrCreatePlayer and the new player initialization.
                    if (!player) {
                        // This case should ideally not be hit if PacketProcessor guarantees a valid player,
                        // but a safety check doesn't hurt.
                        RF_NETWORK_WARN("MovementMessageHandler: Received null player pointer for endpoint {}. This should have been handled earlier.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    // REMOVED: The call to m_playerManager.GetOrCreatePlayer(sender_endpoint);
                    // RiftForged::GameLogic::ActivePlayer* player = m_playerManager.GetOrCreatePlayer(sender_endpoint/*, bool& out_was_newly_created*/);

                    RiftForged::Networking::Shared::Vec3 native_local_dir(fb_local_dir_ptr->x(), fb_local_dir_ptr->y(), fb_local_dir_ptr->z()); //
                    bool is_sprinting = message->is_sprinting(); //

                    RF_NETWORK_TRACE("Player {} (endpoint: {}) sent MovementInput. LocalDir: ({},{},{}), Sprint: {}", //
                        player->playerId, sender_endpoint.ToString(), //
                        native_local_dir.x(), native_local_dir.y(), native_local_dir.z(), is_sprinting); //

                    // TODO: The delta_time_sec should come from your server's main loop tick,
                    //       not be a placeholder here. This is critical for consistent simulation.
                    const float placeholder_delta_time_sec = 1.0f / 30.0f; // Example: Assuming a 30Hz tick rate for this placeholder

                    m_gameplayEngine.ProcessMovement(player, native_local_dir, is_sprinting, placeholder_delta_time_sec); //

                    // Movement input typically doesn't generate an immediate S2C_Response.
                    // Player state updates are usually sent periodically by a separate system
                    // based on the 'isDirty' flag in ActivePlayer, or as part of a world state broadcast.
                    return std::nullopt; //
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged