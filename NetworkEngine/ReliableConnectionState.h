// File: NetworkEngine/ReliableConnectionState.h
// RiftForged Game Engine
// Copyright (C) 2022-2028 RiftForged Team

#pragma once

#include <cstdint>   // For uint32_t, uint16_t, uint8_t
#include <vector>    // For std::vector
#include <chrono>    // For std::chrono::steady_clock
#include <list>      // For std::list
#include <mutex>     // For std::mutex
#include <algorithm> // For std::min and std::max
#include <cmath>     // For std::abs

// Include GamePacketHeader as it defines SequenceNumber and GamePacketFlag (if used directly by methods)
// Or if SequenceNumber is a primitive, this might not be strictly needed here but good for context.
// Assuming SequenceNumber is defined in GamePacketHeader.h or is a basic type.
#include "GamePacketHeader.h" // For SequenceNumber type

namespace RiftForged {
    namespace Networking {

        // Forward declare ProcessIncomingPacketHeader for the friend declaration
        // Note: The full signature must match exactly.
        struct GamePacketHeader; // Already included via GamePacketHeader.h but good to be explicit if used directly
        class ReliableConnectionState; // Forward declare for the free function signature

        bool ProcessIncomingPacketHeader(
            ReliableConnectionState& connectionState,
            const GamePacketHeader& receivedHeader,
            const uint8_t* packetPayloadData,
            uint16_t packetPayloadLength,
            const uint8_t** out_payloadToProcess,
            uint16_t* out_payloadSize);

        // RTT calculation constants (based on RFC 6298 recommendations)
        const float RTT_ALPHA = 0.125f; // Factor for SRTT (g)
        const float RTT_BETA = 0.250f;  // Factor for RTTVAR (h)
        const float RTO_K = 4.0f;       // Multiplier for RTTVAR in RTO calculation

        const float DEFAULT_INITIAL_RTT_MS = 200.0f; // Initial Round Trip Time guess
        const float MIN_RTO_MS = 100.0f;             // Minimum Retransmission Timeout
        const float MAX_RTO_MS = 3000.0f;            // Maximum Retransmission Timeout

        // Global maximum retries for a reliable packet before considering the connection dropped.
        const int MAX_PACKET_RETRIES = 10;


        struct ReliableConnectionState {
            mutable std::mutex internalStateMutex;

            SequenceNumber nextOutgoingSequenceNumber = 1;

            struct SentPacketInfo {
                SequenceNumber sequenceNumber;
                std::chrono::steady_clock::time_point timeSent;
                std::vector<uint8_t> packetData;
                int retries = 0;
                bool isAckOnly = false;

                SentPacketInfo(SequenceNumber seq, const std::vector<uint8_t>& data, bool ackOnlyFlag)
                    : sequenceNumber(seq),
                    timeSent(std::chrono::steady_clock::now()),
                    packetData(data),
                    retries(0),
                    isAckOnly(ackOnlyFlag) {
                }
            };
            std::list<SentPacketInfo> unacknowledgedSentPackets;

            SequenceNumber highestReceivedSequenceNumberFromRemote = 0;
            uint32_t receivedSequenceBitfield = 0;

            bool hasPendingAckToSend = false;
            std::chrono::steady_clock::time_point lastPacketSentTimeToRemote;
            std::chrono::steady_clock::time_point lastPacketReceivedTimeFromRemote;

            float smoothedRTT_ms;
            float rttVariance_ms;
            float retransmissionTimeout_ms;
            bool isFirstRTTSample;

            bool connectionDroppedByMaxRetries;
            bool isConnected;

            struct IncomingFragmentBuffer {
                SequenceNumber fragmentStartSequenceNumber = 0;
                uint16_t totalFragments = 0;
                uint16_t receivedFragmentCount = 0;
                std::vector<std::vector<uint8_t>> fragments;
                std::chrono::steady_clock::time_point lastFragmentArrivalTime;
                bool awaitingFragments = false;

                void Reset() {
                    fragmentStartSequenceNumber = 0;
                    totalFragments = 0;
                    receivedFragmentCount = 0;
                    fragments.clear();
                    lastFragmentArrivalTime = std::chrono::steady_clock::time_point::min();
                    awaitingFragments = false;
                }
            };
            IncomingFragmentBuffer incomingFragmentBuffer;

        private:
            // This version does the actual work and ASSUMES internalStateMutex is ALREADY HELD by the caller.
            void ApplyRTTSampleUnlocked(float sampleRTT_ms) {
                if (isFirstRTTSample) {
                    smoothedRTT_ms = sampleRTT_ms;
                    rttVariance_ms = sampleRTT_ms / 2.0f;
                    isFirstRTTSample = false;
                }
                else {
                    float rtt_delta = sampleRTT_ms - smoothedRTT_ms;
                    smoothedRTT_ms = smoothedRTT_ms + RTT_ALPHA * rtt_delta;
                    rttVariance_ms = rttVariance_ms + RTT_BETA * (std::abs(rtt_delta) - rttVariance_ms);
                }

                retransmissionTimeout_ms = smoothedRTT_ms + RTO_K * rttVariance_ms;
                retransmissionTimeout_ms = std::max(MIN_RTO_MS, retransmissionTimeout_ms);
                retransmissionTimeout_ms = std::min(MAX_RTO_MS, retransmissionTimeout_ms);
            }

        public:
            ReliableConnectionState()
                : lastPacketSentTimeToRemote(std::chrono::steady_clock::time_point::min()),
                lastPacketReceivedTimeFromRemote(std::chrono::steady_clock::time_point::min()),
                smoothedRTT_ms(DEFAULT_INITIAL_RTT_MS),
                rttVariance_ms(DEFAULT_INITIAL_RTT_MS / 2.0f),
                retransmissionTimeout_ms(DEFAULT_INITIAL_RTT_MS * 2.0f),
                isFirstRTTSample(true),
                connectionDroppedByMaxRetries(false),
                isConnected(true) {
                if (retransmissionTimeout_ms < MIN_RTO_MS) retransmissionTimeout_ms = MIN_RTO_MS;
                if (retransmissionTimeout_ms > MAX_RTO_MS) retransmissionTimeout_ms = MAX_RTO_MS;
            }

            void Reset() {
                std::lock_guard<std::mutex> lock(internalStateMutex);
                nextOutgoingSequenceNumber = 1;
                unacknowledgedSentPackets.clear();
                highestReceivedSequenceNumberFromRemote = 0;
                receivedSequenceBitfield = 0;
                hasPendingAckToSend = false;
                lastPacketSentTimeToRemote = std::chrono::steady_clock::time_point::min();
                lastPacketReceivedTimeFromRemote = std::chrono::steady_clock::time_point::min();
                isFirstRTTSample = true;
                connectionDroppedByMaxRetries = false;
                isConnected = true; // Or false, depending on desired reset state
                incomingFragmentBuffer.Reset();
                smoothedRTT_ms = DEFAULT_INITIAL_RTT_MS;
                rttVariance_ms = DEFAULT_INITIAL_RTT_MS / 2.0f;
                retransmissionTimeout_ms = DEFAULT_INITIAL_RTT_MS * 2.0f;
                if (retransmissionTimeout_ms < MIN_RTO_MS) retransmissionTimeout_ms = MIN_RTO_MS;
                if (retransmissionTimeout_ms > MAX_RTO_MS) retransmissionTimeout_ms = MAX_RTO_MS;
            }

            void ApplyRTTSample(float sampleRTT_ms) {
                std::lock_guard<std::mutex> lock(internalStateMutex);
                ApplyRTTSampleUnlocked(sampleRTT_ms);
            }

            bool ShouldDropPacket(int retries) const {
                return retries >= MAX_PACKET_RETRIES;
            }

#ifdef _DEBUG
            void ForceAcknowledgePacket(SequenceNumber seq) {
                std::lock_guard<std::mutex> lock(internalStateMutex);
                for (auto it = unacknowledgedSentPackets.begin(); it != unacknowledgedSentPackets.end(); ++it) {
                    if (it->sequenceNumber == seq) {
                        unacknowledgedSentPackets.erase(it);
                        return;
                    }
                }
            }
#endif
            // Friend declaration to allow ProcessIncomingPacketHeader to call ApplyRTTSampleUnlocked
            friend bool ProcessIncomingPacketHeader(
                ReliableConnectionState& connectionState,
                const GamePacketHeader& receivedHeader,
                const uint8_t* packetPayloadData,
                uint16_t packetPayloadLength,
                const uint8_t** out_payloadToProcess,
                uint16_t* out_payloadSize);
        };

    } // namespace Networking
} // namespace RiftForged