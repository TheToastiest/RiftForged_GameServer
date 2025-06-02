#pragma once
#include <cstdint> // For fixed-width integer types like uint32_t, uint16_t

namespace RiftForged {
    namespace Networking {

        const uint32_t CURRENT_PROTOCOL_ID_VERSION = 0x00000004; // Version 0.0.4 WITH PHYSX AND LOGIN PROCEDURES

        enum class MessageType : uint16_t {
            Unknown = 0, // Good to have a default/unknown

            C2S_MovementInput = 1,
            C2S_TurnIntent = 2,
            C2S_RiftStepActivation = 3,
            C2S_BasicAttackIntent = 4,
            C2S_UseAbility = 5,
            C2S_Ping = 6,
            C2S_JoinRequest = 7,  // <<< ADDED: For new clients requesting to join the shard
            // C2S_ClientReady        = 8,  // Example: Client confirms it has loaded initial state after JoinSuccess
            // C2S_AdminCommand       = 9,  // Example for later

            // --- Server-to-Client (S2C) Messages ---
            // These are distinct from C2S values. Starting S2C from 101 is a good way to differentiate.
            // These should correspond conceptually to your S2C_UDP_Payload FlatBuffer enum values.
            S2C_ReservedStart = 100, // Start of S2C block, not a message itself
            S2C_EntityStateUpdate = 101,
            S2C_RiftStepInitiated = 102,
            S2C_ResourceUpdate = 103,
            S2C_CombatEvent = 104,
            S2C_Pong = 105,
            S2C_SystemBroadcast = 106,
            S2C_SpawnProjectile = 107,
            S2C_JoinSuccess = 108, // <<< ADDED: To confirm successful join to the client
            S2C_JoinFailed = 109, // <<< ADDED: To inform client if join failed (e.g., server full, auth issue)
            S2C_PlayerDisconnected = 110, // <<< ADDED: To inform other clients a player left
            S2C_EntityDestroyed = 111, // Example for when projectiles or other entities are removed

            // Keep this updated if you add more
            MessageType_MAX_VALUE // Helper for validation or iteration if needed, not a real message type
        };

        // Helper function to get string name of MessageType
        inline const char* EnumNameMessageType(MessageType e) {
            switch (e) {
                // C2S
            case MessageType::Unknown: return "Unknown";
            case MessageType::C2S_MovementInput: return "C2S_MovementInput";
            case MessageType::C2S_RiftStepActivation: return "C2S_RiftStepActivation";
            case MessageType::C2S_UseAbility: return "C2S_UseAbility";
            case MessageType::C2S_Ping: return "C2S_Ping";
            case MessageType::C2S_TurnIntent: return "C2S_TurnIntent";
            case MessageType::C2S_JoinRequest: return "C2S_JoinRequest"; // <<< ADDED

                // S2C
            case MessageType::S2C_EntityStateUpdate: return "S2C_EntityStateUpdate";
            case MessageType::S2C_RiftStepInitiated: return "S2C_RiftStepInitiated";
            case MessageType::S2C_ResourceUpdate: return "S2C_ResourceUpdate";
            case MessageType::S2C_CombatEvent: return "S2C_CombatEvent";
            case MessageType::S2C_Pong: return "S2C_Pong";
            case MessageType::S2C_SystemBroadcast: return "S2C_SystemBroadcast";
            case MessageType::C2S_BasicAttackIntent: return "C2S_BasicAttackIntent";
            case MessageType::S2C_SpawnProjectile: return "S2C_SpawnProjectile";
            case MessageType::S2C_JoinSuccess: return "S2C_JoinSuccess";         // <<< ADDED
            case MessageType::S2C_JoinFailed: return "S2C_JoinFailed";           // <<< ADDED
            case MessageType::S2C_PlayerDisconnected: return "S2C_PlayerDisconnected"; // <<< ADDED
            case MessageType::S2C_EntityDestroyed: return "S2C_EntityDestroyed";

            default: return "UnhandledMessageType";
            }
        };

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