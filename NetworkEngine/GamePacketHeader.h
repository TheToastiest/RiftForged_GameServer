// File: GamePacketHeader.h
// RiftForged Game Engine
// Copyright (C) 2022-2028 RiftForged Team

#pragma once

#include <cstdint> // For fixed-width integer types like uint32_t, uint16_t

namespace RiftForged {
    namespace Networking {

        // Define your overall network protocol ID version.
        // This helps clients/servers detect incompatible protocol versions.
        const uint32_t CURRENT_PROTOCOL_ID_VERSION = 0x00000004; // Version 0.0.4 WITH PHYSX AND LOGIN PROCEDURES

        // Strong typedef for sequence numbers for better readability and type safety.
        // Using uint32_t allows for a large range of sequence numbers before rollover.
        using SequenceNumber = uint32_t;

        // --- Flags for GamePacketHeader::flags ---
        // These flags describe how the *reliability layer* should interpret and process the packet.
        // They are NOT directly related to the application-level message type (which is in FlatBuffers).
        enum class GamePacketFlag : uint8_t {
            NONE = 0,               // No special flags set
            IS_RELIABLE = 1 << 0,   // This packet requires acknowledgment and retransmission
            IS_ACK_ONLY = 1 << 1,   // This packet contains only ACK information, no application payload
            IS_HEARTBEAT = 1 << 2,  // This is a keep-alive packet
            IS_DISCONNECT = 1 << 3, // This packet signals a disconnection
            IS_FRAGMENT_START = 1 << 4, // For future fragmentation implementation (indicates first fragment)
            IS_FRAGMENT_END = 1 << 5,   // For future fragmentation implementation (indicates last fragment)
            // Additional flags can be added here as needed for transport-layer concerns.
        };

        // Helper functions to work with GamePacketFlag bitmask.
        // These allow for easy bitwise operations on the flags enum.
        inline GamePacketFlag operator|(GamePacketFlag a, GamePacketFlag b) {
            return static_cast<GamePacketFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
        }
        inline uint8_t& operator|=(uint8_t& existingFlags, GamePacketFlag flagToAdd) {
            existingFlags |= static_cast<uint8_t>(flagToAdd);
            return existingFlags;
        }
        inline bool HasFlag(uint8_t headerFlags, GamePacketFlag flagToCheck) {
            // Special handling for GamePacketFlag::NONE: it means *no* flags are set.
            if (flagToCheck == GamePacketFlag::NONE) return headerFlags == static_cast<uint8_t>(GamePacketFlag::NONE);
            return (headerFlags & static_cast<uint8_t>(flagToCheck)) == static_cast<uint8_t>(flagToCheck);
        }

#pragma pack(push, 1) // Ensure no padding is added by the compiler, essential for network packets.

        // Defines the fixed-size header for all UDP packets in the RiftForged network protocol.
        // This header is solely for the reliability and transport layers.
        struct GamePacketHeader {
            uint32_t protocolId;      // The current version of the network protocol (for compatibility checks).
            uint8_t flags;            // A bitmask of GamePacketFlag values, indicating packet properties.
            SequenceNumber sequenceNumber; // This packet's unique ID for reliable delivery (0 for unreliable).
            SequenceNumber ackNumber;      // Highest sequence number received from the remote peer (for ACKing their packets).
            uint32_t ackBitfield;     // A bitfield of recently received (but not highest) sequence numbers from the remote.

            // Constructor initializes default values for a new header.
            // Actual sequence/ack numbers are filled in by the reliability protocol.
            GamePacketHeader(uint8_t initialFlags = static_cast<uint8_t>(GamePacketFlag::NONE))
                : protocolId(CURRENT_PROTOCOL_ID_VERSION),
                flags(initialFlags),
                sequenceNumber(0), // Placeholder; assigned by PrepareOutgoingPacket if IS_RELIABLE.
                ackNumber(0),      // Placeholder; filled by PrepareOutgoingPacket with current remote ACK state.
                ackBitfield(0)     // Placeholder; filled by PrepareOutgoingPacket with current remote ACK state.
            {
            }
        };

#pragma pack(pop) // Restore previous packing alignment.

        // Helper function to get the size of the GamePacketHeader structure.
        // Using constexpr allows this to be evaluated at compile time.
        constexpr size_t GetGamePacketHeaderSize() {
            return sizeof(GamePacketHeader);
        }

    } // namespace Networking
} // namespace RiftForged