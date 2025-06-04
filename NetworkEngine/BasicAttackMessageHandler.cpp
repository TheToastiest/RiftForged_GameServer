// File: BasicAttackMessageHandler.cpp
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "BasicAttackMessageHandler.h"

// FlatBuffers
#include "flatbuffers/flatbuffers.h" // For FlatBufferBuilder
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_UDP_Payload, S2C_CombatEventMsg, S2C_SpawnProjectileMsg, etc.
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // Needed for C2S_BasicAttackIntentMsg

// Game Logic & Engine includes
#include "../Gameplay/PlayerManager.h"
#include "../Gameplay/ActivePlayer.h"
#include "../Gameplay/GameplayEngine.h"
#include "../Gameplay/CombatData.h" // Assuming this defines AttackOutcome and DamageApplicationDetails
#include "../Utils/Logger.h"        // For RF_NETWORK_... macros
#include "../Utils/ThreadPool.h"    // For TaskThreadPool

#include <chrono> // For timestamps and std::this_thread::sleep_for for demonstration

namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {

                BasicAttackMessageHandler::BasicAttackMessageHandler(
                    RiftForged::GameLogic::PlayerManager& playerManager,
                    RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                    RiftForged::Utils::Threading::TaskThreadPool* taskPool)
                    : m_playerManager(playerManager),
                    m_gameplayEngine(gameplayEngine),
                    m_taskThreadPool(taskPool) {
                    RF_NETWORK_INFO("BasicAttackMessageHandler: Constructed.");
                    if (m_taskThreadPool) {
                        RF_NETWORK_INFO("BasicAttackMessageHandler: TaskThreadPool provided.");
                    }
                    else {
                        RF_NETWORK_WARN("BasicAttackMessageHandler: No TaskThreadPool provided.");
                    }
                }

                std::optional<RiftForged::Networking::S2C_Response> BasicAttackMessageHandler::Process(
                    const RiftForged::Networking::NetworkEndpoint& sender_endpoint,
                    RiftForged::GameLogic::ActivePlayer* attacker,
                    const RiftForged::Networking::UDP::C2S::C2S_BasicAttackIntentMsg* message) {

                    if (!message) {
                        RF_NETWORK_WARN("BasicAttackMessageHandler: Received null C2S_BasicAttackIntentMsg from %s. Discarding.", sender_endpoint.ToString());
                        return std::nullopt;
                    }
                    if (!message->aim_direction()) {
                        RF_NETWORK_WARN("BasicAttackMessageHandler: C2S_BasicAttackIntentMsg from %s is missing aim_direction. Discarding.", sender_endpoint.ToString());
                        return std::nullopt;
                    }

                    if (!attacker) {
                        RF_NETWORK_WARN("BasicAttackMessageHandler: Null attacker pointer received for endpoint %s. This should ideally be caught earlier.", sender_endpoint.ToString());
                        return std::nullopt; // Should be handled by PacketProcessor/MessageDispatcher
                    }

                    RiftForged::Networking::Shared::Vec3 world_aim_direction( // Assuming Vec3 has a constructor from x,y,z
                        message->aim_direction()->x(),
                        message->aim_direction()->y(),
                        message->aim_direction()->z()
                    );
                    uint64_t optional_target_id = message->target_entity_id();

                    RF_NETWORK_DEBUG("Player %llu (endpoint: %s) sent BasicAttackIntent. Aim: (%.1f,%.1f,%.1f), TargetID: %llu",
                        attacker->playerId, sender_endpoint.ToString(),
                        world_aim_direction.x(), world_aim_direction.y(), world_aim_direction.z(),
                        optional_target_id);

                    // Execute basic attack in GameplayEngine
                    RiftForged::GameLogic::AttackOutcome outcome =
                        m_gameplayEngine.ExecuteBasicAttack(attacker, world_aim_direction, optional_target_id);

                    if (!outcome.success) {
                        RF_NETWORK_INFO("BasicAttackMessageHandler: GameplayEngine indicated Basic Attack failed for PlayerID %llu. Reason: %s",
                            attacker->playerId, outcome.failure_reason_code.c_str()); // Assuming failure_reason_code is a string
                        return std::nullopt; // No response needed for client-side failures for basic attack
                    }

                    if (outcome.damage_events.empty() && !outcome.spawned_projectile) {
                        RF_NETWORK_INFO("BasicAttackMessageHandler: PlayerID %llu Basic Attack performed but no targets hit or no projectile spawned. AnimTag: %s",
                            attacker->playerId, outcome.attack_animation_tag_for_caster.c_str()); // Assuming string
                        return std::nullopt; // No response if nothing happened
                    }

                    // --- Potential Thread Pool Usage (Hypothetical for Complex Attacks) ---
                    if (m_taskThreadPool) {
                        // Capture immutable copies of data needed for the asynchronous task
                        uint64_t attackerId_copy = attacker->playerId;
                        RiftForged::Networking::Shared::Vec3 attackOrigin_copy = outcome.projectile_start_position; // Or attacker's position
                        std::vector<RiftForged::GameLogic::DamageApplicationDetails> damageEvents_copy = outcome.damage_events; // Copy results

                        m_taskThreadPool->enqueue([attackerId_copy, attackOrigin_copy, damageEvents_copy]() {
                            // This task runs on a worker thread.
                            // Simulate heavy post-attack analysis or complex environmental updates.
                            std::this_thread::sleep_for(std::chrono::milliseconds(20));
                            RF_NETWORK_DEBUG("BasicAttackMessageHandler (ThreadPool): Async post-attack analysis for Player %llu from (%.1f, %.1f, %.1f). Hits: %zu",
                                attackerId_copy, attackOrigin_copy.x(), attackOrigin_copy.y(), attackOrigin_copy.z(),
                                damageEvents_copy.size());

                            // If this async task modifies game state, it must re-queue commands to the main simulation thread.
                            });
                    }

                    // Construct S2C_ messages based on outcome.
                    // Prioritize projectile spawn message if applicable, then combat event.

                    RF_NETWORK_INFO("BasicAttackMessageHandler: GameplayEngine SUCCESS for Basic Attack by PlayerID %llu. Total damage events: %zu, Projectile Spawned: %s",
                        attacker->playerId, outcome.damage_events.size(), outcome.spawned_projectile ? "true" : "false");

                    // If a projectile was spawned, create and return its message
                    if (outcome.spawned_projectile) {
                        flatbuffers::FlatBufferBuilder builder(512);
                        auto s2c_projectile_payload_offset = RiftForged::Networking::UDP::S2C::CreateS2C_SpawnProjectileMsgDirect(builder,
                            outcome.projectile_id,
                            attacker->playerId, // owner_entity_id
                            &outcome.projectile_start_position,
                            &outcome.projectile_direction,
                            outcome.projectile_speed,
                            outcome.projectile_max_range,
                            outcome.projectile_vfx_tag.c_str()
                        );
                        auto root_s2c_message_offset = RiftForged::Networking::UDP::S2C::CreateRoot_S2C_UDP_Message(builder,
                            RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_SpawnProjectile,
                            s2c_projectile_payload_offset.Union()
                        );
                        builder.Finish(root_s2c_message_offset);

                        RiftForged::Networking::S2C_Response response_to_send;
                        response_to_send.data = builder.Release();
                        // CORRECTED: Use FlatBuffer enum directly
                        response_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_SpawnProjectile;
                        response_to_send.broadcast = true; // Projectiles typically broadcast
                        RF_NETWORK_INFO("BasicAttackMessageHandler: S2C_SpawnProjectileMsg prepared for broadcast. ProjectileID: %llu by PlayerID: %llu",
                            outcome.projectile_id, attacker->playerId);
                        return response_to_send;
                    }
                    // If no projectile, then process damage events (could be multiple if AoE/cleave)
                    else if (!outcome.damage_events.empty()) {
                        // For simplicity, creating a response for the *first* damage event.
                        // In a real game, you might bundle multiple hits into one S2C_CombatEventMsg
                        // or send multiple messages if they target different clients/require different visibility.
                        const RiftForged::GameLogic::DamageApplicationDetails& first_hit = outcome.damage_events[0];

                        flatbuffers::FlatBufferBuilder builder(512);

                        // Note: DamageInstance is a FlatBuffer 'struct', so you create it directly.
                        RiftForged::Networking::Shared::DamageInstance fb_damage_instance(
                            first_hit.final_damage_dealt,
                            first_hit.damage_type, // Assuming this is compatible with StatusEffectCategory (or DamageType enum if you have one)
                            first_hit.was_crit
                        );

                        auto damage_details_offset = RiftForged::Networking::UDP::S2C::CreateCombatEvent_DamageDealtDetails(builder,
                            attacker->playerId,     // source_entity_id
                            first_hit.target_id,    // target_entity_id
                            &fb_damage_instance,    // Pass POINTER to the FlatBuffer DamageInstance struct
                            first_hit.was_kill,
                            true                    // is_basic_attack = true
                        );

                        uint64_t server_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();

                        // Create the S2C_CombatEventMsg, wrapping the damage details
                        auto s2c_combat_event_payload_offset = RiftForged::Networking::UDP::S2C::CreateS2C_CombatEventMsg(builder,
                            RiftForged::Networking::UDP::S2C::CombatEventType_DamageDealt,
                            RiftForged::Networking::UDP::S2C::CombatEventPayload_DamageDealt,
                            damage_details_offset.Union(),
                            server_ts
                        );

                        // Create the root S2C_UDP_Message
                        auto root_s2c_message_offset = RiftForged::Networking::UDP::S2C::CreateRoot_S2C_UDP_Message(builder,
                            RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_CombatEvent,
                            s2c_combat_event_payload_offset.Union()
                        );
                        builder.Finish(root_s2c_message_offset);

                        RiftForged::Networking::S2C_Response response_to_send;
                        response_to_send.data = builder.Release();
                        // CORRECTED: Use FlatBuffer enum directly
                        response_to_send.flatbuffer_payload_type = RiftForged::Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_CombatEvent;
                        response_to_send.broadcast = true; // Combat events typically broadcast
                        RF_NETWORK_INFO("BasicAttackMessageHandler: S2C_CombatEventMsg (DamageDealt) prepared for broadcast. Attacker: %llu, Target: %llu, Damage: %d",
                            attacker->playerId, first_hit.target_id, first_hit.final_damage_dealt);
                        return response_to_send;
                    }

                    return std::nullopt; // No projectile and no damage events
                }

            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged