// PlayerManager.h
// RiftForged Gaming
// Copyright 2023 RiftForged

#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <mutex>    // For std::mutex and std::lock_guard
#include <optional> // For FindPlayerBy... returning an optional pointer or wrapped result
#include <atomic>
#include <functional>


// Adjust paths as necessary for your project structure
#include "ActivePlayer.h" // For RiftForged::GameLogic::ActivePlayer
#include "../NetworkEngine/NetworkEndpoint.h" // For RiftForged::Networking::NetworkEndpoint
#include "../Utils/Logger.h" // For SpdLog macros

namespace RiftForged {
    namespace GameLogic {

        class PlayerManager {
        public:
            PlayerManager();
            ~PlayerManager();

            // Prevent copying and assignment
            PlayerManager(const PlayerManager&) = delete;
            PlayerManager& operator=(const PlayerManager&) = delete;

            // Gets an existing player for the endpoint or creates a new one.
            // Returns a raw pointer to the ActivePlayer object stored in the manager.
            // The PlayerManager owns the ActivePlayer objects.
            ActivePlayer* GetOrCreatePlayer(
                const RiftForged::Networking::NetworkEndpoint& endpoint,
                bool& out_was_newly_created
            );

            // Finds a player by their unique PlayerID.
            ActivePlayer* FindPlayerById(uint64_t playerId);

            // Finds a player by their network endpoint.
            ActivePlayer* FindPlayerByEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint);

            // Removes a player, e.g., on disconnect or timeout.
            void RemovePlayerByEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint);
            void RemovePlayerById(uint64_t playerId);

            // Gets a list of all currently active client network endpoints.
            // Useful for broadcasting messages via UDPSocketAsync.
            std::vector<RiftForged::Networking::NetworkEndpoint> GetAllActiveClientEndpoints() const;

            // Gets a list of pointers to all active player objects.
            // Useful for GameServerEngine's SimulationTick to update/read player states.
            // The caller must be careful with the lifetime of these pointers if players can be removed concurrently.
            std::vector<ActivePlayer*> GetAllActivePlayerPointersForUpdate();

            // ***** ADDED FOR PROJECTILE ID *****
            uint64_t GetNextAvailableProjectileID();
            // **********************************

        private:
            // Map from endpoint string key (IP:Port) to the ActivePlayer object.
            // Using the object directly in the map simplifies ownership.
            std::map<std::string, ActivePlayer> m_activePlayersByEndpointKey;

            // Map from PlayerID to a raw pointer to the ActivePlayer object in the above map.
            // This provides fast lookup by ID. Pointers are non-owning here.
            std::map<uint64_t, ActivePlayer*> m_playerPtrsById;

            // ***** ADDED FOR PROJECTILE ID *****
            std::atomic<uint64_t> m_nextProjectileId;
            // **********************************

            uint64_t m_nextPlayerId; // Simple counter for assigning unique PlayerIDs
            mutable std::mutex m_playerMapMutex; // Mutex to protect access to the player maps
        };

    } // namespace GameLogic
} // namespace RiftForged