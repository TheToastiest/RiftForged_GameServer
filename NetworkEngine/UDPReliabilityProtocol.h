#pragma once

#include <cstdint>  // For uint32_t, uint16_t, uint8_t
#include <vector>   // For std::vector
#include <string>   // For std::string
#include <chrono>   // For std::chrono::steady_clock
#include <list>     // For std::list
#include <map>      // For potential future use
#include <mutex>

// Include your existing header that defines GamePacketHeader and MessageType
#include "GamePacketHeader.h" 

namespace RiftForged {
    namespace Networking {

        // RTT calculation constants (based on RFC 6298 recommendations)
        const float RTT_ALPHA = 0.125f; // For SRTT (g)
        const float RTT_BETA = 0.250f; // For RTTVAR (h)
        const float RTO_K = 4.0f;      // Multiplier for RTTVAR in RTO calculation

        const float DEFAULT_INITIAL_RTT_MS = 200.0f; // Initial RTT guess, similar to your old fixed RTO
        const float MIN_RTO_MS = 100.0f;             // Minimum RTO
        const float MAX_RTO_MS = 3000.0f;            // Maximum RTO (to prevent overly long waits)

        // --- Flags for GamePacketHeader::flags ---
        enum class GamePacketFlag : uint8_t {
            NONE = 0,
            IS_RELIABLE = 1 << 0,
            IS_ACK_ONLY = 1 << 1,
            IS_HEARTBEAT = 1 << 2,
            IS_DISCONNECT = 1 << 3,
            IS_FRAGMENT_START = 1 << 4, // For future
            IS_FRAGMENT_END = 1 << 5, // For future
        };

        // Helper functions to work with GamePacketFlag bitmask
        inline GamePacketFlag operator|(GamePacketFlag a, GamePacketFlag b) {
            return static_cast<GamePacketFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
        }
        inline uint8_t& operator|=(uint8_t& existingFlags, GamePacketFlag flagToAdd) {
            existingFlags |= static_cast<uint8_t>(flagToAdd);
            return existingFlags;
        }
        inline bool HasFlag(uint8_t headerFlags, GamePacketFlag flagToCheck) {
            if (flagToCheck == GamePacketFlag::NONE) return headerFlags == static_cast<uint8_t>(GamePacketFlag::NONE);
            return (headerFlags & static_cast<uint8_t>(flagToCheck)) == static_cast<uint8_t>(flagToCheck);
        }

        // --- State Management for Each Reliable Connection ---

        // Manages the reliability state for a single connection (e.g., server's view of one client).
        struct ReliableConnectionState {
            mutable std::mutex internalStateMutex; // <<< ADD THIS MUTEX

            // Sending Side (Our outgoing packets to the remote host)
            uint32_t nextOutgoingSequenceNumber = 1; // Start sequence numbers from 1

            // Receiving Side (Processing incoming packets from the remote host, and ACKing them)
            uint32_t highestReceivedSequenceNumberFromRemote = 0;
            uint32_t receivedSequenceBitfield = 0;

            // For managing when to send ACK-only packets
            bool hasPendingAckToSend = false;
            std::chrono::steady_clock::time_point lastPacketSentTimeToRemote;
            std::chrono::steady_clock::time_point lastPacketReceivedTimeFromRemote; // <<< ADD THIS LINE


            // Structure to hold information about a packet that has been sent reliably
            // and is currently awaiting acknowledgment. This is nested inside ReliableConnectionState.
            struct SentPacketInfo { // Correctly nested definition
                uint32_t sequenceNumber;
                std::chrono::steady_clock::time_point timeSent;
                std::vector<uint8_t> packetData; // Holds the full packet (header + payload)
                int retries = 0;                 // Number of times this packet has been retransmitted
                bool isAckOnly = false;          // Was this an ACK-only packet we sent?

                SentPacketInfo(uint32_t seq, const std::vector<uint8_t>& data, bool ackOnlyFlag)
                    : sequenceNumber(seq),
                    timeSent(std::chrono::steady_clock::now()),
                    packetData(data),
                    retries(0),
                    isAckOnly(ackOnlyFlag) {
                }
            };
            std::list<SentPacketInfo> unacknowledgedSentPackets; // Packets we sent that remote hasn't ACKed yet

            // --- NEW/UPDATED RTT and RTO fields ---
            float smoothedRTT_ms;       // SRTT
            float rttVariance_ms;       // RTTVAR
            float retransmissionTimeout_ms; // Calculated RTO
			bool isFirstRTTSample;          // To track if we have a valid SRTT yet
            bool connectionDroppedByMaxRetries; //

            ReliableConnectionState()
                : lastPacketSentTimeToRemote(std::chrono::steady_clock::time_point::min()),
                lastPacketReceivedTimeFromRemote(std::chrono::steady_clock::time_point::min()),
                smoothedRTT_ms(DEFAULT_INITIAL_RTT_MS),
                rttVariance_ms(DEFAULT_INITIAL_RTT_MS / 2.0f),
                retransmissionTimeout_ms(DEFAULT_INITIAL_RTT_MS * 2.0f), // Initial RTO
                isFirstRTTSample(true),
                connectionDroppedByMaxRetries(false) // <<< INITIALIZE THIS
            {
                if (retransmissionTimeout_ms < MIN_RTO_MS) retransmissionTimeout_ms = MIN_RTO_MS;
                if (retransmissionTimeout_ms > MAX_RTO_MS) retransmissionTimeout_ms = MAX_RTO_MS;
            }

            void Reset() {
                std::lock_guard<std::mutex> lock(internalStateMutex); // Lock for reset
                nextOutgoingSequenceNumber = 1;
                unacknowledgedSentPackets.clear();
                highestReceivedSequenceNumberFromRemote = 0;
                receivedSequenceBitfield = 0;
                hasPendingAckToSend = false;
                lastPacketSentTimeToRemote = std::chrono::steady_clock::time_point::min();
                lastPacketReceivedTimeFromRemote = std::chrono::steady_clock::time_point::min();
                isFirstRTTSample = true; // 
                connectionDroppedByMaxRetries = false; // 
            }
        };


        // --- Core Protocol Function Declarations ---
        // (Implementations will go into UDPReliabilityProtocol.cpp)

        std::vector<uint8_t> PrepareOutgoingPacket(
            ReliableConnectionState& connectionState,
            MessageType messageType,
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
            std::chrono::steady_clock::time_point currentTime,
            int maxRetries = 5
        );

        // ... (TODOs for RTT, fragmentation, etc.) ...

    } // namespace Networking
} // namespace RiftForged