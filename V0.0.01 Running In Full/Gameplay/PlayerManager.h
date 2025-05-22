#pragma once

#include "ActivePlayer.h" // Defines ActivePlayer struct
#include "../NetworkEngine/NetworkEndpoint.h" // For NetworkEndpoint
#include <string>
#include <map>
#include <mutex> // For thread-safe access to the player map

namespace RiftForged {
    namespace GameLogic {

        class PlayerManager {
        public:
            PlayerManager();

            // Gets an existing player by endpoint. If not found, creates a new one.
            // Returns nullptr if endpoint is invalid or creation fails.
            ActivePlayer* GetOrCreatePlayer(const RiftForged::Networking::NetworkEndpoint& endpoint);

            // Gets an existing player, returns nullptr if not found.
            ActivePlayer* FindPlayerByEndpoint(const RiftForged::Networking::NetworkEndpoint& endpoint);
            ActivePlayer* FindPlayerById(uint64_t playerId);

            // Removes a player (e.g., on disconnect)
            void RemovePlayer(const RiftForged::Networking::NetworkEndpoint& endpoint);

            // For broadcasting S2C messages
            std::vector<RiftForged::Networking::NetworkEndpoint> GetAllActiveClientEndpoints() const;

        private:
            // Stores the actual ActivePlayer objects, keyed by "ip:port" string
            std::map<std::string, ActivePlayer> m_activePlayersByEndpointKey;
            // Stores POINTERS to the ActivePlayer objects in the map above, keyed by playerId
            std::map<uint64_t, ActivePlayer*> m_playerPtrsById;
            mutable std::mutex m_playerMapMutex; // Mutable to allow locking in const methods
            uint64_t m_nextPlayerId;             // Simple ID generation for testing
        };

    } // namespace GameLogic
} // namespace RiftForged