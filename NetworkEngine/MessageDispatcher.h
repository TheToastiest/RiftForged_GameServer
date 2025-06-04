// File: MessageDispatcher.h
// RiftForged Gaming Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include <cstdint>
#include <optional>
#include <functional> // For std::function if handlers need a common interface

// We no longer directly include GamePacketHeader.h for its MessageType enum.
// The reliability layer's header and flags are handled by the UDPReliabilityProtocol.
// MessageDispatcher's job is purely to dispatch application payloads.
// #include "GamePacketHeader.h" // REMOVED: No longer needed for message type

#include "NetworkEndpoint.h"
#include "NetworkCommon.h"          // Defines RiftForged::Networking::S2C_Response (now uses FB S2C payload type)
#include "../Gameplay/ActivePlayer.h" // For RiftForged::GameLogic::ActivePlayer
#include "../Utils/ThreadPool.h"    // Adjust path if necessary

// Include the FlatBuffers generated C2S messages header.
// This brings in RiftForged::Networking::UDP::C2S::C2S_UDP_Payload
// and the message structs/tables like C2S_MovementInputMsg.
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h"


// Forward declarations for specific message handlers
namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                // These handlers will process the FlatBuffer message structs directly.
                // Their `Process` methods will take a const pointer to the specific FlatBuffer message type.
                class MovementMessageHandler;
                class RiftStepMessageHandler;
                class AbilityMessageHandler;
                class PingMessageHandler;
                class TurnMessageHandler;
                class BasicAttackMessageHandler;
                class JoinRequestMessageHandler;
            } // namespace C2S
        } // namespace UDP
    } // namespace Networking
} // namespace RiftForged


namespace RiftForged {
    namespace Networking {

        class MessageDispatcher {
        public:
            // Constructor: Injects all specific handlers and the thread pool.
            MessageDispatcher(
                UDP::C2S::MovementMessageHandler& movementHandler,
                UDP::C2S::RiftStepMessageHandler& riftStepHandler,
                UDP::C2S::AbilityMessageHandler& abilityHandler,
                UDP::C2S::PingMessageHandler& pingHandler,
                UDP::C2S::TurnMessageHandler& turnHandler,
                UDP::C2S::BasicAttackMessageHandler& basicAttackHandler,
                UDP::C2S::JoinRequestMessageHandler& joinRequestHandler,
                RiftForged::Utils::Threading::TaskThreadPool* taskPool
            );

            // Dispatches a C2S message. It expects a raw FlatBuffer payload.
            // The dispatcher's job is to verify, get the root, and use the
            // FlatBuffer's internal payload_type for dispatching.
            std::optional<RiftForged::Networking::S2C_Response> DispatchC2SMessage(
                const uint8_t* flatbuffer_payload_ptr,
                uint16_t flatbuffer_payload_size,
                const NetworkEndpoint& sender_endpoint,
                RiftForged::GameLogic::ActivePlayer* player
            );

        private:
            // Member handlers, stored as references (dependency injection).
            UDP::C2S::MovementMessageHandler& m_movementHandler;
            UDP::C2S::RiftStepMessageHandler& m_riftStepHandler;
            UDP::C2S::AbilityMessageHandler& m_abilityHandler;
            UDP::C2S::PingMessageHandler& m_pingHandler;
            UDP::C2S::TurnMessageHandler& m_turnHandler;
            UDP::C2S::BasicAttackMessageHandler& m_basicAttackHandler;
            UDP::C2S::JoinRequestMessageHandler& m_joinRequestHandler;
            RiftForged::Utils::Threading::TaskThreadPool* m_taskThreadPool;
        };

    } // namespace Networking
} // namespace RiftForged