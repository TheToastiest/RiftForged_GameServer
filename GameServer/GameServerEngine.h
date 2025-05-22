#pragma once

#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable> // For controlled shutdown of its tick loop

// Forward declarations or includes (adjust paths)
// FlatBuffer Declarations
#include "../FlatBuffers/V0.0.1/riftforged_common_types_generated.h" // For FlatBuffer types
#include "../FlatBuffers/V0.0.1/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.1/riftforged_s2c_udp_messages_generated.h"
// Networking Declarations
#include "../Gameplay/PlayerManager.h" // Needs access to players
#include "../NetworkEngine/UDPSocketAsync.h"   // Needs to send messages
#include "../NetworkEngine/GamePacketHeader.h" // For GamePacketHeader 


namespace RiftForged {
    namespace Server {

        class GameServerEngine {
        public:
            GameServerEngine(
                RiftForged::GameLogic::PlayerManager& playerManager,
                RiftForged::Networking::UDPSocketAsync& udpSocket
                // You might add GameplayEngine& gameplayEngine later
            );
            ~GameServerEngine();

            GameServerEngine(const GameServerEngine&) = delete;
            GameServerEngine& operator=(const GameServerEngine&) = delete;

            void StartSimulationLoop();
            void StopSimulationLoop();
            bool IsSimulating() const;

        private:
            void SimulationTick(); // The main loop function for this engine

            RiftForged::GameLogic::PlayerManager& m_playerManager;
            RiftForged::Networking::UDPSocketAsync& m_udpSocket;
            // GameplayEngine& m_gameplayEngine; // For later

            std::atomic<bool> m_isSimulatingThread; // Flag specifically for the simulation thread's loop
            std::thread m_simulationThread;
            std::chrono::milliseconds m_tickIntervalMs;

            std::mutex m_shutdownThreadMutex; // Mutex for the condition variable used in Tick
            std::condition_variable m_shutdownThreadCv; // For waking up the tick thread on stop
        };

    } // namespace Server
} // namespace RiftForged