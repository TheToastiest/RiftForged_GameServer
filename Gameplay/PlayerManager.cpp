// PlayerManager.cpp
// RiftForged Gaming
// Copyright 2025-2028 RiftForged

#include "PlayerManager.h"
// Logger is included via PlayerManager.h -> Utilities/Logger.h

namespace RiftForged {
    namespace GameLogic {

        PlayerManager::PlayerManager() : m_nextPlayerId(1) { // Player IDs start from 1
            RF_PLAYERMGR_INFO("PlayerManager: Initialized.");
        }

        PlayerManager::~PlayerManager() {
            RF_PLAYERMGR_INFO("PlayerManager: Shutting down. Clearing {} active players.", m_activePlayersByEndpointKey.size());
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            m_playerPtrsById.clear();
            m_activePlayersByEndpointKey.clear();
            // ActivePlayer objects are destroyed when m_activePlayersByEndpointKey is cleared.
        }

        ActivePlayer* PlayerManager::GetOrCreatePlayer(
            const RiftForged::Networking::NetworkEndpoint& endpoint,
            bool& out_was_newly_created
        )
        { //
            out_was_newly_created = false; // Initialize to false

            if (endpoint.ipAddress.empty() || endpoint.port == 0) { //
                RF_PLAYERMGR_CRITICAL("PlayerManager::GetOrCreatePlayer: Attempted with INVALID endpoint: {}", endpoint.ToString()); //
                return nullptr;
            }


            std::string endpointKey = endpoint.ToString();
            std::lock_guard<std::mutex> lock(m_playerMapMutex); // Protect map access

            auto it = m_activePlayersByEndpointKey.find(endpointKey);
            if (it == m_activePlayersByEndpointKey.end()) {
                // Player not found, create a new one
                out_was_newly_created = true; // <<< SET THE FLAG HERE
                uint64_t newPlayerId = m_nextPlayerId++;
                RF_PLAYERMGR_INFO("PlayerManager: CREATING New Player. ID: {}, EndpointKey: [{}]", newPlayerId, endpointKey);

                // Emplace directly into the map (C++17, or use try_emplace)
                // The ActivePlayer constructor (from response #135) takes (pId, endpoint, startPos, startOrientation)
                auto emplace_result = m_activePlayersByEndpointKey.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(endpointKey), // Arguments for map's key (std::string)
                    std::forward_as_tuple(              // Arguments for map's value (ActivePlayer)
                        newPlayerId,
                        endpoint,                       // Pass the NetworkEndpoint object
                        RiftForged::Networking::Shared::Vec3{ 0.0f, 0.0f, 0.0f }, // Default startPos
                        RiftForged::Networking::Shared::Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } // Default startOrientation (identity)
                    )
                );

                if (!emplace_result.second) {
                    RF_PLAYERMGR_CRITICAL("PlayerManager: Failed to emplace new player for endpoint key [{}]. This should not happen if find failed.", endpointKey);
                    // This case (find fails but emplace fails) is highly unlikely for std::map unless out of memory.
                    return nullptr;
                }

                ActivePlayer* newPlayerPtr = &(emplace_result.first->second); // Get pointer to the emplaced ActivePlayer object
                m_playerPtrsById[newPlayerId] = newPlayerPtr;

                // TODO: When a player is created, their initial state might need to be loaded from DragonflyDB/SQL
                // For now, they get default ActivePlayer constructor values.
                // Example: newPlayerPtr->LoadPersistentState(newPlayerId, m_databaseService);

                return newPlayerPtr;
            }
            else {
                // Player found
                RF_PLAYERMGR_TRACE("PlayerManager: FOUND Existing Player. ID: {}, EndpointKey: [{}]", it->second.playerId, endpointKey);
                return &(it->second); // Return pointer to existing ActivePlayer object
            }
        }

        ActivePlayer* PlayerManager::FindPlayerById(uint64_t playerId) {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            auto it = m_playerPtrsById.find(playerId);
            if (it != m_playerPtrsById.end()) {
                return it->second; // Returns the stored raw pointer
            }
            RF_PLAYERMGR_WARN("PlayerManager::FindPlayerById: Player with ID {} not found.", playerId);
            return nullptr;
        }

        ActivePlayer* PlayerManager::FindPlayerByEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint) {
            std::string endpointKey = endpoint.ToString();
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            auto it = m_activePlayersByEndpointKey.find(endpointKey);
            if (it != m_activePlayersByEndpointKey.end()) {
                return &(it->second); // Return pointer to ActivePlayer object in map
            }
            // RF_PLAYERMGR_TRACE("PlayerManager::FindPlayerByEndpoint: Player with endpoint [{}] not found.", endpointKey);
            return nullptr;
        }

        void PlayerManager::RemovePlayerByEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint) {
            std::string endpointKey = endpoint.ToString();
            std::lock_guard<std::mutex> lock(m_playerMapMutex); // Protect map access

            auto it = m_activePlayersByEndpointKey.find(endpointKey);
            if (it != m_activePlayersByEndpointKey.end()) {
                uint64_t playerIdToRemove = it->second.playerId;
                RF_PLAYERMGR_INFO("PlayerManager: Removing Player ID {} from endpoint [{}].", playerIdToRemove, endpointKey);

                m_playerPtrsById.erase(playerIdToRemove);
                m_activePlayersByEndpointKey.erase(it); // This destructs the ActivePlayer object
                // TODO: Save player state to DB before removal if needed.
            }
            else {
                RF_PLAYERMGR_WARN("PlayerManager::RemovePlayerByEndpoint: Attempted to remove non-existent player for endpoint [{}].", endpointKey);
            }
        }

        void PlayerManager::RemovePlayerById(uint64_t playerId) {
            std::lock_guard<std::mutex> lock(m_playerMapMutex); // Protect map access

            auto it_by_id = m_playerPtrsById.find(playerId);
            if (it_by_id != m_playerPtrsById.end()) {
                // Need to get the endpoint key to remove from the other map
                // ActivePlayer* playerToRemove = it_by_id->second; // This points to an object in m_activePlayersByEndpointKey
                // std::string endpointKeyToRemove = playerToRemove->networkEndpoint.ToString(); // Assuming ActivePlayer stores its endpoint

                // Safer: Iterate to find the key if ActivePlayer doesn't store its key redundantly
                // Or, ActivePlayer MUST store its NetworkEndpoint so we can reconstruct the key
                // Assuming ActivePlayer.networkEndpoint is reliable:
                std::string endpointKeyToRemove = it_by_id->second->networkEndpoint.ToString();

                RF_PLAYERMGR_INFO("PlayerManager: Removing Player ID {} (endpoint key [{}]).", playerId, endpointKeyToRemove);

                m_playerPtrsById.erase(it_by_id);
                m_activePlayersByEndpointKey.erase(endpointKeyToRemove); // This destructs the ActivePlayer object
                // TODO: Save player state to DB before removal.
            }
            else {
                RF_PLAYERMGR_WARN("PlayerManager::RemovePlayerById: Attempted to remove non-existent player with ID {}.", playerId);
            }
        }

        std::vector<RiftForged::Networking::NetworkEndpoint> PlayerManager::GetAllActiveClientEndpoints() const {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            std::vector<RiftForged::Networking::NetworkEndpoint> endpoints;
            endpoints.reserve(m_activePlayersByEndpointKey.size());
            for (const auto& pair : m_activePlayersByEndpointKey) {
                // pair.second is the ActivePlayer object, which contains its networkEndpoint
                endpoints.push_back(pair.second.networkEndpoint);
            }
            return endpoints;
        }

        std::vector<ActivePlayer*> PlayerManager::GetAllActivePlayerPointersForUpdate() {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            std::vector<ActivePlayer*> player_pointers;
            player_pointers.reserve(m_activePlayersByEndpointKey.size());
            for (auto& pair : m_activePlayersByEndpointKey) { // Iterate by reference to get address
                player_pointers.push_back(&pair.second); // Get pointer to the ActivePlayer object in the map
            }
            return player_pointers;
        }

        // ***** ADDED DEFINITION FOR PROJECTILE ID *****
        uint64_t PlayerManager::GetNextAvailableProjectileID() {
            return m_nextProjectileId.fetch_add(1, std::memory_order_relaxed);
        }
        // *********************************************

    } // namespace GameLogic
} // namespace RiftForged