// File: UDPServer/PacketManagement/Handlers_C2S/MessageDispatcher.cpp
// RiftForged Game Development Team
// Copyright (c) 2025-2008 RiftForged Game Development Team

#include "GameServerEngine.h"
#include "../Gameplay/ActivePlayer.h"
#include <iostream> // For logging (replace with your logger)
#include <vector>   // For std::vector
#include <cstring>  // For memcpy
#include <windows.h> // For Sleep (or use std::this_thread::sleep_for in C++11+)
#include <timeapi.h> // For MMRESULT, TIMERR_NOERROR, timeBeginPeriod, timeEndPeriod

#pragma comment(lib, "Winmm.lib") 


namespace RiftForged {
    namespace Server {

        GameServerEngine::GameServerEngine(
            RiftForged::GameLogic::PlayerManager& playerManager,
            RiftForged::Gameplay::GameplayEngine& gameplayEngine,
            RiftForged::Networking::UDPSocketAsync& udpSocket,
            RiftForged::Physics::PhysicsEngine& physicsEngine,
            std::chrono::milliseconds tickInterval)
            : m_playerManager(playerManager),         //
            m_gameplayEngine(gameplayEngine),       //
            m_physicsEngine(physicsEngine),         //
            m_udpSocket(udpSocket),                 //
            m_isSimulatingThread(false),            //
            m_tickIntervalMs(tickInterval) {        //
            RF_CORE_INFO("GameServerEngine: Constructed. Tick Interval: {}ms", m_tickIntervalMs.count()); //
        }

        GameServerEngine::~GameServerEngine() {
            RF_CORE_INFO("GameServerEngine: Destructor called. Ensuring simulation loop is stopped."); //
            StopSimulationLoop(); //
        }

        void GameServerEngine::StartSimulationLoop() {
            if (m_isSimulatingThread.load(std::memory_order_relaxed)) { //
                RF_CORE_WARN("GameServerEngine: Simulation loop already running."); //
                return;
            }
            RF_CORE_INFO("GameServerEngine: Starting simulation loop..."); //

            MMRESULT timeResult = timeBeginPeriod(1); 
            if (timeResult != TIMERR_NOERROR) {
                RF_CORE_WARN("GameServerEngine: Failed to set timer resolution to 1ms. Error code: {}. Timing precision may be affected.", timeResult);
                // You might choose to not start if this fails, or proceed with potentially less accurate timing.
            } else {
                RF_CORE_INFO("GameServerEngine: Timer resolution successfully set to 1ms.");
            }

            m_isSimulatingThread.store(true, std::memory_order_release); //
            try {
                m_simulationThread = std::thread(&GameServerEngine::SimulationTick, this); //
            }
            catch (const std::system_error& e) {
                RF_CORE_CRITICAL("GameServerEngine: Failed to create simulation thread: {}", e.what()); //
                m_isSimulatingThread.store(false, std::memory_order_relaxed);
            }
        }

        void GameServerEngine::StopSimulationLoop() {
            if (!m_isSimulatingThread.exchange(false, std::memory_order_acq_rel)) {
                // If it was already false, we might still need to join if called concurrently
                // or if Stop was called before Start completed thread creation but after m_isSimulatingThread was set.
                if (m_simulationThread.joinable() && m_simulationThread.get_id() != std::this_thread::get_id()) {
                    m_shutdownThreadCv.notify_one();
                    m_simulationThread.join();
                }
                // If it was already false and not joinable, or called from itself (which is an error state handled later),
                // it means the loop wasn't really "running" from this call's perspective for timer period management.
                // RF_CORE_INFO("GameServerEngine: StopSimulationLoop called but loop was not considered actively running by this call.");
                return; // Avoid attempting to restore timer period if it wasn't set by a corresponding Start
            }

            RF_CORE_INFO("GameServerEngine: Signaling simulation loop to stop...");
            m_shutdownThreadCv.notify_one();

            if (m_simulationThread.joinable()) {
                if (m_simulationThread.get_id() == std::this_thread::get_id()) {
                    RF_CORE_CRITICAL("GameServerEngine::StopSimulationLoop called from simulation thread itself! This would deadlock.");
                    // Cannot join self, and cannot safely restore timer period here as thread is still running.
                    // This case means m_isSimulatingThread was true, so timer period *was* set.
                    // This is a design issue if it happens. We should NOT restore timer here as thread is not finished.
                }
                else {
                    m_simulationThread.join();
                    RF_CORE_INFO("GameServerEngine: Simulation loop stopped.");

                    // --- BEGIN MODIFICATION: Restore timer resolution ---
                    // This should be called after the thread has successfully stopped.
                    MMRESULT timeResult = timeEndPeriod(1); // Use the same period value as in timeBeginPeriod
                    if (timeResult != TIMERR_NOERROR) {
                        RF_CORE_ERROR("GameServerEngine: Failed to restore timer resolution. Error code: {}. This may affect system power or other applications.", timeResult);
                    }
                    else {
                        RF_CORE_INFO("GameServerEngine: Timer resolution successfully restored.");
                    }
                    // --- END MODIFICATION ---
                }
            }
            else {
                RF_CORE_WARN("GameServerEngine: Simulation thread was not joinable upon stop request, but m_isSimulatingThread was true. Timer period might not be restored if thread never ran/exited cleanly.");
                // If the thread wasn't joinable but m_isSimulatingThread was true, it implies an unusual state.
                // It's possible the thread never actually started or crashed.
                // We might still want to attempt to restore the timer period as a best-effort,
                // assuming timeBeginPeriod was called.
                MMRESULT timeResult = timeEndPeriod(1);
                if (timeResult != TIMERR_NOERROR) {
                    RF_CORE_ERROR("GameServerEngine: Attempted to restore timer resolution (thread not joinable), but failed. Error code: {}", timeResult);
                }
                else {
                    RF_CORE_INFO("GameServerEngine: Attempted to restore timer resolution (thread not joinable).");
                }
            }


            // Shutdown Physics Engine after simulation thread has stopped
            m_physicsEngine.Shutdown();
            RF_CORE_INFO("GameServerEngine: PhysicsEngine Shutdown.");
        }

        void GameServerEngine::SimulationTick() {
            // ... (Thread ID logging and delta_time calculation as in your code) ...
            std::stringstream ss_thread_id;
            ss_thread_id << std::this_thread::get_id();
            RF_CORE_INFO("GameServerEngine: SimulationTick thread started (ID: {})", ss_thread_id.str());
            auto last_tick_time = std::chrono::steady_clock::now();

            while (m_isSimulatingThread.load(std::memory_order_acquire)) {
                auto current_tick_start_time = std::chrono::steady_clock::now();
                auto delta_time_duration = current_tick_start_time - last_tick_time;
                float delta_time_sec = std::chrono::duration<float>(delta_time_duration).count();
                last_tick_time = current_tick_start_time;

                // --- 1. Process Incoming Network Commands / Update Player Intents ---
                // This is where you'd typically process a queue of commands populated by your network threads (UDPSocketAsync).
                // These commands would have already been parsed by PacketProcessor and dispatched by MessageDispatcher,
                // leading to calls like m_gameplayEngine.ProcessMovement(), m_gameplayEngine.ExecuteRiftStep(), etc.
                // For now, we assume these calls happen and set player->isDirty flags correctly.
                // m_gameplayEngine.ProcessPendingInputs(delta_time_sec); // Conceptual

                // --- 2. Execute Periodic Game Logic (AI, Buffs/Debuffs, etc.) ---
                //m_gameplayEngine.UpdatePeriodicLogic(delta_time_sec); // Your placeholder

                // --- 3. Physics Simulation Step ---
                // <<< UNCOMMENTED AND ENSURED m_physicsEngine IS USED >>>
                m_physicsEngine.StepSimulation(delta_time_sec); //

                // --- 4. Post-Physics Updates & Game Logic ---
                // If your GameplayEngine::ProcessMovement (and similar methods) update ActivePlayer state
                // *immediately* after calling PhysicsEngine->MoveCharacterController, then the ActivePlayer
                // objects are already up-to-date with their physics positions.
                // If not, you would iterate players here, get their updated pose from PhysicsEngine,
                // and update ActivePlayer, setting 'isDirty'.
                // m_gameplayEngine.ApplyPhysicsResultsToGameLogic(); // Conceptual if needed

                // --- 5. State Synchronization ("Town Crier") ---
                // Using GetAllActivePlayerPointersForUpdate for efficiency
                std::vector<RiftForged::GameLogic::ActivePlayer*> active_players_for_sync =
                    m_playerManager.GetAllActivePlayerPointersForUpdate(); //

                if (!active_players_for_sync.empty()) { // (modified to use new vector name)
                    RF_ENGINE_TRACE("SIM_TICK: Checking {} active players for state sync.", active_players_for_sync.size()); //
                }

                for (RiftForged::GameLogic::ActivePlayer* player : active_players_for_sync) { // Iterate by pointer
                    if (player && player->isDirty.load(std::memory_order_acquire)) { //
                        RF_ENGINE_DEBUG("SIM_TICK: Player {} at [{}] is dirty. Pos: ({:.1f},{:.1f},{:.1f}). Prepping S2C_EntityStateUpdate.", //
                            player->playerId, player->networkEndpoint.ToString(),
                            player->position.x(), player->position.y(), player->position.z());

                        flatbuffers::FlatBufferBuilder builder(512); //

                        // ... (active_effects_fb_vector_offset creation as per your code - looks good) ...
                        flatbuffers::Offset<flatbuffers::Vector<uint32_t>> active_effects_fb_vector_offset;
                        if (!player->activeStatusEffects.empty()) {
                            std::vector<uint32_t> effects_as_uints;
                            effects_as_uints.reserve(player->activeStatusEffects.size());
                            for (const auto& effect_enum : player->activeStatusEffects) {
                                effects_as_uints.push_back(static_cast<uint32_t>(effect_enum));
                            }
                            if (!effects_as_uints.empty()) { // Ensure not to create empty vector if effects_as_uints is empty
                                active_effects_fb_vector_offset = builder.CreateVector(effects_as_uints);
                            }
                        }


                        uint64_t server_timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>( //
                            std::chrono::system_clock::now().time_since_epoch()).count();

                        auto state_payload_offset = RiftForged::Networking::UDP::S2C::CreateS2C_EntityStateUpdateMsg( //
                            builder, player->playerId, &player->position, &player->orientation,
                            player->currentHealth, player->maxHealth, player->currentWill, player->maxWill,
                            server_timestamp_ms,
                            player->animationStateId, active_effects_fb_vector_offset
                        );

                        RiftForged::Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder); //
                        root_builder.add_payload_type(RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_EntityStateUpdate); //
                        root_builder.add_payload(state_payload_offset.Union()); //

                        auto root_offset = root_builder.Finish(); // <<< CORRECTED: Capture root offset
                        builder.Finish(root_offset);              // <<< CORRECTED: Pass root offset

                        RiftForged::Networking::GamePacketHeader s2c_header(RiftForged::Networking::MessageType::S2C_EntityStateUpdate); //
                        // TODO: Populate s2c_header.sequenceNumber for reliability

                        std::vector<char> send_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize()); //
                        memcpy(send_buffer.data(), &s2c_header, RiftForged::Networking::GetGamePacketHeaderSize()); //
                        memcpy(send_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize()); //

                        // Send only to the specific player whose state this is.
                        // Broadcasting all individual state updates is inefficient.
                        if (!m_udpSocket.SendRawTo(player->networkEndpoint, send_buffer.data(), static_cast<int>(send_buffer.size()))) { //
                            RF_NETWORK_ERROR("GameServerEngine: SendRawTo failed for S2C_EntityStateUpdateMsg for Player {} to {}", //
                                player->playerId, player->networkEndpoint.ToString());
                        }
                        else {
                            // RF_NETWORK_TRACE("GameServerEngine: Sent S2C_EntityStateUpdate for Player {} to {}", player->playerId, player->networkEndpoint.ToString());
                        }

                        player->isDirty.store(false, std::memory_order_release); //
                    }
                }
                // --- End State Synchronization ---

                // --- 6. Control Tick Rate ---
                // ... (Your existing tick rate control logic using m_shutdownThreadCv.wait_for - looks good) ...
                auto current_tick_end_time = std::chrono::steady_clock::now();
                auto tick_processing_duration = current_tick_end_time - current_tick_start_time;
                auto sleep_for = m_tickIntervalMs - tick_processing_duration;

                if (m_isSimulatingThread.load(std::memory_order_relaxed)) {
                    if (sleep_for > std::chrono::milliseconds(0)) {
                        std::unique_lock<std::mutex> lock(m_shutdownThreadMutex); // Assuming m_shutdownThreadMutex is a member
                        m_shutdownThreadCv.wait_for(lock, sleep_for, [this] {
                            return !m_isSimulatingThread.load(std::memory_order_relaxed);
                            });
                    }

                    else if (sleep_for < std::chrono::milliseconds(0)) { // Only log if we're genuinely behind
                        RF_ENGINE_WARN("SimulationTick: Tick processing duration ({:.2f}ms) exceeded interval ({}ms). Server may be overloaded.",
                            std::chrono::duration<double, std::milli>(tick_processing_duration).count(),
                            m_tickIntervalMs.count());
                        // Yield if significantly behind to avoid starving other threads, though wait_for with 0 would return immediately.
                        // std::this_thread::yield(); // Consider if needed if sleep_for is consistently negative by a large margin.
                    }
                }
            }
            // ... (Thread exit logging as in your code) ...
            std::stringstream ss_exit_thread_id;
            ss_exit_thread_id << std::this_thread::get_id();
            RF_CORE_INFO("GameServerEngine: SimulationTick thread exiting gracefully (ID: {})", ss_exit_thread_id.str());
        }

    } // namespace Server
} // namespace RiftForged