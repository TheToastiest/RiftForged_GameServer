// File: Gameplay/PlayerManager.cpp (Refactored)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#include "PlayerManager.h"
#include "../Gameplay/ActivePlayer.h" // Ensure ActivePlayer.h doesn't include NetworkEndpoint

// Logger is included via PlayerManager.h

namespace RiftForged {
    namespace GameLogic {

        PlayerManager::PlayerManager()
            : m_nextPlayerId(1), // Player IDs start from 1
            m_nextProjectileId(1) {
            RF_GAMELOGIC_INFO("PlayerManager: Initialized."); // Changed log scope
        }

        PlayerManager::~PlayerManager() {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            RF_GAMELOGIC_INFO("PlayerManager: Shutting down. Clearing {} active players.", m_playersById.size());
            m_playersById.clear(); // std::unique_ptr will handle deletion of ActivePlayer objects
        }

        ActivePlayer* PlayerManager::CreatePlayer(
            uint64_t playerId,
            const RiftForged::Networking::Shared::Vec3& startPos,
            const RiftForged::Networking::Shared::Quaternion& startOrientation,
            float cap_radius, float cap_half_height) {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);

            if (m_playersById.count(playerId)) {
                RF_GAMELOGIC_WARN("PlayerManager::CreatePlayer: Attempted to create player with existing ID {}.", playerId);
                return m_playersById.at(playerId).get(); // Return existing if duplicate ID somehow assigned
            }

            RF_GAMELOGIC_INFO("PlayerManager: Creating New Player. ID: {}", playerId);

            // ActivePlayer constructor no longer takes NetworkEndpoint
            auto newPlayer = std::make_unique<ActivePlayer>(
                playerId,
                startPos,
                startOrientation,
                cap_radius,
                cap_half_height
            );

            ActivePlayer* newPlayerPtr = newPlayer.get();
            m_playersById[playerId] = std::move(newPlayer);

            // Initial state loading or other game-logic specific setup for the new ActivePlayer
            // could happen here or be triggered by GameServerEngine after this call.
            // Example: newPlayerPtr->InitializeDefaultStats();
            // Example: newPlayerPtr->LoadPersistentData(m_someDatabaseService); // If PM has DB access

            return newPlayerPtr;
        }

        bool PlayerManager::RemovePlayer(uint64_t playerId) {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);

            auto it = m_playersById.find(playerId);
            if (it != m_playersById.end()) {
                RF_GAMELOGIC_INFO("PlayerManager: Removing Player ID {}.", playerId);
                // The ActivePlayer object will be destructed when its unique_ptr is erased.
                // GameServerEngine should have already handled:
                // 1. Notifying other game systems (GameplayEngine, Social, etc.)
                // 2. Coordinating with PhysicsEngine to remove the character controller
                // 3. Saving player's final state to DB
                m_playersById.erase(it);
                return true;
            }
            else {
                RF_GAMELOGIC_WARN("PlayerManager::RemovePlayer: Attempted to remove non-existent player with ID {}.", playerId);
                return false;
            }
        }

        ActivePlayer* PlayerManager::FindPlayerById(uint64_t playerId) const { // Made const
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            auto it = m_playersById.find(playerId);
            if (it != m_playersById.end()) {
                return it->second.get();
            }
            // RF_GAMELOGIC_TRACE("PlayerManager::FindPlayerById: Player with ID {} not found.", playerId); // More of a trace
            return nullptr;
        }

        std::vector<ActivePlayer*> PlayerManager::GetAllActivePlayerPointersForUpdate() {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            std::vector<ActivePlayer*> player_pointers;
            player_pointers.reserve(m_playersById.size());
            for (auto& pair : m_playersById) {
                player_pointers.push_back(pair.second.get());
            }
            return player_pointers;
        }

        // Const 
        // for read-only iteration
        std::vector<const ActivePlayer*> PlayerManager::GetAllActivePlayerPointersForUpdate() const {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            std::vector<const ActivePlayer*> player_pointers;
            player_pointers.reserve(m_playersById.size());
            for (const auto& pair : m_playersById) {
                player_pointers.push_back(pair.second.get());
            }
            return player_pointers;
        }


        uint64_t PlayerManager::GetNextAvailablePlayerID() {
            return m_nextPlayerId.fetch_add(1, std::memory_order_relaxed);
        }

        uint64_t PlayerManager::GetNextAvailableProjectileID() {
            return m_nextProjectileId.fetch_add(1, std::memory_order_relaxed);
        }

    } // namespace GameLogic
} // namespace RiftForged