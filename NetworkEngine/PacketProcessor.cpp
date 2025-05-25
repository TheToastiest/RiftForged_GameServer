// File: PacketProcessor.cpp
// RiftForged Gaming
// Copyright (c) 2023-2025 RiftForged Game Development Team


#include "PacketProcessor.h"        // Includes NetworkCommon.h, GamePacketHeader.h, forward declares MessageDispatcher & GameplayEngine
#include "MessageDispatcher.h"      // For m_messageDispatcher.DispatchC2SMessage
#include "../Gameplay/GameplayEngine.h" // For m_gameplayEngine.InitializePlayerInWorld and accessing PlayerManager
#include "../Gameplay/PlayerManager.h"  // For m_gameplayEngine.GetPlayerManager().GetOrCreatePlayer()
#include "../Gameplay/ActivePlayer.h"    // For ActivePlayer* type
#include "../Utils/Logger.h"        // For RF_NETWORK_ERROR, RF_NETWORK_INFO, etc. (Ensure path is correct)

#include <cstring> // For std::memcpy
// Note: <iostream> was previously used for std::cerr/cout, assuming Logger.h now provides RF_... macros for all logging.

namespace RiftForged {
    namespace Networking {

        // Constructor now takes GameplayEngine
        PacketProcessor::PacketProcessor(MessageDispatcher& dispatcher, RiftForged::Gameplay::GameplayEngine& gameplayEngine)
            : m_messageDispatcher(dispatcher),
            m_gameplayEngine(gameplayEngine) { // Initialize m_gameplayEngine
            RF_NETWORK_INFO("PacketProcessor: Initialized with MessageDispatcher and GameplayEngine.");
        }

        std::optional<S2C_Response> PacketProcessor::ProcessIncomingRawPacket(
            const char* raw_buffer,
            int raw_length,
            const NetworkEndpoint& sender_endpoint) {

            // 1. Validate basic packet size
            if (raw_length < static_cast<int>(GetGamePacketHeaderSize())) { //
                RF_NETWORK_ERROR("PacketProcessor: Packet from {} too small ({} bytes) for GamePacketHeader.", sender_endpoint.ToString(), raw_length);
                return std::nullopt;
            }

            // 2. Deserialize our custom GamePacketHeader
            GamePacketHeader header; //
            std::memcpy(&header, raw_buffer, GetGamePacketHeaderSize());

            // 3. Validate Protocol ID
            if (header.protocolId != CURRENT_PROTOCOL_ID_VERSION) { //
                RF_NETWORK_WARN("PacketProcessor: Mismatched protocol ID from {}. Expected: {}, Got: {}",
                    sender_endpoint.ToString(), CURRENT_PROTOCOL_ID_VERSION, header.protocolId);
                return std::nullopt;
            }

            // --- NEW: Player Creation and World Initialization Logic ---
            bool player_was_newly_created = false;
            // Assuming GameplayEngine has a public member m_playerManager or a GetPlayerManager() method.
            // Let's use a conceptual GetPlayerManager() for better encapsulation.
            RiftForged::GameLogic::ActivePlayer* player =
                m_gameplayEngine.GetPlayerManager().GetOrCreatePlayer(sender_endpoint, player_was_newly_created);
            // GetOrCreatePlayer signature needs to be: ActivePlayer* GetOrCreatePlayer(const NetworkEndpoint&, bool&);

            if (!player) {
                RF_NETWORK_ERROR("PacketProcessor: Failed to get or create player for endpoint: {}. Dropping packet for MessageType: {}",
                    sender_endpoint.ToString(), static_cast<int>(header.messageType));
                return std::nullopt;
            }

            if (player && player_was_newly_created) {
                RF_NETWORK_INFO("PacketProcessor: New player {} detected from endpoint {}. Initializing in world.", player->playerId, sender_endpoint.ToString());

                // Define spawn parameters (these should eventually come from a spawn manager or world settings)
                RiftForged::Networking::Shared::Vec3 spawn_position(0.0f, 0.0f, 1.0f); // Example spawn point
                RiftForged::Networking::Shared::Quaternion spawn_orientation(0.0f, 0.0f, 0.0f, 1.0f); // Identity orientation

                // Call GameplayEngine to create the PhysX controller and set initial logical/physical state
                m_gameplayEngine.InitializePlayerInWorld(player, spawn_position, spawn_orientation); //
                // InitializePlayerInWorld will internally call m_physicsEngine.CreateCharacterController and SetCharacterControllerOrientation
            }
            // --- END NEW Player Creation and World Initialization Logic ---

            // 4. TODO: Process Reliability Information from the header (your existing TODO)
            // ...

            // 5. Determine the start and size of the FlatBuffer payload
            const uint8_t* flatbuffer_payload_ptr = reinterpret_cast<const uint8_t*>(raw_buffer + GetGamePacketHeaderSize());
            int flatbuffer_payload_size = raw_length - static_cast<int>(GetGamePacketHeaderSize());

            if (flatbuffer_payload_size < 0) {
                RF_NETWORK_ERROR("PacketProcessor: Negative FlatBuffer payload size calculated from {}. Header size: {}, Total length: {}. MessageType: {}",
                    sender_endpoint.ToString(), GetGamePacketHeaderSize(), raw_length, static_cast<int>(header.messageType));
                return std::nullopt;
            }

            // 6. Dispatch to the MessageDispatcher
            // Pass the ActivePlayer* pointer to the dispatcher, which can then pass it to the handlers.
            // This assumes DispatchC2SMessage is updated to accept ActivePlayer*.
            return m_messageDispatcher.DispatchC2SMessage(header, flatbuffer_payload_ptr, flatbuffer_payload_size, sender_endpoint, player);
        }

    } // namespace Networking
} // namespace RiftForged