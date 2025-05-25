// File: GameServer/GameServerEngine.h
// RiftForged Game Development Team
// Copyright (c) 2025-2028 RiftForged Game Development Team

#pragma once

#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex> // For m_shutdownThreadMutex
#include <any>   // For std::any in PendingGameplayEvent
#include <deque> // For m_outgoingPacketQueue

// Forward declarations or includes (adjust paths)
// FlatBuffer Declarations
#include "../FlatBuffers/V0.0.3/riftforged_common_types_generated.h" // For FlatBuffer types
#include "../FlatBuffers/V0.0.3/riftforged_c2s_udp_messages_generated.h"
#include "../FlatBuffers/V0.0.3/riftforged_s2c_udp_messages_generated.h"
// Networking Declarations
#include "../Gameplay/GameplayEngine.h" // For GameplayEngine (if needed later)
#include "../Gameplay/PlayerManager.h" // Needs access to players
#include "../Gameplay/ActivePlayer.h" // Needs access to ActivePlayer
#include "../NetworkEngine/UDPSocketAsync.h"   // Needs to send messages
#include "../NetworkEngine/GamePacketHeader.h" // For GamePacketHeader 

#include "../PhysicsEngine/PhysicsEngine.h"

#include "../Utils/Logger.h"

namespace RiftForged {
    namespace Server {

        class GameServerEngine {
        public:
            GameServerEngine(
                RiftForged::GameLogic::PlayerManager& playerManager,
                RiftForged::Gameplay::GameplayEngine& gameplayEngine, // Added GameplayEngine
                RiftForged::Networking::UDPSocketAsync& udpSocket,
                RiftForged::Physics::PhysicsEngine& physicsEngine,
                std::chrono::milliseconds tickInterval = std::chrono::milliseconds(8) // Default to 10Hz
            );
            ~GameServerEngine();

            GameServerEngine(const GameServerEngine&) = delete;
            GameServerEngine& operator=(const GameServerEngine&) = delete;

            void StartSimulationLoop();
            void StopSimulationLoop();
            bool IsSimulating() const;

        private:
            void SimulationTick();

            RiftForged::GameLogic::PlayerManager& m_playerManager;
            RiftForged::Gameplay::GameplayEngine& m_gameplayEngine; // Store reference
            RiftForged::Networking::UDPSocketAsync& m_udpSocket;
			RiftForged::Physics::PhysicsEngine& m_physicsEngine; // Added PhysicsEngine reference

            std::atomic<bool> m_isSimulatingThread;
            std::thread m_simulationThread;
            std::chrono::milliseconds m_tickIntervalMs;

            std::mutex m_shutdownThreadMutex;
            std::condition_variable m_shutdownThreadCv;
        };

    } // namespace Server
} // namespace RiftForged