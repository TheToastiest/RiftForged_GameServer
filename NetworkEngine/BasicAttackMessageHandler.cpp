// File: BasicAttackMessageHandler.cpp (Updated)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team


#include "BasicAttackMessageHandler.h"

// FlatBuffers (V0.0.3)
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h"    // For Vec3, DamageInstance, Enums
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h"    // For S2C_CombatEventMsg builders etc.
// C2S messages included via BasicAttackMessageHandler.h or directly if preferred

// Project specific
#include "../NetworkEngine/GamePacketHeader.h" // For MessageType
#include "../Gameplay/PlayerManager.h"         // Included for m_playerManager type, though not used for GetOrCreatePlayer anymore
#include "../Gameplay/ActivePlayer.h"          // For ActivePlayer definition
#include "../Gameplay/GameplayEngine.h"        // For m_gameplayEngine
#include "../Gameplay/CombatData.h"           // For AttackOutcome, DamageApplicationDetails
#include "../Utils/Logger.h"                   // For RF_NETWORK_INFO etc.

#include <chrono> // For timestamps

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                BasicAttackMessageHandler::BasicAttackMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine)
                    : m_playerManager(playerManager), //
                    m_gameplayEngine(gameplayEngine) { //
                    RF_NETWORK_INFO("BasicAttackMessageHandler: Constructed with PlayerManager and GameplayEngine."); //
                }

                // <<< MODIFIED Process method signature and implementation >>>
                std::optional<RiftForged::Networking::S2C_Response> BasicAttackMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* attacker, // <<< Using passed-in attacker
                    const RiftForged::Networking::UDP::C2S::C2S_BasicAttackIntentMsg* message) { //

                    if (!message) { //
                        RF_NETWORK_WARN("BasicAttackMessageHandler: Received null C2S_BasicAttackIntentMsg from {}", sender_endpoint.ToString()); //
                        return std::nullopt;
                    }
                    if (!message->aim_direction()) { //
                        RF_NETWORK_WARN("BasicAttackMessageHandler: C2S_BasicAttackIntentMsg from {} is missing aim_direction.", sender_endpoint.ToString()); //
                        return std::nullopt;
                    }

                    // Player pointer is now passed in, no need to call GetOrCreatePlayer here.
                    // It should have been validated by MessageDispatcher before calling this handler.
                    if (!attacker) { //
                        RF_NETWORK_WARN("BasicAttackMessageHandler: Null attacker pointer received for endpoint {}. This should ideally be caught earlier.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    // All existing logic from your previous BasicAttackMessageHandler.cpp is good from here onwards,
                    // as it already uses the 'attacker' variable.
                    RiftForged::Networking::Shared::Vec3 world_aim_direction( //
                        message->aim_direction()->x(), //
                        message->aim_direction()->y(), //
                        message->aim_direction()->z()  //
                    );
                    uint64_t optional_target_id = message->target_entity_id(); //

                    RF_NETWORK_DEBUG("Player {} (endpoint: {}) sent BasicAttackIntent. Aim: ({:.1f},{:.1f},{:.1f}), TargetID: {}", //
                        attacker->playerId, sender_endpoint.ToString(), //
                        world_aim_direction.x(), world_aim_direction.y(), world_aim_direction.z(), //
                        optional_target_id); //

                    RiftForged::GameLogic::AttackOutcome outcome = //
                        m_gameplayEngine.ExecuteBasicAttack(attacker, world_aim_direction, optional_target_id); //

                    if (!outcome.success) { //
                        RF_NETWORK_INFO("BasicAttackMessageHandler: GameplayEngine indicated Basic Attack failed for PlayerID {}. Reason: {}", //
                            attacker->playerId, outcome.failure_reason_code); //
                        return std::nullopt;
                    }

                    // Assuming AttackOutcome has 'damage_events' as std::vector<DamageApplicationDetails>
                    // And DamageApplicationDetails has target_id, final_damage_dealt, damage_type, was_crit, was_kill
                    if (outcome.damage_events.empty()) { // Using your updated member name from your .cpp
                        RF_NETWORK_INFO("BasicAttackMessageHandler: PlayerID {} Basic Attack performed but no targets hit (or miss). AnimTag: {}", //
                            attacker->playerId, outcome.attack_animation_tag_for_caster); //
                        // Decide if you want to send an S2C message for a "miss" or just do nothing.
                        // For now, returning nullopt means no S2C message from this handler for a miss.
                        return std::nullopt;
                    }

                    // For now, send one S2C_CombatEventMsg for the first hit.
                    // TODO: Handle multiple damage events if basic attack is AoE/cleave.
                    const RiftForged::GameLogic::DamageApplicationDetails& first_hit = outcome.damage_events[0]; //

                    RF_NETWORK_INFO("BasicAttackMessageHandler: GameplayEngine SUCCESS for Basic Attack by PlayerID {}. TargetID: {}, Damage: {}", //
                        attacker->playerId, first_hit.target_id, first_hit.final_damage_dealt); //

                    flatbuffers::FlatBufferBuilder builder(512); //

                    RiftForged::Networking::Shared::DamageInstance fb_damage_instance( //
                        first_hit.final_damage_dealt, //
                        first_hit.damage_type,        //
                        first_hit.was_crit            //
                    );

                    auto damage_details_offset = RiftForged::Networking::UDP::S2C::CreateCombatEvent_DamageDealtDetails(builder, //
                        attacker->playerId,      // source_entity_id
                        first_hit.target_id,     // target_entity_id
                        &fb_damage_instance,     // Pass POINTER to the FlatBuffer DamageInstance struct
                        first_hit.was_kill,      //
                        true                     // is_basic_attack = true
                    );

                    uint64_t server_ts = std::chrono::duration_cast<std::chrono::milliseconds>( //
                        std::chrono::system_clock::now().time_since_epoch()).count(); //

                    RiftForged::Networking::UDP::S2C::S2C_CombatEventMsgBuilder combat_event_builder(builder); //
                    combat_event_builder.add_event_type(RiftForged::Networking::UDP::S2C::CombatEventType_DamageDealt); //
                    combat_event_builder.add_event_payload_type(RiftForged::Networking::UDP::S2C::CombatEventPayload_DamageDealt); //
                    combat_event_builder.add_event_payload(damage_details_offset.Union()); //
                    combat_event_builder.add_server_timestamp_ms(server_ts); //
                    auto s2c_payload_offset = combat_event_builder.Finish(); //

                    RiftForged::Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder); //
                    root_builder.add_payload_type(RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_CombatEvent); //
                    root_builder.add_payload(s2c_payload_offset.Union()); //
                    auto root_offset = root_builder.Finish(); //
                    builder.Finish(root_offset); //

                    S2C_Response response_to_send; //
                    response_to_send.data = builder.Release(); //
                    response_to_send.messageType = RiftForged::Networking::MessageType::S2C_CombatEvent; //
                    response_to_send.broadcast = true; //
                    // Consider if combat events should always be broadcast or sometimes targeted.
                    // For now, broadcast is fine for all clients to see the event.
                    // response_to_send.specific_recipient = sender_endpoint; // Not needed if broadcast true

                    RF_NETWORK_INFO("BasicAttackMessageHandler: S2C_CombatEventMsg (DamageDealt) prepared for broadcast. Attacker: {}, Target: {}", //
                        attacker->playerId, first_hit.target_id); //
                    return response_to_send;
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged