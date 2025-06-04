// File: NetworkEngine/UDPReliabilityProtocol.h
// RiftForged Game Engine
// Copyright (C) 2022-2028 RiftForged Team

#pragma once

#include <cstdint>   // For uint32_t, uint16_t, uint8_t
#include <vector>    // For std::vector
#include <string>    // For std::string
#include <chrono>    // For std::chrono::steady_clock
#include <functional>// For std::function

#include "ReliableConnectionState.h" // <<< INCLUDE THE NEW HEADER
#include "GamePacketHeader.h"        // Still needed for GamePacketHeader struct used in function signatures

namespace RiftForged {
    namespace Networking {

        // Declarations of the protocol functions.
        // ReliableConnectionState is now defined in ReliableConnectionState.h
        // Constants like RTT_ALPHA, etc., are now also in ReliableConnectionState.h
        inline bool IsSequenceGreaterThan(SequenceNumber s1, SequenceNumber s2) {
            // For example, if SequenceNumber is uint32_t:
            // const uint32_t halfRange = 0x80000000U; // For uint32_t
            // If SequenceNumber is uint16_t:
            // const uint16_t halfRange = 0x8000U; // For uint16_t

            // Generic approach (assuming SequenceNumber is unsigned):
            using T = SequenceNumber; // Get the actual type
            const T halfRange = (static_cast<T>(-1) / 2) + 1; // Max value / 2 + 1, works for unsigned types

            return ((s1 > s2) && (s1 - s2 < halfRange)) ||
                ((s2 > s1) && (s2 - s1 >= halfRange)); // s1 wrapped
        }

        inline bool IsSequenceLessThan(SequenceNumber s1, SequenceNumber s2) {
            return IsSequenceGreaterThan(s2, s1); // s1 < s2 is equivalent to s2 > s1
        }

        inline bool IsSequenceGreaterEqual(SequenceNumber s1, SequenceNumber s2) {
            return IsSequenceGreaterThan(s1, s2) || (s1 == s2);
        }

        std::vector<uint8_t> PrepareOutgoingPacket(
            ReliableConnectionState& connectionState,
            const uint8_t* payloadData,
            uint16_t payloadSize,
            uint8_t packetFlags
        );

        bool ProcessIncomingPacketHeader(
            ReliableConnectionState& connectionState,
            const GamePacketHeader& receivedHeader,
            const uint8_t* packetPayloadData,
            uint16_t packetPayloadLength,
            const uint8_t** out_payloadToProcess,
            uint16_t* out_payloadSize
        );

        std::vector<std::vector<uint8_t>> GetPacketsForRetransmission(
            ReliableConnectionState& connectionState,
            std::chrono::steady_clock::time_point currentTime
        );

        bool TrySendAckOnlyPacket(
            ReliableConnectionState& connectionState,
            std::chrono::steady_clock::time_point currentTime,
            std::function<void(const std::vector<uint8_t>&)> sendPacketFunc
        );

        // These helpers might be better as static functions within UDPReliabilityProtocol.cpp
        // or remain here if they are truly general utilities for packet manipulation.
        // For now, keeping their declarations here.
        std::vector<uint8_t> SerializePacket(const GamePacketHeader& header, const uint8_t* payload, uint16_t payloadSize);
        GamePacketHeader DeserializePacketHeader(const uint8_t* data, uint16_t dataSize);

    } // namespace Networking
} // namespace RiftForged