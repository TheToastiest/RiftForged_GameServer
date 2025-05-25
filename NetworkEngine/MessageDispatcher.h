
// File : MessageDispatcher.h
// RiftForged Gaming Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team

#pragma once

#include <cstdint>  // For uint8_t
#include <optional> // For std::optional

// Include necessary definitions for types used in the public interface.
// Assuming your flat file structure where these are findable by the compiler:
#include "GamePacketHeader.h" // Defines RiftForged::Networking::GamePacketHeader and MessageType
#include "NetworkEndpoint.h"  // Defines RiftForged::Networking::NetworkEndpoint
#include "NetworkCommon.h"    // Defines RiftForged::Networking::S2C_Response and includes <optional>
#include "../Gameplay/ActivePlayer.h"

// Forward declarations
namespace RiftForged {
    namespace Networking {
        namespace UDP {
            namespace C2S {
                class MovementMessageHandler;     //
                class RiftStepMessageHandler;     //
                class AbilityMessageHandler;    //
                class PingMessageHandler;         //
                class TurnMessageHandler;         //
                class BasicAttackMessageHandler;  //
            } // namespace C2S
        } // namespace UDP
    } // namespace Networking

    namespace GameLogic { // <<< ADDED forward declaration for ActivePlayer
        struct ActivePlayer;
    }
} // namespace RiftForged


namespace RiftForged {
    namespace Networking {

        class MessageDispatcher {
        public:
            MessageDispatcher(
                UDP::C2S::MovementMessageHandler& movementHandler,             //
                UDP::C2S::RiftStepMessageHandler& riftStepHandler,             //
                UDP::C2S::AbilityMessageHandler& abilityHandler,               //
                UDP::C2S::PingMessageHandler& pingHandler,                     //
                UDP::C2S::TurnMessageHandler& turnHandler,                     //
                UDP::C2S::BasicAttackMessageHandler& basicAttackHandler        //
            );

            // <<< MODIFIED DispatchC2SMessage signature: Added ActivePlayer* player >>>
            std::optional<RiftForged::Networking::S2C_Response> DispatchC2SMessage(
                const GamePacketHeader& header,                     //
                const uint8_t* flatbuffer_payload_ptr,              //
                int flatbuffer_payload_size,                        //
                const NetworkEndpoint& sender_endpoint,             //
                RiftForged::GameLogic::ActivePlayer* player         // <<< ADDED this parameter
            );

        private:
            UDP::C2S::MovementMessageHandler& m_movementHandler;            //
            UDP::C2S::RiftStepMessageHandler& m_riftStepHandler;            //
            UDP::C2S::AbilityMessageHandler& m_abilityHandler;              //
            UDP::C2S::PingMessageHandler& m_pingHandler;                    //
            UDP::C2S::TurnMessageHandler& m_turnHandler;                    //
            UDP::C2S::BasicAttackMessageHandler& m_basicAttackHandler;      //
        };

    } // namespace Networking
} // namespace RiftForged