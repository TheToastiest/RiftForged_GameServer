#pragma once
#include <cstdint> // For fixed-width integer types like uint32_t, uint16_t

namespace RiftForged {
    namespace Networking {

        const uint32_t CURRENT_PROTOCOL_ID_VERSION = 0x00000001; // Example for v0.0.1

        enum class MessageType : uint16_t {
            Unknown = 0, // Good to have a default/unknown

            // C2S Messages (matching C2S_UDP_Payload enum values, usually starting from 1 if NONE is 0)
            C2S_MovementInput = 1,
            C2S_TurnIntent = 2,
            C2S_RiftStepActivation = 3, // Updated from C2S_RiftStep
            C2S_UseAbility = 4,
            C2S_Ping = 5,

            // S2C Messages (matching S2C_UDP_Payload enum values, ensure unique range)
            S2C_EntityStateUpdate = 101,
            S2C_RiftStepInitiated = 102, // Updated from S2C_RiftStepExecuted
            S2C_ResourceUpdate = 103,
            S2C_CombatEvent = 104,
            S2C_Pong = 105,
            S2C_SystemBroadcast = 106, // Added
            // S2C_RiftStepCompleted = 107; // Consider for later
        };

        // Helper function to get string name of MessageType
        inline const char* EnumNameMessageType(MessageType e) {
            switch (e) {
            case MessageType::Unknown: return "Unknown";
            case MessageType::C2S_MovementInput: return "C2S_MovementInput";
            case MessageType::C2S_RiftStepActivation: return "C2S_RiftStepActivation";
            case MessageType::C2S_UseAbility: return "C2S_UseAbility";
            case MessageType::C2S_Ping: return "C2S_Ping";
            case MessageType::C2S_TurnIntent: return "C2S_TurnIntent";
            case MessageType::S2C_EntityStateUpdate: return "S2C_EntityStateUpdate";
            case MessageType::S2C_RiftStepInitiated: return "S2C_RiftStepInitiated";
            case MessageType::S2C_ResourceUpdate: return "S2C_ResourceUpdate";
            case MessageType::S2C_CombatEvent: return "S2C_CombatEvent";
            case MessageType::S2C_Pong: return "S2C_Pong";
            case MessageType::S2C_SystemBroadcast: return "S2C_SystemBroadcast";
            default: return "UnhandledMessageType";
            }
        }

#pragma pack(push, 1)
        struct GamePacketHeader {
            uint32_t protocolId;
            MessageType messageType;
            uint32_t sequenceNumber;
            uint32_t ackNumber;
            uint32_t ackBitfield;
            uint8_t flags;
            // uint8_t reserved; // Optional for 16-byte alignment

            GamePacketHeader(MessageType type = MessageType::Unknown, uint32_t seq = 0)
                : protocolId(CURRENT_PROTOCOL_ID_VERSION),
                messageType(type),
                sequenceNumber(seq),
                ackNumber(0),
                ackBitfield(0),
                flags(0) {
            }
        };
#pragma pack(pop)

        constexpr size_t GetGamePacketHeaderSize() {
            return sizeof(GamePacketHeader);
        }

    } // namespace Networking
} // namespace RiftForged