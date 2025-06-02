// File: MessageDispatcher.h (Refactored)
// RiftForged Gaming Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include <cstdint>
#include <optional>

#include "GamePacketHeader.h" // Defines RiftForged::Networking::MessageType
#include "NetworkEndpoint.h"
#include "NetworkCommon.h"    // Defines RiftForged::Networking::S2C_Response
#include "../Gameplay/ActivePlayer.h" // For RiftForged::GameLogic::ActivePlayer

// Forward declarations for specific message handlers
namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                class MovementMessageHandler;
                class RiftStepMessageHandler;
                class AbilityMessageHandler;
                class PingMessageHandler;
                class TurnMessageHandler;
                class BasicAttackMessageHandler;
            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
    // ActivePlayer is now directly included.
} // namespace RiftForged


namespace RiftForged {
    namespace Networking {

        class MessageDispatcher {
        public:
            // Constructor remains the same, injecting all specific handlers
            MessageDispatcher(
                UDP::C2S::MovementMessageHandler& movementHandler,
                UDP::C2S::RiftStepMessageHandler& riftStepHandler,
                UDP::C2S::AbilityMessageHandler& abilityHandler,
                UDP::C2S::PingMessageHandler& pingHandler,
                UDP::C2S::TurnMessageHandler& turnHandler,
                UDP::C2S::BasicAttackMessageHandler& basicAttackHandler
            );

            // <<< MODIFIED DispatchC2SMessage signature >>>
            // Now takes MessageType messageId directly, and payload size as uint16_t for consistency.
            std::optional<RiftForged::Networking::S2C_Response> DispatchC2SMessage(
                RiftForged::Networking::MessageType messageId,          // <<< CHANGED from const GamePacketHeader& header
                const uint8_t* flatbuffer_payload_ptr,
                uint16_t flatbuffer_payload_size,                     // <<< CHANGED from int to uint16_t
                const NetworkEndpoint& sender_endpoint,
                RiftForged::GameLogic::ActivePlayer* player
            );

        private:
            // Member handlers remain the same
            UDP::C2S::MovementMessageHandler& m_movementHandler;
            UDP::C2S::RiftStepMessageHandler& m_riftStepHandler;
            UDP::C2S::AbilityMessageHandler& m_abilityHandler;
            UDP::C2S::PingMessageHandler& m_pingHandler;
            UDP::C2S::TurnMessageHandler& m_turnHandler;
            UDP::C2S::BasicAttackMessageHandler& m_basicAttackHandler;
        };

    } // namespace Networking
} // namespace RiftForged