#include "GameServerEngine.h"
#include "../Gameplay/ActivePlayer.h"
#include <iostream> // For logging (replace with your logger)
#include <vector>   // For std::vector
#include <cstring>  // For memcpy

namespace RiftForged{
namespace Server {

GameServerEngine::GameServerEngine(
    RiftForged::GameLogic::PlayerManager& playerManager,
    RiftForged::Networking::UDPSocketAsync& udpSocket
    // GameplayEngine& gameplayEngine
) : m_playerManager(playerManager),
    m_udpSocket(udpSocket),
    // m_gameplayEngine(gameplayEngine),
    m_isSimulatingThread(false),
    m_tickIntervalMs(100) { // e.g., 10 state sync ticks per second
    std::cout << "GameServerEngine: Constructed." << std::endl;
}

GameServerEngine::~GameServerEngine() {
    StopSimulationLoop(); // Ensure thread is properly stopped and joined
}

void GameServerEngine::StartSimulationLoop() {
    if (m_isSimulatingThread.load()) {
        std::cout << "GameServerEngine: Simulation loop already running." << std::endl;
        return;
    }
    std::cout << "GameServerEngine: Starting simulation loop..." << std::endl;
    m_isSimulatingThread = true; // Set flag before starting thread
    m_simulationThread = std::thread(&GameServerEngine::SimulationTick, this);
}

void GameServerEngine::StopSimulationLoop() {
    if (!m_isSimulatingThread.exchange(false)) {
        // If it was already false, the thread is either not running or already stopping.
        // We still might want to join if joinable.
        if (m_simulationThread.joinable() && m_simulationThread.get_id() != std::this_thread::get_id()) {
            // It's tricky to call notify if it might already be exiting.
            // The primary signal is m_isSimulatingThread.
            // A one-time notify here is okay.
             m_shutdownThreadCv.notify_one(); // Attempt to wake it if it's in wait_for
            m_simulationThread.join();
        }
        return;
    }
    std::cout << "GameServerEngine: Signaling simulation loop to stop..." << std::endl;
    m_shutdownThreadCv.notify_one(); // Notify the simulation thread to wake up if sleeping
    if (m_simulationThread.joinable()) {
        if (m_simulationThread.get_id() == std::this_thread::get_id()) {
            std::cerr << "GameServerEngine::StopSimulationLoop called from simulation thread itself! This would deadlock." << std::endl;
        }
 else {
  m_simulationThread.join();
}
}
std::cout << "GameServerEngine: Simulation loop stopped." << std::endl;
}

bool GameServerEngine::IsSimulating() const {
    return m_isSimulatingThread.load();
}

void GameServerEngine::SimulationTick() {
    std::cout << "GameServerEngine: SimulationTick thread started (ID: " << std::this_thread::get_id() << ")." << std::endl;
    auto last_tick_processing_end_time = std::chrono::steady_clock::now();

    while (m_isSimulatingThread.load()) {
        auto tick_start_time = std::chrono::steady_clock::now();

        // --- 1. Game Logic Updates (AI, timed events, physics step if not event driven) ---
        // Example: m_gameplayEngine.UpdateWorldTick(deltaTime);
        // This is where NPCs would decide to move, buffs might tick, etc.
        // These actions would mark relevant ActivePlayer (or NPC) objects as 'isDirty'.
        // For now, we assume 'isDirty' is set by MessageHandlers calling GameplayEngine/ActivePlayer methods.

        // --- 2. State Synchronization ("Town Crier") ---
        // Get all client endpoints that have active players.
        std::vector<RiftForged::Networking::NetworkEndpoint> all_client_endpoints =
            m_playerManager.GetAllActiveClientEndpoints();

        for (const auto& endpoint : all_client_endpoints) {
            RiftForged::GameLogic::ActivePlayer* player = m_playerManager.FindPlayerByEndpoint(endpoint);

            if (player && player->isDirty) { // Check if player exists and is dirty
                // std::cout << "GameServerEngine: Player " << player->playerId << " is dirty. Broadcasting state to " << endpoint.ToString() << std::endl;

                flatbuffers::FlatBufferBuilder builder(512); // Builder for this specific message

                flatbuffers::Offset<flatbuffers::Vector<uint32_t>> active_effects_fb_vector_offset;
                if (!player->activeStatusEffects.empty()) {
                    std::vector<uint32_t> effects_as_uints;
                    effects_as_uints.reserve(player->activeStatusEffects.size());
                    for (const auto& effect_enum : player->activeStatusEffects) {
                        effects_as_uints.push_back(static_cast<uint32_t>(effect_enum));
                    }
                    active_effects_fb_vector_offset = builder.CreateVector(effects_as_uints);
                }

                auto state_payload_offset = RiftForged::Networking::UDP::S2C::CreateS2C_EntityStateUpdateMsg(
                    builder,
                    player->playerId,
                    &player->position,
                    &player->orientation,
                    player->currentHealth, player->maxHealth,
                    player->currentWill, player->maxWill,
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count(), // Server timestamp
                    player->animationStateId, // If 0 is default, FlatBuffers may optimize if not set
                    active_effects_fb_vector_offset // If offset is null, vector won't be written
                );

                RiftForged::Networking::UDP::S2C::Root_S2C_UDP_MessageBuilder root_builder(builder);
                root_builder.add_payload_type(RiftForged::Networking::UDP::S2C::S2C_UDP_Payload_EntityStateUpdate);
                root_builder.add_payload(state_payload_offset.Union());
                auto root_offset = root_builder.Finish();
                builder.Finish(root_offset);

                RiftForged::Networking::GamePacketHeader s2c_header(RiftForged::Networking::MessageType::S2C_EntityStateUpdate);
                // TODO: Populate reliability fields in s2c_header for this specific client 'endpoint'
                // e.g., s2c_header.sequenceNumber = m_playerManager.GetNextOutgoingSequenceFor(endpoint); 
                //      s2c_header.ackNumber = ... ; s2c_header.ackBitfield = ...;

                std::vector<char> send_buffer(RiftForged::Networking::GetGamePacketHeaderSize() + builder.GetSize());
                memcpy(send_buffer.data(), &s2c_header, RiftForged::Networking::GetGamePacketHeaderSize());
                memcpy(send_buffer.data() + RiftForged::Networking::GetGamePacketHeaderSize(), builder.GetBufferPointer(), builder.GetSize());

                // Send the state update specifically to this player's endpoint
                if (!m_udpSocket.SendTo(player->networkEndpoint, send_buffer.data(), static_cast<int>(send_buffer.size()))) {
                     std::cerr << "GameServerEngine: SendTo failed for S2C_EntityStateUpdateMsg to "
                               << player->networkEndpoint.ToString() << std::endl;
                }

                player->isDirty = false; // Clear the flag AFTER attempting to send
            }
        }
        // --- End State Synchronization ---

        // --- Calculate sleep time to maintain tick rate ---
        auto tick_processing_end_time = std::chrono::steady_clock::now();
        auto tick_processing_duration = tick_processing_end_time - tick_start_time;
        auto sleep_for = m_tickIntervalMs - tick_processing_duration;

        if (m_isSimulatingThread.load()) { // Only sleep/wait if still supposed to be running
            if (sleep_for > std::chrono::milliseconds(0)) {
                std::unique_lock<std::mutex> lock(m_shutdownThreadMutex);
                m_shutdownThreadCv.wait_for(lock, sleep_for, [this] { return !m_isSimulatingThread.load(); });
            }
 else if (tick_processing_duration > m_tickIntervalMs) {
  std::cerr << "GameServerEngine: WARNING - Tick took too long: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(tick_processing_duration).count()
            << "ms" << std::endl;
  // Optionally yield if tick is over budget to prevent 100% CPU spin if continuously over budget
  std::this_thread::yield();
}
}
        // If m_isSimulatingThread became false (either by wait_for condition or loop check), loop will terminate.
    }
    std::cout << "GameServerEngine: SimulationTick thread exiting gracefully." << std::endl;
}

} // namespace Server
} // namespace RiftForged