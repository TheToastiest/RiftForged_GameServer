#include "MovementMessageHandler.h" // Includes NetworkCommon.h, NetworkEndpoint.h implicitly

// FlatBuffer generated header (assuming it's in an include path or same directory)
// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.
// #include "riftforged_s2c_udp_messages_generated.h" // If needed for S2C_Response
// This should define RiftForged::Networking::UDP::C2S::C2S_MovementInputMsg

#include "../Gameplay/GameplayEngine.h" // For GameplayEngine

#include <iostream> // For logging

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                MovementMessageHandler::MovementMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine)
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine) {
                    std::cout << "MovementMessageHandler: Constructed with PlayerManager and GameplayEngine." << std::endl;
                }

                std::optional<RiftForged::Networking::S2C_Response> MovementMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    const RiftForged::Networking::UDP::C2S::C2S_MovementInputMsg* message) {

                    if (!message) {
                        std::cerr << "MovementMessageHandler: Received null C2S_MovementInputMsg pointer from "
                            << sender_endpoint.ToString() << std::endl;
                        return std::nullopt;
                    }
                    const Shared::Vec3* fb_move_dir_ptr = message->desired_direction();
                    if (!fb_move_dir_ptr) {
                        std::cerr << "MovementMessageHandler: C2S_MovementInputMsg from " << sender_endpoint.ToString()
                            << " is missing desired_direction." << std::endl;
                        return std::nullopt;
                    }

                    RiftForged::GameLogic::ActivePlayer* player = m_playerManager.GetOrCreatePlayer(sender_endpoint);
                    if (!player) {
                        std::cerr << "MovementMessageHandler: PlayerManager could not get/create player for "
                            << sender_endpoint.ToString() << ". No action taken." << std::endl;
                        return std::nullopt;
                    }

                    // Create a Shared::Vec3 from the FlatBuffer struct pointer for GameplayEngine
                    Shared::Vec3 native_move_dir(fb_move_dir_ptr->x(), fb_move_dir_ptr->y(), fb_move_dir_ptr->z());
                    bool is_sprinting = message->is_sprinting();

                    // Call GameplayEngine to process movement
                    m_gameplayEngine.ProcessMovement(player, native_move_dir, is_sprinting);
                    // GameplayEngine::ProcessMovement directly updates player->position and sets player->isDirty = true.
                    // The GameServerEngine::SimulationTick will pick up the dirty player and send S2C_EntityStateUpdateMsg.

                    // std::cout << "MovementMessageHandler: PlayerID " << player->playerId << " movement intent processed by GameplayEngine." << std::endl;

                    return std::nullopt; // Movement input typically doesn't generate an immediate, unique S2C_Response from this handler.
                }

            }
        }
    }
}