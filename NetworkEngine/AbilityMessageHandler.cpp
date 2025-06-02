#include "AbilityMessageHandler.h"
// Generated FlatBuffer headers (adjust path to your SharedProtocols/Generated/ folder)
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_RiftStepExecutedMsg
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h" // For Vec3, Quaternion, etc.
#include "../Gameplay/PlayerManager.h"
#include "../Gameplay/ActivePlayer.h"
#include "../Gameplay/GameplayEngine.h"
#include <iostream>


namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                // AbilityMessageHandler::AbilityMessageHandler(RiftForged::GameLogic::AbilityExecutionService& abilityService)
                // : m_abilityService(abilityService) {}
                AbilityMessageHandler::AbilityMessageHandler(RiftForged::GameLogic::PlayerManager& playerManager, RiftForged::Gameplay::GameplayEngine& gameplayEngine)
                    : m_playerManager(playerManager),
					 m_gameplayEngine(gameplayEngine)
                { // Initialize the reference member
                    std::cout << "AbilityMessageManager: Constructed with PlayerManager." << std::endl;
                }

                std::optional<RiftForged::Networking::S2C_Response> AbilityMessageHandler::Process(
                    const NetworkEndpoint& sender_endpoint,
					GameLogic::ActivePlayer* player, // Added ActivePlayer pointer
                    const C2S_UseAbilityMsg* message) {
                    if (!message) {
                        std::cerr << "AbilityMessageHandler: Null message from " << sender_endpoint.ToString() << std::endl;
                        return std::nullopt;
                    }

                    std::cout << "AbilityMessageHandler: Received UseAbility from " << sender_endpoint.ToString()
                        << " AbilityID: " << message->ability_id() << std::endl;

                    // TODO: 
                    // 1. Get Player object.
                    // 2. Validate.
                    // 3. Call GameplayEngine.
                    // 4. Construct S2C_CombatEventMsg or other S2C messages based on outcome.
                    // For now, no direct response from this stub.
                    return std::nullopt;
                }
            }
        }
    }
}