// File: GameServer/GameServerEngine.cpp (Updated for FlatBuffer Command Processing)
// RiftForged Game Development Team
// Copyright (c) 2025-2028 RiftForged Game Development Team

#include "GameServerEngine.h"
#include <sstream>

// PhysX (or physics abstraction) includes for PxControllerCollisionFlag if used by GameplayEngine indirectly
#include "physx/PxQueryReport.h" // Example for PxControllerCollisionFlag

// Windows specific for timer resolution
#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "Winmm.lib")
#endif

// Specific T-types from FlatBuffers C2S messages for std::any_cast
// These are forward-declared in the header, but full definition needed for std::any_cast value semantics.
// Ensure these T-types have appropriate copy/move constructors if std::any is to store them by value.
// Storing std::shared_ptr<const T> in std::any is often safer if T objects are complex.
// For simplicity, assuming direct storage and copyability for now.
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S_MovementInputMsgT etc.

namespace RiftForged {
    namespace Server {

        GameServerEngine::GameServerEngine(
           RiftForged::GameLogic::PlayerManager& playerManager,
           RiftForged::Gameplay::GameplayEngine& gameplayEngine,
           //RiftForged::Networking::UDPPacketHandler& packetHandler,
           RiftForged::Physics::PhysicsEngine& physicsEngine,
           std::chrono::milliseconds tickInterval)
           : m_playerManager(playerManager),
             m_gameplayEngine(gameplayEngine),
             m_packetHandlerPtr(nullptr), // Initialize m_packetHandlerPtr to nullptr
             m_physicsEngine(physicsEngine),
             m_isSimulatingThread(false),
             m_tickIntervalMs(tickInterval),
             m_timerResolutionWasSet(false) {
           RF_CORE_INFO("GameServerEngine: Constructed. Tick Interval: {}ms", m_tickIntervalMs.count());
        }

        GameServerEngine::~GameServerEngine() {
            RF_CORE_INFO("GameServerEngine: Destructor called. Ensuring simulation loop is stopped.");
            StopSimulationLoop();
        }

        RiftForged::GameLogic::PlayerManager& GameServerEngine::GetPlayerManager() {
            return m_playerManager;
        }

        // Optional const version
        const RiftForged::GameLogic::PlayerManager& GameServerEngine::GetPlayerManager() const {
            return m_playerManager;
        }

        void GameServerEngine::SetPacketHandler(RiftForged::Networking::UDPPacketHandler* handler) {
            m_packetHandlerPtr = handler;
            if (m_packetHandlerPtr) {
                RF_CORE_INFO("GameServerEngine: UDPPacketHandler has been set.");
            }
            else {
                RF_CORE_WARN("GameServerEngine: UDPPacketHandler was set to nullptr.");
            }
        }

        std::vector<RiftForged::Networking::NetworkEndpoint> GameServerEngine::GetAllActiveSessionEndpoints() const {
            std::lock_guard<std::mutex> lock(m_sessionMapsMutex); // Assuming m_sessionMapsMutex protects m_playerIdToEndpointMap
            std::vector<RiftForged::Networking::NetworkEndpoint> endpoints;
            endpoints.reserve(m_playerIdToEndpointMap.size());
            for (const auto& pair : m_playerIdToEndpointMap) {
                endpoints.push_back(pair.second);
            }
            return endpoints;
        }

        void GameServerEngine::StartSimulationLoop() {
            if (m_isSimulatingThread.load(std::memory_order_relaxed)) {
                RF_CORE_WARN("GameServerEngine: Simulation loop already running.");
                return;
            }
            RF_CORE_INFO("GameServerEngine: Starting simulation loop...");

#ifdef _WIN32
            MMRESULT timeResult = timeBeginPeriod(1);
            if (timeResult != TIMERR_NOERROR) {
                RF_CORE_WARN("GameServerEngine: Failed to set timer resolution to 1ms. Error code: {}. Timing precision may be affected.", timeResult);
                m_timerResolutionWasSet = false;
            }
            else {
                RF_CORE_INFO("GameServerEngine: Timer resolution successfully set to 1ms.");
                m_timerResolutionWasSet = true;
            }
#endif

            m_isSimulatingThread.store(true, std::memory_order_release);
            try {
                m_simulationThread = std::thread(&GameServerEngine::SimulationTick, this);
            }
            catch (const std::system_error& e) {
                RF_CORE_CRITICAL("GameServerEngine: Failed to create simulation thread: {}", e.what());
                m_isSimulatingThread.store(false, std::memory_order_relaxed);
#ifdef _WIN32
                if (m_timerResolutionWasSet) {
                    timeEndPeriod(1);
                    m_timerResolutionWasSet = false;
                }
#endif
            }
        }

        void GameServerEngine::StopSimulationLoop() {
            bool wasSimulating = m_isSimulatingThread.exchange(false, std::memory_order_acq_rel);
            if (!wasSimulating) {
                if (m_simulationThread.joinable() && m_simulationThread.get_id() != std::this_thread::get_id()) {
                    m_shutdownThreadCv.notify_one();
                    m_simulationThread.join();
                    RF_CORE_INFO("GameServerEngine: Lingering simulation thread joined.");
                }
#ifdef _WIN32
                if (m_timerResolutionWasSet) {
                    MMRESULT endResult = timeEndPeriod(1);
                    if (endResult != TIMERR_NOERROR) {
                        RF_CORE_ERROR("GameServerEngine: Failed to restore timer resolution on non-running stop. Error code: {}", endResult);
                    }
                    else {
                        RF_CORE_INFO("GameServerEngine: Timer resolution restored on non-running stop.");
                    }
                    m_timerResolutionWasSet = false;
                }
#endif
                return;
            }

            RF_CORE_INFO("GameServerEngine: Signaling simulation loop to stop...");
            m_shutdownThreadCv.notify_one();

            if (m_simulationThread.joinable()) {
                if (m_simulationThread.get_id() == std::this_thread::get_id()) {
                    RF_CORE_CRITICAL("GameServerEngine::StopSimulationLoop called from simulation thread itself! This would deadlock.");
                }
                else {
                    m_simulationThread.join();
                    RF_CORE_INFO("GameServerEngine: Simulation loop stopped.");
#ifdef _WIN32
                    if (m_timerResolutionWasSet) {
                        MMRESULT endResult = timeEndPeriod(1);
                        if (endResult != TIMERR_NOERROR) {
                            RF_CORE_ERROR("GameServerEngine: Failed to restore timer resolution. Error code: {}", endResult);
                        }
                        else {
                            RF_CORE_INFO("GameServerEngine: Timer resolution successfully restored.");
                        }
                        m_timerResolutionWasSet = false;
                    }
#endif
                }
            }
            else {
                RF_CORE_WARN("GameServerEngine: Simulation thread was not joinable upon stop request.");
#ifdef _WIN32
                if (m_timerResolutionWasSet) {
                    timeEndPeriod(1);
                    m_timerResolutionWasSet = false;
                    RF_CORE_INFO("GameServerEngine: Timer resolution restored (thread not joinable fallback).");
                }
#endif
            }
        }


        // --- Session Management Methods ---
        uint64_t GameServerEngine::OnClientAuthenticatedAndJoining(
            const RiftForged::Networking::NetworkEndpoint& newEndpoint,
            const std::string& characterIdToLoad) {

            std::string endpointKey = newEndpoint.ToString();
            RF_CORE_INFO("GameServerEngine: Client joining from endpoint [{}]. Character to load: '{}'", endpointKey, characterIdToLoad.empty() ? "New/Default" : characterIdToLoad);

            uint64_t existingPlayerId = 0;
            {
                std::lock_guard<std::mutex> lock(m_sessionMapsMutex);
                auto it = m_endpointKeyToPlayerIdMap.find(endpointKey);
                if (it != m_endpointKeyToPlayerIdMap.end()) {
                    existingPlayerId = it->second;
                    RF_CORE_WARN("GameServerEngine: Endpoint [{}] already associated with PlayerId {}. Re-joining logic needed or kick old.", endpointKey, existingPlayerId);
                    // For simplicity, allow re-association, potentially kicking old session implicitly if new one fully replaces it.
                    // Or, return existingPlayerId; a more robust system would handle this carefully.
                    // Let's assume for now we proceed to create a new mapping if characterId is different or some other logic.
                    // To be safe, if an endpoint is reused, we should probably clean up the old PlayerId associated with it.
                    // For now, let's simplify and assume it is a genuinely new session for this example.
                    // If found, it might be better to call OnClientDisconnected for that old player id first.
                    // For this iteration, if found, we just return it to avoid duplicate PlayerId creation for same endpoint.
                    return existingPlayerId;
                }
            }

            uint64_t newPlayerId = m_playerManager.GetNextAvailablePlayerID();
            if (newPlayerId == 0) {
                RF_CORE_CRITICAL("GameServerEngine: PlayerManager returned invalid new PlayerId (0).");
                return 0;
            }


            RiftForged::Networking::Shared::Vec3 spawnPos(0.f, 0.f, 1.5f);
            RiftForged::Networking::Shared::Quaternion spawnOrient(0.f, 0.f, 0.f, 1.f);
            // TODO: Load actual character data and spawn location using characterIdToLoad

            GameLogic::ActivePlayer* player = m_playerManager.CreatePlayer(newPlayerId, spawnPos, spawnOrient);
            if (!player) {
                RF_CORE_ERROR("GameServerEngine: Failed to create ActivePlayer for PlayerId {}.", newPlayerId);
                SendJoinFailedResponse(m_packetHandlerPtr, newEndpoint, "Player creation failed.", 1001); // Example reason_code
                return 0;
            }
            {
                std::lock_guard<std::mutex> lock(m_sessionMapsMutex);
                m_endpointKeyToPlayerIdMap[endpointKey] = newPlayerId;
                m_playerIdToEndpointMap[newPlayerId] = newEndpoint;
            }

            m_gameplayEngine.InitializePlayerInWorld(player, spawnPos, spawnOrient);

            RF_CORE_INFO("GameServerEngine: Player {} successfully created and initialized for endpoint [{}].", newPlayerId, endpointKey);

            if (m_packetHandlerPtr) {
                flatbuffers::FlatBufferBuilder builder;
                uint16_t tick_rate_hz = static_cast<uint16_t>(1000 / m_tickIntervalMs.count()); // Calculate from interval
                auto welcome_message = builder.CreateString("Welcome to RiftForged!"); // Or more dynamic

                auto join_success_payload = RF_S2C::CreateS2C_JoinSuccessMsg(builder,
                    newPlayerId,
                    welcome_message,
                    tick_rate_hz);

                RF_S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                root_builder.add_payload_type(RF_S2C::S2C_UDP_Payload_S2C_JoinSuccessMsg);
                root_builder.add_payload(join_success_payload.Union());
                auto root_offset = root_builder.Finish();
                builder.Finish(root_offset);

                std::vector<uint8_t> payloadBytes(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
                // Consider if this should be reliable or unreliable. Join success is usually important.
                m_packetHandlerPtr->SendReliablePacket(newEndpoint, RF_Net::MessageType::S2C_JoinSuccess, payloadBytes);
            }
            return newPlayerId;
        }

        void GameServerEngine::ProcessDisconnectRequests() {
            std::deque<Networking::NetworkEndpoint> requestsToProcess;
            {
                std::lock_guard<std::mutex> lock(m_disconnectRequestQueueMutex);
                if (m_disconnectRequestQueue.empty()) return;
                requestsToProcess.swap(m_disconnectRequestQueue);
            }
            RF_ENGINE_TRACE("SIM_TICK: Processing %zu queued disconnect requests.", requestsToProcess.size());
            for (const auto& ep : requestsToProcess) {
                OnClientDisconnected(ep);
            }
        }


        void GameServerEngine::OnClientDisconnected(const RiftForged::Networking::NetworkEndpoint& endpoint) {
            std::string endpointKey = endpoint.ToString();
            RF_CORE_INFO("GameServerEngine: Client disconnected from endpoint [{}]", endpointKey);

            uint64_t playerIdToDisconnect = 0;
            {
                std::lock_guard<std::mutex> lock(m_sessionMapsMutex);
                auto it = m_endpointKeyToPlayerIdMap.find(endpointKey);
                if (it != m_endpointKeyToPlayerIdMap.end()) {
                    playerIdToDisconnect = it->second;
                    m_endpointKeyToPlayerIdMap.erase(it);
                    m_playerIdToEndpointMap.erase(playerIdToDisconnect);
                }
                else {
                    RF_CORE_WARN("GameServerEngine: Received disconnect for unknown or already removed endpoint [{}].", endpointKey);
                    return;
                }
            }

            if (playerIdToDisconnect != 0) {
                RF_CORE_INFO("GameServerEngine: Processing disconnect for PlayerId {}.", playerIdToDisconnect);
                // m_gameplayEngine.HandlePlayerDeparture(playerIdToDisconnect); // TODO: Define this in GameplayEngine if needed for game logic cleanup
                m_physicsEngine.UnregisterPlayerController(playerIdToDisconnect); // Assumes method exists
                m_playerManager.RemovePlayer(playerIdToDisconnect);
                // TODO: Save player data, notify other systems (guilds, parties)
            }
        }

        uint64_t GameServerEngine::GetPlayerIdForEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint) const {
            std::string endpointKey = endpoint.ToString();
            std::lock_guard<std::mutex> lock(m_sessionMapsMutex);
            auto it = m_endpointKeyToPlayerIdMap.find(endpointKey);
            if (it != m_endpointKeyToPlayerIdMap.end()) {
                return it->second;
            }
            return 0;
        }

        std::optional<RiftForged::Networking::NetworkEndpoint> GameServerEngine::GetEndpointForPlayerId(uint64_t playerId) const {
            std::lock_guard<std::mutex> lock(m_sessionMapsMutex);
            auto it = m_playerIdToEndpointMap.find(playerId);
            if (it != m_playerIdToEndpointMap.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        void GameServerEngine::SubmitPlayerCommand(uint64_t playerId, std::any commandPayload) {
            if (playerId == 0) {
                RF_CORE_WARN("GameServerEngine::SubmitPlayerCommand: Received command with invalid playerId (0).");
                return;
            }
            std::lock_guard<std::mutex> lock(m_commandQueueMutex);
            m_incomingCommandQueue.push_back({ playerId, std::move(commandPayload) }); // Use std::move if appropriate
        }

        // In GameServerEngine.cpp
        void GameServerEngine::QueueClientJoinRequest(const Networking::NetworkEndpoint& endpoint, const std::string& characterIdToLoad) {
            std::lock_guard<std::mutex> lock(m_joinRequestQueueMutex);
            m_joinRequestQueue.push_back({ endpoint, characterIdToLoad });
            RF_CORE_INFO("GameServerEngine: Queued join request for endpoint [{}] with charId '{}'", endpoint.ToString(), characterIdToLoad);
        }

        void GameServerEngine::ProcessJoinRequests() {
            std::deque<ClientJoinRequest> requestsToProcess;
            {
                std::lock_guard<std::mutex> lock(m_joinRequestQueueMutex);
                if (m_joinRequestQueue.empty()) return;
                requestsToProcess.swap(m_joinRequestQueue);
            }

            RF_ENGINE_TRACE("SIM_TICK: Processing %zu queued join requests.", requestsToProcess.size());
            for (const auto& req : requestsToProcess) {
                // Assuming authentication has occurred prior to this game server connection,
                // or is implicitly handled by the connection itself to this shard.
                // If further auth checks specific to the shard/character are needed, do them here.
                OnClientAuthenticatedAndJoining(req.endpoint, req.characterIdToLoad);
            }
        }

        void GameServerEngine::SendJoinFailedResponse(
            RF_Net::UDPPacketHandler* packetHandler,
            const Networking::NetworkEndpoint& recipient,
            const std::string& reason_message_str,
            int16_t reason_code) {
            if (!packetHandler) {
                RF_CORE_ERROR("GameServerEngine::SendJoinFailedResponse: Packet handler is null. Cannot send to [{}].", recipient.ToString());
                return;
            }

            flatbuffers::FlatBufferBuilder builder;
            auto reason_fb_str = builder.CreateString(reason_message_str);
            auto join_failed_payload = RF_S2C::CreateS2C_JoinFailedMsg(builder, reason_fb_str, reason_code);

            RF_S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
            root_builder.add_payload_type(RF_S2C::S2C_UDP_Payload_S2C_JoinFailedMsg);
            root_builder.add_payload(join_failed_payload.Union());
            auto root_offset = root_builder.Finish();
            builder.Finish(root_offset);

            std::vector<uint8_t> payloadBytes(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
            packetHandler->SendReliablePacket(recipient, RF_Net::MessageType::S2C_JoinFailed, payloadBytes);
            RF_CORE_INFO("GameServerEngine: Sent JoinFailed (Code: {}) to [{}] Reason: {}", reason_code, recipient.ToString(), reason_message_str);
        }

        void GameServerEngine::ProcessPlayerCommands() {
            std::deque<QueuedPlayerCommand> commandsToProcess;
            {
                std::lock_guard<std::mutex> lock(m_commandQueueMutex);
                if (m_incomingCommandQueue.empty()) return;
                commandsToProcess.swap(m_incomingCommandQueue);
            }

            RF_ENGINE_TRACE("SIM_TICK: Processing %zu queued player commands.", commandsToProcess.size());

            for (const auto& queuedCmd : commandsToProcess) {
                GameLogic::ActivePlayer* player = m_playerManager.FindPlayerById(queuedCmd.playerId);
                if (!player) {
                    RF_CORE_WARN("GameServerEngine::ProcessPlayerCommands: Player {} not found for command processing.", queuedCmd.playerId);
                    continue;
                }

                try {
                    // C2S Movement Input
                    if (queuedCmd.commandPayload.type() == typeid(RiftForged::Networking::UDP::C2S::C2S_MovementInputMsgT)) {
                        const auto& cmd = std::any_cast<const RiftForged::Networking::UDP::C2S::C2S_MovementInputMsgT&>(queuedCmd.commandPayload);
                        if (cmd.local_direction_intent) {
                            player->last_processed_movement_intent = RiftForged::Networking::Shared::Vec3(
                                cmd.local_direction_intent->x(),
                                cmd.local_direction_intent->y(),
                                cmd.local_direction_intent->z()
                            );
                            player->was_sprint_intended = cmd.is_sprinting;
                        }
                        // C2S Turn Intent
                    }
                    else if (queuedCmd.commandPayload.type() == typeid(RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsgT)) {
                        const auto& cmd = std::any_cast<const RiftForged::Networking::UDP::C2S::C2S_TurnIntentMsgT&>(queuedCmd.commandPayload);
                        m_gameplayEngine.TurnPlayer(player, cmd.turn_delta_degrees);
                        // C2S RiftStep Activation
                    }
                    else if (queuedCmd.commandPayload.type() == typeid(RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsgT)) {
                        const auto& cmd = std::any_cast<const RiftForged::Networking::UDP::C2S::C2S_RiftStepActivationMsgT&>(queuedCmd.commandPayload);
                        GameLogic::RiftStepOutcome outcome = m_gameplayEngine.ExecuteRiftStep(player, cmd.directional_intent); // Use member, not method call

                        if (auto endpointOpt = GetEndpointForPlayerId(player->playerId)) {
                            flatbuffers::FlatBufferBuilder builder;
                            // Convert GameLogic::GameplayEffectInstance to Flatbuffers vector of S2C::RiftStepEffectPayloadUnion
                            std::vector<flatbuffers::Offset<void>> fb_entry_effects;
                            std::vector<int8_t> fb_entry_effects_type;
                            for (const auto& gei : outcome.entry_effects_data) {
                                fb_entry_effects_type.push_back(static_cast<int8_t>(gei.effect_payload_type));
                                // TODO: This switch needs to build the correct S2C effect table based on gei.effect_payload_type
                                // and add its offset to fb_entry_effects.
                                // For example, if gei.effect_payload_type == RiftStepEffectPayload_AreaDamage:
                                //   auto area_damage_offset = Networking::UDP::S2C::CreateEffect_AreaDamageData(builder, &gei.center_position, gei.radius, &gei.damage).Union();
                                //   fb_entry_effects.push_back(area_damage_offset);
                                // This part needs to be carefully implemented for each effect type.
                            }
                            // Similar loop for fb_exit_effects and fb_exit_effects_type

                            auto entry_effects_type_vec = builder.CreateVector(fb_entry_effects_type);
                            auto entry_effects_vec = builder.CreateVector(fb_entry_effects);
                            // auto exit_effects_type_vec = ...
                            // auto exit_effects_vec = ...

                            // Create S2C_RiftStepInitiatedMsg
                            // Note: The S2C_RiftStepInitiatedMsg in the provided header does not exactly match GameLogic::RiftStepOutcome.
                            // Specifically, S2C_RiftStepInitiatedMsg has 'calculated_target_position' but outcome has 'intended_target_position' and 'calculated_target_position'.
                            // Assuming 'calculated_target_position' from outcome is what's sent.
                            auto s2c_payload = Networking::UDP::S2C::CreateS2C_RiftStepInitiatedMsg(builder,
                                outcome.instigator_entity_id,
                                &outcome.actual_start_position,
                                &outcome.calculated_target_position, // Map from outcome field
                                &outcome.actual_final_position,
                                outcome.travel_duration_sec,
                                entry_effects_type_vec, entry_effects_vec,
                                0, 0, // Placeholder for exit effects
                                builder.CreateString(outcome.start_vfx_id),
                                builder.CreateString(outcome.travel_vfx_id),
                                builder.CreateString(outcome.end_vfx_id)
                            );
                            Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                            root_builder.add_payload_type(Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_RiftStepInitiated);
                            root_builder.add_payload(s2c_payload.Union());
                            auto root_offset = root_builder.Finish();
                            builder.Finish(root_offset);
                            std::vector<uint8_t> payloadBytes(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
                            if (m_packetHandlerPtr) m_packetHandlerPtr->SendReliablePacket(endpointOpt.value(), Networking::MessageType::S2C_RiftStepInitiated, payloadBytes); // Assuming generic MessageType
                        }

                        // C2S Basic Attack
                    }
                    else if (queuedCmd.commandPayload.type() == typeid(RiftForged::Networking::UDP::C2S::C2S_BasicAttackIntentMsgT)) {
                        const auto& cmd = std::any_cast<const RiftForged::Networking::UDP::C2S::C2S_BasicAttackIntentMsgT&>(queuedCmd.commandPayload);
                        if (cmd.aim_direction) {
                            RiftForged::Networking::Shared::Vec3 aimDir(cmd.aim_direction->x(), cmd.aim_direction->y(), cmd.aim_direction->z());
                            GameLogic::AttackOutcome outcome = m_gameplayEngine.ExecuteBasicAttack(player, aimDir, cmd.target_entity_id);
                            if (auto endpointOpt = GetEndpointForPlayerId(player->playerId)) {
                                if (outcome.spawned_projectile) {
                                    flatbuffers::FlatBufferBuilder builder;
                                    auto s2c_payload = Networking::UDP::S2C::CreateS2C_SpawnProjectileMsgDirect(builder,
                                        outcome.projectile_id,
                                        player->playerId, // owner_entity_id
                                        &outcome.projectile_start_position,
                                        &outcome.projectile_direction,
                                        outcome.projectile_speed,
                                        outcome.projectile_max_range,
                                        outcome.projectile_vfx_tag.c_str()
                                    );
                                    Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                                    root_builder.add_payload_type(Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_SpawnProjectile);
                                    root_builder.add_payload(s2c_payload.Union());
                                    auto root_offset = root_builder.Finish();
                                    builder.Finish(root_offset);
                                    std::vector<uint8_t> payloadBytes(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
                                    // Send to all relevant players, not just the attacker
                                    // For now, sending to attacker for testing. Broadcasting needs a separate mechanism.
                                    if (m_packetHandlerPtr) m_packetHandlerPtr->SendReliablePacket(endpointOpt.value(), Networking::MessageType::S2C_SpawnProjectile, payloadBytes);
                                }
                                // Handle outcome.damage_events for melee - construct and send S2C_CombatEventMsg
                                for (const auto& damage_detail : outcome.damage_events) {
                                    flatbuffers::FlatBufferBuilder builder;
                                    // Create CombatEvent_DamageDealtDetails from GameLogic::DamageApplicationDetails
                                    RiftForged::Networking::Shared::DamageInstance fb_dmg_inst(damage_detail.final_damage_dealt, damage_detail.damage_type, damage_detail.was_crit);
                                    auto damage_dealt_payload = Networking::UDP::S2C::CreateCombatEvent_DamageDealtDetails(builder,
                                        player->playerId, // source_entity_id
                                        damage_detail.target_id,
                                        &fb_dmg_inst,
                                        damage_detail.was_kill,
                                        outcome.is_basic_attack);

                                    auto combat_event_payload = Networking::UDP::S2C::CreateS2C_CombatEventMsg(builder,
                                        Networking::UDP::S2C::CombatEventType_DamageDealt,
                                        Networking::UDP::S2C::CombatEventPayload_DamageDealt, // type for union
                                        damage_dealt_payload.Union(), // actual payload
                                        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
                                    );
                                    Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                                    root_builder.add_payload_type(Networking::UDP::S2C::S2C_UDP_Payload::S2C_UDP_Payload_CombatEvent);
                                    root_builder.add_payload(combat_event_payload.Union());
                                    auto root_offset = root_builder.Finish();
                                    builder.Finish(root_offset);
                                    std::vector<uint8_t> payloadBytes(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
                                    // Send to relevant players (attacker, target, observers)
                                    if (m_packetHandlerPtr) m_packetHandlerPtr->SendReliablePacket(endpointOpt.value(), Networking::MessageType::S2C_CombatEvent, payloadBytes);
                                    if (damage_detail.target_id != player->playerId) { // Also send to target if different
                                        if (auto targetEndpointOpt = GetEndpointForPlayerId(damage_detail.target_id)) {
                                            if (m_packetHandlerPtr) m_packetHandlerPtr->SendReliablePacket(targetEndpointOpt.value(), Networking::MessageType::S2C_CombatEvent, payloadBytes);
                                        }
                                    }
                                }
                            }
                        }
                        // C2S Use Ability
                    }
                    else if (queuedCmd.commandPayload.type() == typeid(RiftForged::Networking::UDP::C2S::C2S_UseAbilityMsgT)) {
                        const auto& cmd = std::any_cast<const RiftForged::Networking::UDP::C2S::C2S_UseAbilityMsgT&>(queuedCmd.commandPayload);
                        RF_CORE_INFO("Player {} trying to use ability {}. TargetEntity: {}. TargetPos specified: {}",
                            player->playerId, cmd.ability_id, cmd.target_entity_id, cmd.target_position ? "Yes" : "No");
                        // TODO: Call a generic m_gameplayEngine.ExecutePlayerAbility(player, cmd.ability_id, cmd.target_entity_id, cmd.target_position.get());
                        // Process the outcome from that and send appropriate S2C messages.
                    }
                    else {
                        RF_CORE_WARN("GameServerEngine::ProcessPlayerCommands: Unknown command payload type for player {}. Type: {}", queuedCmd.playerId, queuedCmd.commandPayload.type().name());
                    }
                }
                catch (const std::bad_any_cast& e) {
                    RF_CORE_ERROR("GameServerEngine::ProcessPlayerCommands: Bad std::any_cast for player {}: {}. Payload type_info: {}", queuedCmd.playerId, e.what(), queuedCmd.commandPayload.type().name());
                }
            }
        }

        void GameServerEngine::SimulationTick() {
            // (Timer setup logic for thread ID and last_tick_time)
            std::stringstream ss_thread_id; ss_thread_id << std::this_thread::get_id();
            RF_CORE_INFO("GameServerEngine: SimulationTick thread started (ID: {})", ss_thread_id.str()); // Changed to CORE
            auto last_tick_time = std::chrono::steady_clock::now();

            while (m_isSimulatingThread.load(std::memory_order_acquire)) {
                auto current_tick_start_time = std::chrono::steady_clock::now();
                auto delta_time_duration = current_tick_start_time - last_tick_time;
                float delta_time_sec = std::chrono::duration<float>(delta_time_duration).count();
                if (delta_time_sec <= 0.0f) { // Avoid zero or negative delta on first few ticks or system clock issues
                    delta_time_sec = std::chrono::duration<float>(m_tickIntervalMs).count() * 0.5f; // Use a small fraction of tick interval
                    RF_ENGINE_TRACE("SIM_TICK: Clamped non-positive delta_time_sec to %.4f sec", delta_time_sec);
                }
                if (delta_time_sec > 0.2f) { // Max step time to prevent spiral of death
                    RF_CORE_WARN("SIM_TICK: Large delta_time_sec detected: %.4f sec. Clamping to 0.2 sec.", delta_time_sec);
                    delta_time_sec = 0.2f;
                }
                last_tick_time = current_tick_start_time;

                // --- 0. Process Connection Management --- // New conceptual step
                ProcessJoinRequests();
                ProcessDisconnectRequests(); // Add this when implemented


                // --- 1. Process Queued Player Commands ---
                ProcessPlayerCommands(); // Updates player intents like movement vectors based on network input

                // --- 2. Update Gameplay Logic (uses intents, applies timed effects, AI) ---
                std::vector<GameLogic::ActivePlayer*> players_for_gameplay_update =
                    m_playerManager.GetAllActivePlayerPointersForUpdate();

                for (GameLogic::ActivePlayer* player : players_for_gameplay_update) {
                    if (player) {
                        // GameplayEngine uses the intents stored on ActivePlayer by ProcessPlayerCommands
                        m_gameplayEngine.ProcessMovement(player, player->last_processed_movement_intent, player->was_sprint_intended, delta_time_sec);
                        // TODO: m_gameplayEngine.UpdatePlayerLogic(player, delta_time_sec); // For buffs, DoTs, ability state machines etc.
                    }
                }
                // TODO: m_gameplayEngine.UpdateNPCsAndWorldEvents(delta_time_sec);

                // --- 3. Physics Simulation Step ---
                m_physicsEngine.StepSimulation(delta_time_sec);

                // --- 4. Post-Physics Updates & Game Logic Reconcile ---
                for (GameLogic::ActivePlayer* player : players_for_gameplay_update) {
                    if (player && player->playerId != 0) { // Ensure player is valid
                        physx::PxController* px_controller = m_physicsEngine.GetPlayerController(player->playerId);
                        if (px_controller) {
                            RiftForged::Networking::Shared::Vec3 new_pos_from_physics = m_physicsEngine.GetCharacterControllerPosition(px_controller);
                            player->SetPosition(new_pos_from_physics);
                            // Orientation is usually set by TurnPlayer based on input, then synced to PxController.
                            // If physics (e.g. ragdoll, knockback rotation) can change orientation, sync it back here too:
                            // RiftForged::Networking::Shared::Quaternion new_orient_from_physics = m_physicsEngine.GetCharacterControllerOrientation(player->playerId);
                            // player->SetOrientation(new_orient_from_physics);
                        }
                    }
                }

                // --- 5. State Synchronization ---
                std::vector<const GameLogic::ActivePlayer*> active_players_for_sync_const =
                    std::as_const(m_playerManager).GetAllActivePlayerPointersForUpdate(); // Calls the const overload

                if (!active_players_for_sync_const.empty()) {
                    RF_ENGINE_TRACE("SIM_TICK: Checking %zu active players for state sync.", active_players_for_sync_const.size());
                }

                for (const GameLogic::ActivePlayer* player_const : active_players_for_sync_const) { // Renamed to avoid conflict
                    if (player_const && player_const->isDirty.load(std::memory_order_acquire)) {
                        std::optional<Networking::NetworkEndpoint> endpointOpt = GetEndpointForPlayerId(player_const->playerId);
                        if (endpointOpt) {
                            const Networking::NetworkEndpoint& playerEndpoint = endpointOpt.value();
                            RF_ENGINE_DEBUG("SIM_TICK: Player {} is dirty. Pos: ({:.1f},{:.1f},{:.1f}). Prepping S2C_EntityStateUpdate for endpoint [{}].",
                                player_const->playerId, player_const->position.x(), player_const->position.y(), player_const->position.z(), playerEndpoint.ToString());

                            flatbuffers::FlatBufferBuilder builder(1024); // Increased default size a bit

                            // S2C_EntityStateUpdateMsg construction:
                            RiftForged::Networking::Shared::Vec3 pos_val(player_const->position.x(), player_const->position.y(), player_const->position.z());
                            RiftForged::Networking::Shared::Quaternion orient_val(player_const->orientation.x(), player_const->orientation.y(), player_const->orientation.z(), player_const->orientation.w());

                            flatbuffers::Offset<flatbuffers::Vector<uint32_t>> active_effects_fb_vector_offset;
                            if (!player_const->activeStatusEffects.empty()) {
                                std::vector<uint32_t> effects_as_uints;
                                effects_as_uints.reserve(player_const->activeStatusEffects.size());
                                for (const auto& effect_enum : player_const->activeStatusEffects) {
                                    effects_as_uints.push_back(static_cast<uint32_t>(effect_enum));
                                }
                                active_effects_fb_vector_offset = builder.CreateVector(effects_as_uints);
                            }

                            uint64_t server_timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();

                            auto state_payload_offset = Networking::UDP::S2C::CreateS2C_EntityStateUpdateMsg(
                                builder, player_const->playerId, &pos_val, &orient_val,
                                player_const->currentHealth, player_const->maxHealth, player_const->currentWill, player_const->maxWill,
                                server_timestamp_ms,
                                player_const->animationStateId, active_effects_fb_vector_offset);

                            Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                            root_builder.add_payload_type(Networking::UDP::S2C::S2C_UDP_Payload_EntityStateUpdate);
                            root_builder.add_payload(state_payload_offset.Union());
                            auto root_offset = root_builder.Finish();
                            builder.Finish(root_offset);

                            std::vector<uint8_t> payloadBytes(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());

                            if (m_packetHandlerPtr) { // Check if the pointer is valid
                                if (!m_packetHandlerPtr->SendUnreliablePacket( // Use the pointer
                                    playerEndpoint,
                                    RiftForged::Networking::MessageType::S2C_EntityStateUpdate,
                                    payloadBytes)) {
                                    RF_NETWORK_ERROR("GameServerEngine: SendUnreliablePacket failed for S2C_EntityStateUpdate for Player {} to {}",
                                        player_const->playerId, playerEndpoint.ToString());
                                }
                            }
                            else {
                                RF_NETWORK_ERROR("GameServerEngine: m_packetHandlerPtr is null. Cannot send S2C_EntityStateUpdate for Player {} to {}.",
                                    player_const->playerId, playerEndpoint.ToString());
                            }
                            // Safely cast away const to modify atomic isDirty flag. This is generally okay if this is the designated point for reset.
                            const_cast<GameLogic::ActivePlayer*>(player_const)->isDirty.store(false, std::memory_order_release);
                        }
                        else {
                            RF_CORE_WARN("GameServerEngine: No endpoint for dirty player {}, cannot sync. Resetting dirty flag.", player_const->playerId);
                            const_cast<GameLogic::ActivePlayer*>(player_const)->isDirty.store(false, std::memory_order_release);
                        }
                    }
                }

                // --- 6. Control Tick Rate ---
                // (Same as before)
                auto current_tick_end_time = std::chrono::steady_clock::now();
                auto tick_processing_duration = current_tick_end_time - current_tick_start_time;
                auto sleep_for = m_tickIntervalMs - tick_processing_duration;

                if (m_isSimulatingThread.load(std::memory_order_relaxed)) {
                    if (sleep_for > std::chrono::milliseconds(0)) {
                        std::unique_lock<std::mutex> lock(m_shutdownThreadMutex);
                        m_shutdownThreadCv.wait_for(lock, sleep_for, [this] {
                            return !m_isSimulatingThread.load(std::memory_order_relaxed);
                            });
                    }
                    else if (sleep_for < std::chrono::milliseconds(0)) {
                        RF_ENGINE_WARN("SimulationTick: Tick processing duration ({:.2f}ms) exceeded interval ({}ms). Server may be overloaded.",
                            std::chrono::duration<double, std::milli>(tick_processing_duration).count(),
                            m_tickIntervalMs.count());
                    }
                }
            } // End while(m_isSimulatingThread)

            std::stringstream ss_exit_thread_id_end; ss_exit_thread_id_end << std::this_thread::get_id(); // Use different name
            RF_CORE_INFO("GameServerEngine: SimulationTick thread exiting gracefully (ID: {})", ss_exit_thread_id_end.str()); // Changed to CORE
        }

    } // namespace Server
} // namespace RiftForged