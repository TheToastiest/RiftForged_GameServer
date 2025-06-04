// File: NetworkEngine/UDPReliabilityProtocol.cpp
// RiftForged Game Engine
// Copyright (C) 2022-2028 RiftForged Team

#include "UDPReliabilityProtocol.h"
#include "../Utils/Logger.h"       // For RF_NETWORK_... macros
#include "GamePacketHeader.h"      // For GamePacketFlag, SequenceNumber, GetGamePacketHeaderSize, CURRENT_PROTOCOL_ID_VERSION
#include <cstring>                 // For memcpy
#include <vector>                  // For std::vector
#include <list>                    // For std::list in ReliableConnectionState
#include <chrono>                  // For time points
#include <mutex>                   // For std::mutex
#include <cmath>                   // For std::abs in RTT calculation
#include <algorithm>               // For std::min and std::max in RTO clamping
#include <functional>              // For std::function in TrySendAckOnlyPacket

namespace RiftForged {
    namespace Networking {

        // Internal helper function to do the core work of PrepareOutgoingPacket without locking.
        // Assumes the caller (PrepareOutgoingPacket or TrySendAckOnlyPacket) holds the lock on connectionState.internalStateMutex.
        static std::vector<uint8_t> PrepareOutgoingPacketUnlocked_Internal(
            ReliableConnectionState& connectionState,
            const uint8_t* payloadData,
            uint16_t payloadSize,
            uint8_t packetFlags
        ) {
            if (!HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY) && payloadSize > 0 && payloadData == nullptr) {
                RF_NETWORK_WARN("PrepareOutgoingPacketUnlocked: Payload data is null for a non-ACK-only packet with payload size > 0. Flags: 0x{:X}", packetFlags);
                return {};
            }
            if (HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY) && payloadSize > 0) {
                RF_NETWORK_WARN("PrepareOutgoingPacketUnlocked: ACK-only packet should not have a payload. PayloadSize: {}. Ignoring payload.", payloadSize);
                payloadSize = 0;
                payloadData = nullptr;
            }

            GamePacketHeader header;
            header.protocolId = CURRENT_PROTOCOL_ID_VERSION;
            header.flags = packetFlags;
            header.ackNumber = connectionState.highestReceivedSequenceNumberFromRemote;
            header.ackBitfield = connectionState.receivedSequenceBitfield;

            if (HasFlag(packetFlags, GamePacketFlag::IS_RELIABLE)) {
                header.sequenceNumber = connectionState.nextOutgoingSequenceNumber++;
                RF_NETWORK_TRACE("PrepareOutgoingPacketUnlocked: RELIABLE packet Seq: {}, Ack: {}, AckBits: 0x{:08X}, Flags: 0x{:X}",
                    header.sequenceNumber, header.ackNumber, header.ackBitfield, header.flags);
            }
            else {
                header.sequenceNumber = 0;
                RF_NETWORK_TRACE("PrepareOutgoingPacketUnlocked: UNRELIABLE packet, Ack: {}, AckBits: 0x{:08X}, Flags: 0x{:X}",
                    header.ackNumber, header.ackBitfield, header.flags);
            }

            std::vector<uint8_t> packetBuffer = SerializePacket(header, payloadData, payloadSize);

            if (HasFlag(packetFlags, GamePacketFlag::IS_RELIABLE)) {
                connectionState.unacknowledgedSentPackets.emplace_back(
                    header.sequenceNumber,
                    packetBuffer,
                    HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY)
                );
                RF_NETWORK_TRACE("PrepareOutgoingPacketUnlocked: Queued reliable packet Seq: {} for ACK. Unacked count: {}",
                    header.sequenceNumber, connectionState.unacknowledgedSentPackets.size());
            }

            connectionState.hasPendingAckToSend = false; // This packet carries ACKs or is fresh
            connectionState.lastPacketSentTimeToRemote = std::chrono::steady_clock::now();
            return packetBuffer;
        }

        // Helper function to serialize the GamePacketHeader and payload into a byte vector.
        std::vector<uint8_t> SerializePacket(const GamePacketHeader& header, const uint8_t* payload, uint16_t payloadSize) {
            std::vector<uint8_t> packetBuffer(GetGamePacketHeaderSize() + payloadSize);
            std::memcpy(packetBuffer.data(), &header, GetGamePacketHeaderSize());
            if (payload && payloadSize > 0) {
                std::memcpy(packetBuffer.data() + GetGamePacketHeaderSize(), payload, payloadSize);
            }
            return packetBuffer;
        }

        // Helper function to deserialize raw bytes into a GamePacketHeader.
        GamePacketHeader DeserializePacketHeader(const uint8_t* data, uint16_t dataSize) {
            GamePacketHeader header;
            if (dataSize >= GetGamePacketHeaderSize()) {
                std::memcpy(&header, data, GetGamePacketHeaderSize());
            }
            else {
                RF_NETWORK_ERROR("DeserializePacketHeader: Data size {} too small for GamePacketHeader.", dataSize);
                return GamePacketHeader{};
            }
            return header;
        }

        // --- PrepareOutgoingPacket ---
        std::vector<uint8_t> PrepareOutgoingPacket(
            ReliableConnectionState& connectionState,
            const uint8_t* payloadData,
            uint16_t payloadSize,
            uint8_t packetFlags
        ) {
            std::lock_guard<std::mutex> lock(connectionState.internalStateMutex);
            return PrepareOutgoingPacketUnlocked_Internal(connectionState, payloadData, payloadSize, packetFlags);
        }

        // --- ProcessIncomingPacketHeader ---
        bool ProcessIncomingPacketHeader(
            ReliableConnectionState& connectionState,
            const GamePacketHeader& receivedHeader,
            const uint8_t* packetPayloadData,
            uint16_t packetPayloadLength,
            const uint8_t** out_payloadToProcess,
            uint16_t* out_payloadSize
        ) {
            std::lock_guard<std::mutex> lock(connectionState.internalStateMutex);

            if (out_payloadToProcess) *out_payloadToProcess = nullptr;
            if (out_payloadSize) *out_payloadSize = 0;

            connectionState.lastPacketReceivedTimeFromRemote = std::chrono::steady_clock::now();

            SequenceNumber remoteAckNum = receivedHeader.ackNumber;
            uint32_t remoteAckBits = receivedHeader.ackBitfield;

            if (remoteAckNum > 0 || remoteAckBits > 0 || HasFlag(receivedHeader.flags, GamePacketFlag::IS_ACK_ONLY)) {
                RF_NETWORK_TRACE("ACK RECV: Processing ACKs from remote: RemoteAckNum={}, RemoteAckBits=0x{:08X}. Our current unacked count: {}. HeaderFlags=0x{:02X}",
                    remoteAckNum, remoteAckBits, connectionState.unacknowledgedSentPackets.size(), receivedHeader.flags);
            }

            size_t preAckRemovalCount = connectionState.unacknowledgedSentPackets.size();
            int actualAckedCountThisPass = 0;

            connectionState.unacknowledgedSentPackets.remove_if(
                [&](const ReliableConnectionState::SentPacketInfo& sentPacket) {
                    bool acknowledged = false;
                    if (sentPacket.sequenceNumber == remoteAckNum) {
                        acknowledged = true;
                        RF_NETWORK_INFO("ACK MATCH: Direct ACK for our_sent_seq={} by remote_ack_num={}. Marking for removal.",
                            sentPacket.sequenceNumber, remoteAckNum);
                    }
                    else if (sentPacket.sequenceNumber < remoteAckNum) {
                        uint32_t diff = remoteAckNum - sentPacket.sequenceNumber;
                        if (diff > 0 && diff <= 32) {
                            uint32_t bitIndex = diff - 1;
                            if ((remoteAckBits >> bitIndex) & 1U) {
                                acknowledged = true;
                                RF_NETWORK_INFO("ACK MATCH: Bitfield ACK for our_sent_seq={} (diff={}, bitIndex={}) by remote_ack_num={}, remote_ack_bits=0x{:08X}. Marking for removal.",
                                    sentPacket.sequenceNumber, diff, bitIndex, remoteAckNum, remoteAckBits);
                            }
                            else {
                                RF_NETWORK_TRACE("ACK CHECK: Bitfield NO match for our_sent_seq={} (diff={}, bitIndex={}). Remote AckBits: 0x{:08X}, Bit to test: 0x{:08X}",
                                    sentPacket.sequenceNumber, diff, bitIndex, remoteAckBits, (1U << bitIndex));
                            }
                        }
                        else {
                            RF_NETWORK_TRACE("ACK CHECK: our_sent_seq={} is too old (diff={}) to be in bitfield of remote_ack_num={}. Not acknowledged by this packet.",
                                sentPacket.sequenceNumber, diff, remoteAckNum);
                        }
                    }
                    else {
                        RF_NETWORK_TRACE("ACK CHECK: our_sent_seq={} > remote_ack_num={}. Not ACKed by this ack number or bitfield.",
                            sentPacket.sequenceNumber, remoteAckNum);
                    }

                    if (acknowledged) {
                        actualAckedCountThisPass++;
                        if (sentPacket.retries == 0) {
                            float rtt_sample_ms = static_cast<float>(
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - sentPacket.timeSent
                                ).count()
                                );
                            RF_NETWORK_TRACE("RTT Sample for Seq {}: {:.2f} ms", sentPacket.sequenceNumber, rtt_sample_ms);
                            connectionState.ApplyRTTSampleUnlocked(rtt_sample_ms); // <<< USING UNLOCKED VERSION
                            RF_NETWORK_INFO("RTO Updated for connection: {:.2f} ms (SRTT: {:.2f}, RTTVAR: {:.2f})",
                                connectionState.retransmissionTimeout_ms,
                                connectionState.smoothedRTT_ms,
                                connectionState.rttVariance_ms);
                        }
                        else {
                            RF_NETWORK_TRACE("RTT Sample Skipped for retransmitted packet Seq {} (retries={})",
                                sentPacket.sequenceNumber, sentPacket.retries);
                        }
                        return true;
                    }
                    return false;
                }
            );

            if (actualAckedCountThisPass > 0) {
                RF_NETWORK_TRACE("Processed {} ACKs. Unacked packets remaining: {} (was {})",
                    actualAckedCountThisPass, connectionState.unacknowledgedSentPackets.size(), preAckRemovalCount);
            }
            else if (preAckRemovalCount > 0 && (remoteAckNum > 0 || remoteAckBits > 0)) {
                RF_NETWORK_TRACE("ACK PROC: No new packets ACKed this pass. RemoteAckNum={}, RemoteAckBits=0x{:08X}. Unacked count remains {}.",
                    remoteAckNum, remoteAckBits, connectionState.unacknowledgedSentPackets.size());
            }

            bool shouldRelayToGameLogic = false;
            bool ackStateForRemoteUpdated = false;

            if (HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
                SequenceNumber incomingSeqNum = receivedHeader.sequenceNumber;
                RF_NETWORK_TRACE("RECV RELIABLE: Incoming reliable packet Seq={}. Our highest_remote_seq={}, our_ack_bits_for_them=0x{:08X}",
                    incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);

                if (IsSequenceGreaterThan(incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote)) {
                    uint32_t diff = incomingSeqNum - connectionState.highestReceivedSequenceNumberFromRemote; // Careful with wrap-around if not using IsSequenceGreaterThan
                    if (diff >= 32) { // If using proper sequence comparison, diff is direct for positive jumps
                        connectionState.receivedSequenceBitfield = 0;
                        RF_NETWORK_WARN("RECV RELIABLE: Large sequence number jump detected (Seq={}, prev_highest={}, diff={}). Resetting receivedSequenceBitfield.",
                            incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote, diff);
                    }
                    else {
                        connectionState.receivedSequenceBitfield <<= diff;
                        // Bit for the *old* highest needs to be set before new highest is updated
                        if (connectionState.highestReceivedSequenceNumberFromRemote > 0 || diff > 0) { // Only if old highest was valid or there's a diff
                            connectionState.receivedSequenceBitfield |= (1U << (diff - 1));
                        }
                    }
                    connectionState.highestReceivedSequenceNumberFromRemote = incomingSeqNum;
                    shouldRelayToGameLogic = true;
                    ackStateForRemoteUpdated = true;
                    RF_NETWORK_INFO("RECV RELIABLE: New highest remote Seq={}. Our ACK state FOR THEM: highest_ack_to_send={}, bits_to_send=0x{:08X}. Will process payload.",
                        incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);
                }
                else if (IsSequenceLessThan(incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote)) {
                    uint32_t diff = connectionState.highestReceivedSequenceNumberFromRemote - incomingSeqNum; // Careful with wrap-around
                    if (diff > 0 && diff <= 32) {
                        uint32_t bitToSet = (1U << (diff - 1));
                        if (!(connectionState.receivedSequenceBitfield & bitToSet)) {
                            connectionState.receivedSequenceBitfield |= bitToSet;
                            shouldRelayToGameLogic = true;
                            ackStateForRemoteUpdated = true;
                            RF_NETWORK_INFO("RECV RELIABLE: Accepted out-of-order remote Seq={} (diff={}). Our ACK state FOR THEM: highest_ack_to_send={}, bits_to_send=0x{:08X}. Will process payload.",
                                incomingSeqNum, diff, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);
                        }
                        else {
                            RF_NETWORK_TRACE("RECV RELIABLE: Duplicate OLD reliable remote Seq={} (already in bitfield). Discarding payload.", incomingSeqNum);
                            shouldRelayToGameLogic = false;
                        }
                    }
                    else {
                        RF_NETWORK_TRACE("RECV RELIABLE: Very OLD reliable remote Seq={} (older than highest_remote_seq {} - 32). Discarding payload.",
                            incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote);
                        shouldRelayToGameLogic = false;
                    }
                }
                else { // incomingSeqNum == connectionState.highestReceivedSequenceNumberFromRemote
                    RF_NETWORK_TRACE("RECV RELIABLE: Duplicate of current highest remote Seq={}. Discarding payload.", incomingSeqNum);
                    shouldRelayToGameLogic = false;
                }
            }
            else if (packetPayloadData && packetPayloadLength > 0 && !HasFlag(receivedHeader.flags, GamePacketFlag::IS_ACK_ONLY)) {
                RF_NETWORK_TRACE("RECV UNRELIABLE: Received UNRELIABLE packet with payload. Flags: 0x{:X}. Will process payload.", receivedHeader.flags);
                shouldRelayToGameLogic = true;
            }
            else if (HasFlag(receivedHeader.flags, GamePacketFlag::IS_ACK_ONLY)) {
                RF_NETWORK_TRACE("RECV ACK_ONLY: Processed ACKs. No payload to relay. Flags: 0x{:X}", receivedHeader.flags);
                shouldRelayToGameLogic = false;
            }
            else {
                RF_NETWORK_TRACE("RECV: Packet has no game logic payload to process (e.g., unreliable empty). Flags: 0x{:02X}.", receivedHeader.flags);
                shouldRelayToGameLogic = false;
            }

            if (ackStateForRemoteUpdated) {
                connectionState.hasPendingAckToSend = true;
                RF_NETWORK_TRACE("ACK STATE UPDATE: Marking hasPendingAckToSend=true for remote (because we received new reliable data Seq={}).", receivedHeader.sequenceNumber);
            }

            if (shouldRelayToGameLogic) {
                if (packetPayloadData && packetPayloadLength > 0) {
                    if (out_payloadToProcess) *out_payloadToProcess = packetPayloadData;
                    if (out_payloadSize) *out_payloadSize = packetPayloadLength;
                    RF_NETWORK_TRACE("PAYLOAD TO PROCESS: Yes, Size: {}. Flags: 0x{:X}", packetPayloadLength, receivedHeader.flags);
                    return true;
                }
                else {
                    RF_NETWORK_WARN("PAYLOAD TO PROCESS: Decision to relay, but no payload data. Flags: 0x{:X}. Likely an internal logic error.", receivedHeader.flags);
                    return false;
                }
            }

            RF_NETWORK_TRACE("PAYLOAD TO PROCESS: No (End of function decision for packet with flags: 0x{:02X})", receivedHeader.flags);
            return false;
        }

        // --- GetPacketsForRetransmission ---
        std::vector<std::vector<uint8_t>> GetPacketsForRetransmission(
            ReliableConnectionState& connectionState,
            std::chrono::steady_clock::time_point currentTime
        ) {
            std::vector<std::vector<uint8_t>> packetsToResend;
            std::lock_guard<std::mutex> lock(connectionState.internalStateMutex);
            auto it = connectionState.unacknowledgedSentPackets.begin();

            while (it != connectionState.unacknowledgedSentPackets.end()) {
                ReliableConnectionState::SentPacketInfo& sentPacket = *it;
                auto timeSinceSent = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - sentPacket.timeSent);

                if (timeSinceSent.count() >= static_cast<long long>(connectionState.retransmissionTimeout_ms)) {
                    if (connectionState.ShouldDropPacket(sentPacket.retries)) {
                        RF_NETWORK_ERROR("MAX RETRIES: Packet Seq={} EXCEEDED MAX RETRIES ({}). RTO used: {:.0f}ms. Dropping packet and flagging connection as lost.",
                            sentPacket.sequenceNumber, MAX_PACKET_RETRIES, connectionState.retransmissionTimeout_ms);
                        connectionState.connectionDroppedByMaxRetries = true;
                        connectionState.isConnected = false;
                        it = connectionState.unacknowledgedSentPackets.erase(it);
                    }
                    else {
                        sentPacket.retries++;
                        sentPacket.timeSent = currentTime;
                        packetsToResend.push_back(sentPacket.packetData);

                        // Store current RTO before doubling for logging
                        float rtoThatTriggered = connectionState.retransmissionTimeout_ms;

                        connectionState.retransmissionTimeout_ms = connectionState.retransmissionTimeout_ms * 2.0f;
                        connectionState.retransmissionTimeout_ms = std::min(connectionState.retransmissionTimeout_ms, MAX_RTO_MS);
                        connectionState.retransmissionTimeout_ms = std::max(connectionState.retransmissionTimeout_ms, MIN_RTO_MS);

                        RF_NETWORK_WARN("RETRANSMIT: Packet Seq={} (Attempt #{}). RTO that triggered retransmit: {:.0f}ms. New connection RTO: {:.0f}ms",
                            sentPacket.sequenceNumber, sentPacket.retries,
                            rtoThatTriggered,
                            connectionState.retransmissionTimeout_ms);
                        it++;
                    }
                }
                else {
                    it++;
                }
            }
            if (!packetsToResend.empty()) {
                RF_NETWORK_TRACE("RETRANSMIT: Found {} packets to retransmit this cycle.", packetsToResend.size());
            }
            return packetsToResend;
        }

        // --- TrySendAckOnlyPacket ---
        bool TrySendAckOnlyPacket(ReliableConnectionState& connectionState,
            std::chrono::steady_clock::time_point currentTime,
            std::function<void(const std::vector<uint8_t>&)> sendPacketFunc) {

            // Temp store values needed outside lock to avoid holding lock during PrepareOutgoingPacketUnlocked_Internal
            bool needsToSendAck = false;
            uint8_t flags = 0;
            SequenceNumber currentHighestRemoteSeq = 0; // For logging
            uint32_t currentRemoteAckBits = 0;         // For logging
            long long calculatedTimeSinceLastSent = 0; // For logging

            { // Scope for the first lock
                std::lock_guard<std::mutex> lock(connectionState.internalStateMutex);
                if (!connectionState.hasPendingAckToSend) {
                    return false;
                }

                float ackDelayThresholdMs = std::min(connectionState.smoothedRTT_ms / 4.0f, 20.0f); // e.g. RTT/4 or max 20ms
                if (ackDelayThresholdMs < 5.0f) ackDelayThresholdMs = 5.0f; // Minimum 5ms delay

                calculatedTimeSinceLastSent = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - connectionState.lastPacketSentTimeToRemote
                ).count();

                if (connectionState.lastPacketSentTimeToRemote == std::chrono::steady_clock::time_point::min() ||
                    calculatedTimeSinceLastSent >= static_cast<long long>(ackDelayThresholdMs)) {
                    needsToSendAck = true;
                    flags = static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(GamePacketFlag::IS_ACK_ONLY);
                    currentHighestRemoteSeq = connectionState.highestReceivedSequenceNumberFromRemote; // For logging
                    currentRemoteAckBits = connectionState.receivedSequenceBitfield;                   // For logging
                }
                else {
                    RF_NETWORK_TRACE("Not sending ACK-only yet. Time since last sent: {}ms, Threshold: {:.0f}ms. Pending: {}",
                        calculatedTimeSinceLastSent, ackDelayThresholdMs, connectionState.hasPendingAckToSend);
                }
            } // Lock released

            if (needsToSendAck) {
                // PrepareOutgoingPacketUnlocked_Internal expects the lock to be held by ITS caller if it's going to modify
                // shared state like nextOutgoingSequenceNumber or unacknowledgedSentPackets.
                // However, for an ACK-only packet, it mainly reads ackNumber/ackBitfield and sets flags.
                // The critical part is that it *does* update hasPendingAckToSend and lastPacketSentTimeToRemote,
                // and adds to unacknowledgedSentPackets. So it *does* need the lock.
                // The original problem was TrySendAckOnlyPacket locking, then calling public PrepareOutgoingPacket which also locked.
                // Now, TrySendAckOnlyPacket can lock, then call the internal PrepareOutgoingPacketUnlocked_Internal

                std::vector<uint8_t> ackPacket;
                { // Scope for the lock needed by PrepareOutgoingPacketUnlocked_Internal
                    std::lock_guard<std::mutex> lock(connectionState.internalStateMutex);
                    ackPacket = PrepareOutgoingPacketUnlocked_Internal( // Use the internal unlocked version
                        connectionState,
                        nullptr,
                        0,
                        flags
                    );
                } // Lock for PrepareOutgoingPacketUnlocked_Internal released

                if (!ackPacket.empty()) {
                    sendPacketFunc(ackPacket); // Use the provided callback to send the packet.
                    // Note: hasPendingAckToSend is set to false inside PrepareOutgoingPacketUnlocked_Internal
                    RF_NETWORK_DEBUG("Sent ACK-only packet (Header Seq: {}, Acking Remote Seq: {}, Bits: 0x{:08X}) after {}ms delay.",
                        reinterpret_cast<const GamePacketHeader*>(ackPacket.data())->sequenceNumber,
                        currentHighestRemoteSeq,
                        currentRemoteAckBits,
                        calculatedTimeSinceLastSent);
                    return true;
                }
                else {
                    RF_NETWORK_ERROR("Failed to prepare ACK-only packet with PrepareOutgoingPacketUnlocked_Internal.");
                }
            }
            return false;
        }

    } // namespace Networking
} // namespace RiftForged