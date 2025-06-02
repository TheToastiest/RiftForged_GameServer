// File: Gameplay/PlayerManager.h (Refactored)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include <mutex>
#include <memory> // For std::unique_ptr
#include <atomic>

#include "ActivePlayer.h" // For RiftForged::GameLogic::ActivePlayer
// #include "../NetworkEngine/NetworkEndpoint.h" // <<< REMOVED
#include "../Utils/Logger.h"

namespace RiftForged {
    namespace GameLogic {

        class PlayerManager {
        public:
            PlayerManager();
            ~PlayerManager();

            PlayerManager(const PlayerManager&) = delete;
            PlayerManager& operator=(const PlayerManager&) = delete;

            // Creates a new player instance. Called by GameServerEngine after a playerId is assigned.
            // GameServerEngine is responsible for linking this playerId to a NetworkEndpoint.
            ActivePlayer* CreatePlayer(
                uint64_t playerId,
                const RiftForged::Networking::Shared::Vec3& startPos,
                const RiftForged::Networking::Shared::Quaternion& startOrientation,
                float cap_radius = 0.5f, float cap_half_height = 0.9f
            );

            // Removes a player by their unique PlayerID.
            // Returns true if player was found and removed, false otherwise.
            // Pre-removal logic (saving state, notifying systems) should be handled by GameServerEngine
            // before calling this, or via callbacks/observers if PlayerManager needs to signal.
            bool RemovePlayer(uint64_t playerId);

            // Finds a player by their unique PlayerID.
            ActivePlayer* FindPlayerById(uint64_t playerId) const; // Made const

            // Gets a list of pointers to all active player objects for iteration.
            // The returned pointers are valid as long as the PlayerManager is not modified
            // (players added/removed) concurrently without proper synchronization by the caller.
            // Consider returning a copy of shared_ptrs or a list of IDs if stricter safety across threads is needed by GameServerEngine.
            std::vector<ActivePlayer*> GetAllActivePlayerPointersForUpdate(); // Non-const if internal iteration needs non-const access for some reason. Usually const.
            // Let's make it const for now, assuming read-only iteration for update gathering.
            std::vector<const ActivePlayer*> GetAllActivePlayerPointersForUpdate() const;


            uint64_t GetNextAvailablePlayerID();    // Utility for GameServerEngine to assign new IDs
            uint64_t GetNextAvailableProjectileID();

        private:
            std::map<uint64_t, std::unique_ptr<ActivePlayer>> m_playersById;

            std::atomic<uint64_t> m_nextPlayerId;
            std::atomic<uint64_t> m_nextProjectileId;
            mutable std::mutex m_playerMapMutex; // Protects m_playersById map
        };

    } // namespace GameLogic
} // namespace RiftForged