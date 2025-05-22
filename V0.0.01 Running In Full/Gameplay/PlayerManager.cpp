#include "PlayerManager.h"
#include <iostream> // For logging
#include <mutex>

namespace RiftForged {
    namespace GameLogic {

        PlayerManager::PlayerManager() : m_nextPlayerId(1) { // Start player IDs from 1
            std::cout << "PlayerManager: Initialized." << std::endl;
        }

        ActivePlayer* PlayerManager::GetOrCreatePlayer(const RiftForged::Networking::NetworkEndpoint& endpoint) {
            // 1. Validate the incoming endpoint before creating a key or locking
            if (endpoint.ipAddress.empty() || endpoint.port == 0) {
                std::cerr << "PlayerManager CRITICAL: GetOrCreatePlayer called with INVALID endpoint. IP: ["
                    << endpoint.ipAddress << "], Port: " << endpoint.port << "]. Investigate caller!" << std::endl;
                return nullptr;
            }

            std::string endpointKey = endpoint.ipAddress + ":" + std::to_string(endpoint.port);

            // Lock for thread safety when accessing maps
            std::lock_guard<std::mutex> lock(m_playerMapMutex);

            std::cout << "PlayerManager: GetOrCreatePlayer - Attempting for Key: [" << endpointKey << "]" << std::endl;

            auto it = m_activePlayersByEndpointKey.find(endpointKey);

            if (it == m_activePlayersByEndpointKey.end()) {
                // --- Player NOT found for this endpoint, CREATE a new one ---
                uint64_t newPlayerId = m_nextPlayerId++;

                // Construct the ActivePlayer object using its constructor.
                // It takes (playerId, NetworkEndpoint, startPosition, startOrientation).
                ActivePlayer newPlayerObject(
                    newPlayerId,
                    endpoint, // Pass the full, validated NetworkEndpoint object
                    RiftForged::Networking::Shared::Vec3{ 0.0f, 0.0f, 0.0f }, // Default start pos
                    RiftForged::Networking::Shared::Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } // Default orientation
                );
                // You can set other initial defaults here if needed, e.g.:
                // newPlayerObject.activeRiftStepModifierId = 1; // Example default modifier

                // Emplace the newPlayerObject into the primary map.
                auto emplace_result = m_activePlayersByEndpointKey.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(endpointKey),          // Arguments for map's key (std::string) construction
                    std::forward_as_tuple(                       // Arguments for map's value (ActivePlayer) construction
                        newPlayerId,                             // uint64_t pId
                        endpoint,                                // const NetworkEndpoint& ep
                        RiftForged::Networking::Shared::Vec3{ 0.0f, 0.0f, 0.0f }, // const Vec3& startPos
                        RiftForged::Networking::Shared::Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f } // const Quaternion& startOrientation
                    )
                );

                if (!emplace_result.second) {
                    std::cerr << "PlayerManager: CRITICAL - Failed to emplace new player for key [" << endpointKey
                        << "]. This indicates a logic error or unexpected map state." << std::endl;
                    // It's possible another thread inserted it just after our find() but before emplace(),
                    // though the lock_guard should prevent this specific race on m_activePlayersByEndpointKey.
                    // If this happens, try to find it again to be safe.
                    auto race_it = m_activePlayersByEndpointKey.find(endpointKey);
                    if (race_it != m_activePlayersByEndpointKey.end()) {
                        return &(race_it->second);
                    }
                    return nullptr;
                }

                // Get a pointer to the newly emplaced ActivePlayer object (stored by value in the map)
                ActivePlayer* newPlayerPtr = &(emplace_result.first->second);

                // Store the pointer in the secondary ID-based lookup map.
                m_playerPtrsById[newPlayerId] = newPlayerPtr;

                std::cout << "PlayerManager: CREATED New Player. ID: " << newPlayerId
                    << ", EndpointKey: [" << endpointKey << "]" << std::endl;
                return newPlayerPtr;
            }
            else {
                // --- Player FOUND for this endpoint, return existing ---
                std::cout << "PlayerManager: FOUND Existing Player. ID: " << it->second.playerId
                    << ", EndpointKey: [" << endpointKey << "]" << std::endl;
                return &(it->second); // Return pointer to the existing ActivePlayer object
            }
        }

        ActivePlayer* PlayerManager::FindPlayerByEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint) {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            std::string endpointKey = endpoint.ipAddress + ":" + std::to_string(endpoint.port);
            auto it = m_activePlayersByEndpointKey.find(endpointKey);
            return (it != m_activePlayersByEndpointKey.end()) ? &(it->second) : nullptr;
        }

        ActivePlayer* PlayerManager::FindPlayerById(uint64_t playerId) {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            auto it = m_playerPtrsById.find(playerId);
            return (it != m_playerPtrsById.end()) ? it->second : nullptr;
        }

        void PlayerManager::RemovePlayer(const RiftForged::Networking::NetworkEndpoint& endpoint) {
            std::lock_guard<std::mutex> lock(m_playerMapMutex);
            std::string endpointKey = endpoint.ipAddress + ":" + std::to_string(endpoint.port);
            auto it = m_activePlayersByEndpointKey.find(endpointKey);
            if (it != m_activePlayersByEndpointKey.end()) {
                uint64_t playerIdToRemove = it->second.playerId;
                m_playerPtrsById.erase(playerIdToRemove);
                m_activePlayersByEndpointKey.erase(it); // This invalidates pointers from m_playerPtrsById if they weren't removed first!
                // Order is important: remove from m_playerPtrsById first or ensure no dangling pointers.
                // Actually, since m_playerPtrsById stores pointers to elements in m_activePlayersByEndpointKey,
                // erasing from m_activePlayersByEndpointKey invalidates those pointers.
                // The current order is correct for map value storage.
                std::cout << "PlayerManager: Removed player " << playerIdToRemove << " (" << endpointKey << ")" << std::endl;
            }
            else {
                std::cout << "PlayerManager: Attempted to remove non-existent player for " << endpointKey << std::endl;
            }
        }

        std::vector<RiftForged::Networking::NetworkEndpoint> PlayerManager::GetAllActiveClientEndpoints() const {
            std::lock_guard<std::mutex> lock(m_playerMapMutex); // m_playerMapMutex is mutable
            std::vector<RiftForged::Networking::NetworkEndpoint> endpoints;
            endpoints.reserve(m_activePlayersByEndpointKey.size());
            for (const auto& pair : m_activePlayersByEndpointKey) {
                // Assumes ActivePlayer struct has a 'networkEndpoint' member of the correct type
                endpoints.push_back(pair.second.networkEndpoint);
            }
            return endpoints;
        }

    } // namespace GameLogic
} // namespace RiftForged