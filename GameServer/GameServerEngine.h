// File: GameServer/GameServerEngine.h (Updated for FlatBuffer Command Types)
// RiftForged Game Development Team
// Copyright (c) 2025-2028 RiftForged Game Development Team

#pragma once

#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <any>      // For storing various command types
#include <deque>
#include <map>
#include <string>
#include <optional> // For GetEndpointForPlayerId

// Core Game Logic/Engine Includes
#include "../Gameplay/GameplayEngine.h"
#include "../Gameplay/PlayerManager.h"
#include "../Gameplay/ActivePlayer.h"     // Included via PlayerManager or GameplayEngine
#include "../PhysicsEngine/PhysicsEngine.h"

// Networking
#include "../NetworkEngine/UDPPacketHandler.h"
#include "../NetworkEngine/NetworkEndpoint.h"
#include "../NetworkEngine/GamePacketHeader.h" // For MessageType enum for S2C

// FlatBuffer Declarations (C2S for command types, S2C for sending, Common for shared types)
#include "../FlatBuffers/V0.0.4/riftforged_common_types_generated.h"
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S message types
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C message construction

// Utilities
#include "../Utils/Logger.h"
#include "../Utils/MathUtil.h"

// Aliases
namespace RF_C2S = RiftForged::Networking::UDP::C2S;
namespace RF_S2C = RiftForged::Networking::UDP::S2C;
namespace RF_Shared = RiftForged::Networking::Shared;
namespace RF_Net = RiftForged::Networking;
namespace RF_GameLogic = RiftForged::GameLogic;


namespace RiftForged {
    namespace Server {

        // Forward declare specific C2S message T-types (Object API types) if they are stored directly in std::any
        // This helps with clarity if you're unpacking to these before queueing.

        class GameServerEngine {
        public:
            GameServerEngine(
                RiftForged::GameLogic::PlayerManager& playerManager,
                RiftForged::Gameplay::GameplayEngine& gameplayEngine,
                //RiftForged::Networking::UDPPacketHandler& packetHandler,
                RiftForged::Physics::PhysicsEngine& physicsEngine,
                std::chrono::milliseconds tickInterval = std::chrono::milliseconds(10)
            );
            ~GameServerEngine();

            GameServerEngine(const GameServerEngine&) = delete;
            GameServerEngine& operator=(const GameServerEngine&) = delete;

            void StartSimulationLoop();
            void StopSimulationLoop();

            // --- Session Management ---
            uint64_t OnClientAuthenticatedAndJoining(const RiftForged::Networking::NetworkEndpoint& newEndpoint,
                const std::string& characterIdToLoad = "");
            void OnClientDisconnected(const RiftForged::Networking::NetworkEndpoint& endpoint);
            uint64_t GetPlayerIdForEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint) const;
            std::optional<RiftForged::Networking::NetworkEndpoint> GetEndpointForPlayerId(uint64_t playerId) const;
            void QueueClientJoinRequest(const Networking::NetworkEndpoint& endpoint, const std::string& characterIdToLoad);
            //void QueueClientDisconnectRequest(const Networking::NetworkEndpoint& endpoint);
            //void ProcessJoinRequests();
            //void ProcessDisconnectRequests(const Networking::NetworkEndpoint& endpoint);
            // --- Incoming Command Submission ---
            // MessageDispatcher calls this after parsing a packet and resolving PlayerId.
            // The std::any should ideally hold an unpacked T-object (e.g., C2S_MovementInputMsgT)
            // or a structure that can be easily type-checked and cast.
            void SubmitPlayerCommand(uint64_t playerId, std::any commandPayload);

            std::vector<RiftForged::Networking::NetworkEndpoint> GetAllActiveSessionEndpoints() const;

            RiftForged::GameLogic::PlayerManager& GetPlayerManager();
            const RiftForged::GameLogic::PlayerManager& GetPlayerManager() const; // Optional const version

            void SetPacketHandler(RiftForged::Networking::UDPPacketHandler* handler);


        private:
            void SimulationTick();
            void ProcessPlayerCommands(); // Processes commands from m_incomingCommandQueue

            struct ClientJoinRequest {
                Networking::NetworkEndpoint endpoint;
                std::string characterIdToLoad;
                // You can add other data here if needed, like auth tokens, etc.
            };

            // --- Core Components ---
            RiftForged::GameLogic::PlayerManager& m_playerManager;
            RiftForged::Gameplay::GameplayEngine& m_gameplayEngine;
            RiftForged::Networking::UDPPacketHandler* m_packetHandlerPtr;
            RiftForged::Physics::PhysicsEngine& m_physicsEngine;

			// Join / Disconnect Requests & Queues
            std::deque<ClientJoinRequest> m_joinRequestQueue;
            std::mutex m_joinRequestQueueMutex;
            void ProcessJoinRequests();
            void ProcessDisconnectRequests();
            void SendJoinFailedResponse(RF_Net::UDPPacketHandler* packetHandler,
                const Networking::NetworkEndpoint& recipient,
                const std::string& reason_message,
                int16_t reason_code = 0);
            std::deque<Networking::NetworkEndpoint> m_disconnectRequestQueue;
            std::mutex m_disconnectRequestQueueMutex;
            // --- Simulation Loop Control ---
            std::atomic<bool> m_isSimulatingThread;
            std::thread m_simulationThread;
            std::chrono::milliseconds m_tickIntervalMs;
            bool m_timerResolutionWasSet;
            std::mutex m_shutdownThreadMutex;
            std::condition_variable m_shutdownThreadCv;
            // --- Session Mapping ---
            std::map<std::string, uint64_t> m_endpointKeyToPlayerIdMap;
            std::map<uint64_t, RiftForged::Networking::NetworkEndpoint> m_playerIdToEndpointMap;
            mutable std::mutex m_sessionMapsMutex;

            // --- Command Queue ---
            struct QueuedPlayerCommand {
                uint64_t playerId;
                std::any commandPayload; // Holds specific C2S *T type objects (e.g., C2S_MovementInputMsgT)
            };
            std::deque<QueuedPlayerCommand> m_incomingCommandQueue;
            std::mutex m_commandQueueMutex;
        };

    } // namespace Server
} // namespace RiftForged