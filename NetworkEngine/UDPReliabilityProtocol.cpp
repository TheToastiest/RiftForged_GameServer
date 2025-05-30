// In UDPReliabilityProtocol.cpp

//#include "UDPReliabilityProtocol.h"
//#include "../Utils/Logger.h" // For RF_NETWORK_... macros
//#include <cstring>           // For memcpy
//
//namespace RiftForged {
//    namespace Networking {
//
//        // --- PrepareOutgoingPacket ---
//        std::vector<uint8_t> PrepareOutgoingPacket(
//            ReliableConnectionState& connectionState,
//            MessageType messageType,
//            const uint8_t* payloadData,
//            uint16_t payloadSize,
//            uint8_t packetFlags
//        ) {
//            if (!HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY) && payloadSize > 0 && payloadData == nullptr) {
//                // Corrected Logging:
//                RF_NETWORK_WARN("PrepareOutgoingPacket: Payload data is null for a non-ACK-only packet with payload size > 0. Type: {}", static_cast<uint16_t>(messageType));
//                return {};
//            }
//            if (HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY) && payloadSize > 0) {
//                // Corrected Logging:
//                RF_NETWORK_WARN("PrepareOutgoingPacket: ACK-only packet should not have a payload. Type: {}, PayloadSize: {}. Ignoring payload.", static_cast<uint16_t>(messageType), payloadSize);
//                payloadSize = 0;
//                payloadData = nullptr;
//            }
//
//            GamePacketHeader header;
//            header.protocolId = CURRENT_PROTOCOL_ID_VERSION;
//            header.messageType = messageType;
//            header.flags = packetFlags;
//            header.ackNumber = connectionState.highestReceivedSequenceNumberFromRemote;
//            header.ackBitfield = connectionState.receivedSequenceBitfield;
//
//            if (HasFlag(packetFlags, GamePacketFlag::IS_RELIABLE)) {
//                header.sequenceNumber = connectionState.nextOutgoingSequenceNumber++;
//                // Corrected Logging (if uncommented):
//                RF_NETWORK_TRACE("Preparing RELIABLE packet Seq: {}, Ack: {}, AckBits: 0x{:08X}, Type: {}", 
//                header.sequenceNumber, header.ackNumber, header.ackBitfield, static_cast<uint16_t>(messageType));
//            }
//            else {
//                header.sequenceNumber = 0;
//                // Corrected Logging (if uncommented):
//                RF_NETWORK_TRACE("Preparing UNRELIABLE packet, Ack: {}, AckBits: 0x{:08X}, Type: {}", 
//                header.ackNumber, header.ackBitfield, static_cast<uint16_t>(messageType));
//            }
//
//            std::vector<uint8_t> packetBuffer(GetGamePacketHeaderSize() + payloadSize);
//            std::memcpy(packetBuffer.data(), &header, GetGamePacketHeaderSize());
//            if (payloadData && payloadSize > 0) {
//                std::memcpy(packetBuffer.data() + GetGamePacketHeaderSize(), payloadData, payloadSize);
//            }
//
//            if (HasFlag(packetFlags, GamePacketFlag::IS_RELIABLE)) {
//                connectionState.unacknowledgedSentPackets.emplace_back(
//                    header.sequenceNumber,
//                    packetBuffer,
//                    HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY)
//                );
//                // Corrected Logging (if uncommented):
//                // RF_NETWORK_TRACE("Queued reliable packet Seq: {} for ACK. Unacked count: {}", 
//                //    header.sequenceNumber, connectionState.unacknowledgedSentPackets.size());
//            }
//
//            // This part was in my previous draft but belongs here for PrepareOutgoingPacket
//            connectionState.hasPendingAckToSend = false;
//            connectionState.lastPacketSentTimeToRemote = std::chrono::steady_clock::now();
//
//            return packetBuffer;
//        }
//
//        // --- ProcessIncomingPacketHeader ---
//        bool ProcessIncomingPacketHeader(
//            ReliableConnectionState& connectionState,
//            const GamePacketHeader& receivedHeader,
//            const uint8_t* packetPayloadData,
//            uint16_t packetPayloadLength,
//            const uint8_t** out_payloadToProcess,
//            uint16_t* out_payloadSize
//        ) {
//            if (out_payloadToProcess) *out_payloadToProcess = nullptr;
//            if (out_payloadSize) *out_payloadSize = 0;
//
//            uint32_t remoteAckNum = receivedHeader.ackNumber;
//            uint32_t remoteAckBits = receivedHeader.ackBitfield;
//
//            // Your original logging for "Processing ACKs from remote"
//            if (remoteAckNum > 0 || remoteAckBits > 0) {
//                RF_NETWORK_TRACE("ACK RECV: Processing ACKs from remote: AckNum=%u, AckBits=0x%08X. Current unacked count: %zu",
//                    remoteAckNum, remoteAckBits, connectionState.unacknowledgedSentPackets.size());
//            }
//
//            size_t preAckRemovalCount = connectionState.unacknowledgedSentPackets.size();
//            int actualAckedCountThisPass = 0;
//
//            connectionState.unacknowledgedSentPackets.remove_if(
//                [&](const ReliableConnectionState::SentPacketInfo& sentPacket) {
//                    // Detailed logging for each packet being checked
//                    RF_NETWORK_TRACE("ACK CHECK: Comparing our_sent_seq=%u (isAckOnly=%s) against remote_ack_num=%u, remote_ack_bits=0x%08X",
//                        sentPacket.sequenceNumber, sentPacket.isAckOnly ? "true" : "false", remoteAckNum, remoteAckBits);
//
//                    bool acknowledged = false;
//                    if (sentPacket.sequenceNumber == remoteAckNum) {
//                        acknowledged = true;
//                        RF_NETWORK_INFO("ACK MATCH: Direct ACK for our_sent_seq=%u by remote_ack_num=%u. Marking for removal.",
//                            sentPacket.sequenceNumber, remoteAckNum);
//                    }
//                    else if (sentPacket.sequenceNumber < remoteAckNum) {
//                        uint32_t diff = remoteAckNum - sentPacket.sequenceNumber;
//                        if (diff > 0 && diff <= 32) { // diff must be > 0 for bit index
//                            uint32_t bitIndex = diff - 1;
//                            if ((remoteAckBits >> bitIndex) & 1U) {
//                                acknowledged = true;
//                                RF_NETWORK_INFO("ACK MATCH: Bitfield ACK for our_sent_seq=%u (diff=%u, bitIndex=%u) by remote_ack_num=%u, remote_ack_bits=0x%08X. Marking for removal.",
//                                    sentPacket.sequenceNumber, diff, bitIndex, remoteAckNum, remoteAckBits);
//                            }
//                            else {
//                                RF_NETWORK_TRACE("ACK CHECK: Bitfield NO match for our_sent_seq=%u (diff=%u, bitIndex=%u). Remote AckBits: 0x%08X, Bit to test: 0x%08X",
//                                    sentPacket.sequenceNumber, diff, bitIndex, remoteAckBits, (1U << bitIndex));
//                            }
//                        }
//                        else {
//                            RF_NETWORK_TRACE("ACK CHECK: our_sent_seq=%u is too old (diff=%u > 32) to be in bitfield of remote_ack_num=%u.",
//                                sentPacket.sequenceNumber, diff, remoteAckNum);
//                        }
//                    }
//                    else {
//                        // sentPacket.sequenceNumber > remoteAckNum. This means the remote is ACKing something older than this packet.
//                        // This packet is definitely not ACKed by this remoteAckNum.
//                        RF_NETWORK_TRACE("ACK CHECK: our_sent_seq=%u > remote_ack_num=%u. Not ACKed by this ack number.",
//                            sentPacket.sequenceNumber, remoteAckNum);
//                    }
//
//                    if (acknowledged) {
//                        RF_NETWORK_INFO("ACK REMOVE: Our sent packet Seq=%u was ACKNOWLEDGED. Removing from unacknowledged list.", sentPacket.sequenceNumber);
//                        actualAckedCountThisPass++;
//                        // TODO: Here you would also update Round Trip Time (RTT) estimations
//                        // e.g., RTT = now() - sentPacket.timeSent;
//                        // connectionState.UpdateRTT(RTT);
//                        return true; // Remove from list
//                    }
//                    return false; // Keep in list
//                }
//            );
//
//            if (actualAckedCountThisPass > 0) {
//                RF_NETWORK_INFO("ACK PROC: Processed %d ACKs. Unacked packets remaining: %zu (was %zu)",
//                    actualAckedCountThisPass, connectionState.unacknowledgedSentPackets.size(), preAckRemovalCount);
//            }
//
//
//            // ... (rest of your ProcessIncomingPacketHeader logic for handling incoming sequence numbers,
//            //      updating connectionState.highestReceivedSequenceNumberFromRemote,
//            //      connectionState.receivedSequenceBitfield, setting hasPendingAckToSend,
//            //      and determining shouldProcessPayload for gameLogicPayload) ...
//            // This part should remain as you had it from prompt 4, ensuring it correctly
//            // handles new incoming reliable packets from the client.
//
//            // Example snippet of how that part continues (ensure it matches your version from prompt 4)
//            bool shouldRelayToGameLogic = false; // Renamed from shouldProcessPayload for clarity
//            bool ackStateForRemoteUpdated = false;
//
//            if (HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
//                uint32_t incomingSeqNum = receivedHeader.sequenceNumber;
//                RF_NETWORK_TRACE("RECV RELIABLE: Received RELIABLE packet from remote with Seq=%u. Our highest_remote_seq=%u, bits=0x%08X",
//                    incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);
//
//                if (incomingSeqNum > connectionState.highestReceivedSequenceNumberFromRemote) {
//                    uint32_t diff = incomingSeqNum - connectionState.highestReceivedSequenceNumberFromRemote;
//                    // Shift bitfield left by diff. If diff is large, effectively clears the bitfield.
//                    if (diff >= 32) {
//                        connectionState.receivedSequenceBitfield = 0;
//                    }
//                    else {
//                        connectionState.receivedSequenceBitfield <<= diff;
//                    }
//                    // Set the bit for the packet just before the new highest (if applicable and within 32)
//                    // This logic correctly ACKs the packet that *was* highest if the new one is exactly 1 greater.
//                    // And also correctly sets bits if there was a gap.
//                    if (connectionState.highestReceivedSequenceNumberFromRemote > 0 && diff > 0 && diff <= 32) {
//                        // This sets bit for highestReceivedSequenceNumberFromRemote which is now incomingSeqNum-diff
//                        // The bit for incomingSeqNum-1 should be set if diff == 1, 
//                        // which is (1U << (1-1)) = (1U << 0)
//                        connectionState.receivedSequenceBitfield |= (1U << (diff - 1));
//                    }
//                    else if (connectionState.highestReceivedSequenceNumberFromRemote == 0 && incomingSeqNum == 0 && diff == 0) {
//                        // Special case: first packet is seq 0. Highest is 0, bitfield remains 0.
//                        // The fact that we received seq 0 is "remembered" by highestReceivedSequenceNumberFromRemote becoming 0.
//                        // The ACK packet will send ackNum=0, ackBits=0. The sender checks if sent_seq == ackNum.
//                    }
//
//
//                    connectionState.highestReceivedSequenceNumberFromRemote = incomingSeqNum;
//                    shouldRelayToGameLogic = true; // This is a new packet
//                    ackStateForRemoteUpdated = true;
//                    RF_NETWORK_INFO("RECV RELIABLE: New highest remote Seq=%u. Our ACK state for them: highest=%u, bits=0x%08X. Will process payload.",
//                        incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);
//                }
//                else if (incomingSeqNum < connectionState.highestReceivedSequenceNumberFromRemote) {
//                    uint32_t diff = connectionState.highestReceivedSequenceNumberFromRemote - incomingSeqNum;
//                    if (diff > 0 && diff <= 32) { // Packet is within the bitfield range
//                        uint32_t bitToTestOrSet = (1U << (diff - 1));
//                        if (!(connectionState.receivedSequenceBitfield & bitToTestOrSet)) { // If bit not already set
//                            connectionState.receivedSequenceBitfield |= bitToTestOrSet; // Set the bit
//                            shouldRelayToGameLogic = true; // Process this out-of-order packet
//                            ackStateForRemoteUpdated = true;
//                            RF_NETWORK_INFO("RECV RELIABLE: Accepted out-of-order remote Seq=%u (diff=%u). Our ACK state for them: highest=%u, bits=0x%08X. Will process payload.",
//                                incomingSeqNum, diff, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);
//                        }
//                        else {
//                            RF_NETWORK_TRACE("RECV RELIABLE: Duplicate reliable remote Seq=%u (already in bitfield). Discarding payload.", incomingSeqNum);
//                            shouldRelayToGameLogic = false; // Already processed
//                        }
//                    }
//                    else { // Packet is too old (older than highest - 32)
//                        RF_NETWORK_TRACE("RECV RELIABLE: Very old reliable remote Seq=%u (older than highest_remote_seq %u - 32). Discarding payload.",
//                            incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote);
//                        shouldRelayToGameLogic = false;
//                    }
//                }
//                else { // incomingSeqNum == connectionState.highestReceivedSequenceNumberFromRemote
//                    // This is a duplicate of the current highest.
//                    // This check assumes seq numbers are > 0 for normal ops, or seq 0 is handled by the '>' path first.
//                    // If highestReceivedSequenceNumberFromRemote is 0 and incomingSeqNum is 0, it would have been caught by ">" if it was the first (diff 0 means no shift in bitfield).
//                    // Thus, if they are equal here, it's a duplicate of something already processed as highest.
//                    RF_NETWORK_TRACE("RECV RELIABLE: Duplicate of highest remote Seq=%u. Discarding payload.", incomingSeqNum);
//                    shouldRelayToGameLogic = false;
//                }
//            }
//            else if (packetPayloadData && packetPayloadLength > 0 && !HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
//                // Unreliable packet with payload
//                RF_NETWORK_TRACE("RECV UNRELIABLE: Received UNRELIABLE packet with payload. Will process payload.");
//                shouldRelayToGameLogic = true;
//            }
//            else {
//                // No payload to process (e.g. ACK-only packet that is not reliable, or reliable packet with no payload that was duplicate/old)
//                RF_NETWORK_TRACE("RECV: Packet from remote has no game logic payload to process (Header MsgType: %s, Flags: 0x%02X).",
//                    EnumNameMessageType(receivedHeader.messageType), receivedHeader.flags);
//            }
//
//            if (ackStateForRemoteUpdated) {
//                connectionState.hasPendingAckToSend = true;
//                RF_NETWORK_TRACE("ACK STATE UPDATE: Marking hasPendingAckToSend=true for remote due to received reliable packet.");
//            }
//
//            if (shouldRelayToGameLogic) {
//                if (packetPayloadData && packetPayloadLength > 0) {
//                    if (out_payloadToProcess) *out_payloadToProcess = packetPayloadData;
//                    if (out_payloadSize) *out_payloadSize = packetPayloadLength;
//                    return true; // Payload should be processed by PacketProcessor
//                }
//                else if (HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
//                    // Reliable packet was marked to process, but no actual payload.
//                    // This can happen if an ACK-only reliable packet is somehow marked for processing (shouldn't)
//                    // or if a new sequence number arrived in a packet that genuinely had no higher-level payload.
//                    // The ACK has been processed, and an ACK will be sent back.
//                    RF_NETWORK_WARN("ProcessIncomingPacketHeader: Reliable packet Seq=%u indicated for payload processing but payload is null/empty. ACK already handled.",
//                        receivedHeader.sequenceNumber);
//                    return false; // No actual game payload to pass up
//                }
//            }
//            return false; // No game payload to process by PacketProcessor
//        }
//
//
//        // --- GetPacketsForRetransmission ---
//        std::vector<std::vector<uint8_t>> GetPacketsForRetransmission(
//            ReliableConnectionState& connectionState,
//            std::chrono::steady_clock::time_point currentTime,
//            float RTO_ms,
//            int maxRetries
//        ) {
//            std::vector<std::vector<uint8_t>> packetsToResend;
//            // RF_NETWORK_TRACE("Checking for retransmissions... Unacked: {}, RTO: {:.2f} ms", 
//            //    connectionState.unacknowledgedSentPackets.size(), RTO_ms);
//
//            auto it = connectionState.unacknowledgedSentPackets.begin();
//            while (it != connectionState.unacknowledgedSentPackets.end()) {
//                ReliableConnectionState::SentPacketInfo& sentPacket = *it;
//                auto timeSinceSent = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - sentPacket.timeSent);
//
//                if (timeSinceSent.count() >= static_cast<long long>(RTO_ms)) {
//                    if (sentPacket.retries < maxRetries) {
//                        sentPacket.retries++;
//                        sentPacket.timeSent = currentTime;
//                        packetsToResend.push_back(sentPacket.packetData);
//                        // Corrected Logging:
//                        // RF_NETWORK_WARN("Retransmitting packet Seq={} (Attempt #{}), RTO_timeout={:.0f}ms, TimeSinceSent={}ms",
//                        //    sentPacket.sequenceNumber, sentPacket.retries, RTO_ms, timeSinceSent.count());
//                        it++;
//                    }
//                    else {
//                        // Corrected Logging:
//                        RF_NETWORK_ERROR("Packet Seq={} EXCEEDED MAX RETRIES ({}). Dropping packet. Consider connection unstable.",
//                            sentPacket.sequenceNumber, maxRetries);
//                        it = connectionState.unacknowledgedSentPackets.erase(it);
//                    }
//                }
//                else {
//                    it++;
//                }
//            }
//            // if (!packetsToResend.empty()) {
//            //    RF_NETWORK_TRACE("Found {} packets to retransmit.", packetsToResend.size());
//            // }
//            return packetsToResend;
//        }
//
//    } // namespace Networking
//} // namespace RiftForged

// File: NetworkEngine/UDPReliabilityProtocol.cpp
// RiftForged Game Engine
// Copyright (C) 2022-2028 RiftForged Team

#include "UDPReliabilityProtocol.h"
#include "../Utils/Logger.h" // For RF_NETWORK_... macros
#include "GamePacketHeader.h" // For GamePacketFlag, MessageType, EnumNameMessageType
#include <cstring>           // For memcpy
#include <vector>            // For std::vector in PrepareOutgoingPacket
#include <list>              // For std::list in ReliableConnectionState
#include <chrono>            // For time points
//
namespace RiftForged {
    namespace Networking {

        // --- PrepareOutgoingPacket ---
        // (Assuming your existing PrepareOutgoingPacket function from prompt 4 is here)
        // Make sure it correctly populates GamePacketHeader, including sequenceNumber if reliable,
        // and ackNumber/ackBitfield from connectionState.
        // And adds to connectionState.unacknowledgedSentPackets if IS_RELIABLE.
        std::vector<uint8_t> PrepareOutgoingPacket(
                        ReliableConnectionState& connectionState,
                        MessageType messageType,
                        const uint8_t* payloadData,
                        uint16_t payloadSize,
                        uint8_t packetFlags
                    ) {
                        if (!HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY) && payloadSize > 0 && payloadData == nullptr) {
                            // Corrected Logging:
                            RF_NETWORK_WARN("PrepareOutgoingPacket: Payload data is null for a non-ACK-only packet with payload size > 0. Type: {}", static_cast<uint16_t>(messageType));
                            return {};
                        }
                        if (HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY) && payloadSize > 0) {
                            // Corrected Logging:
                            RF_NETWORK_WARN("PrepareOutgoingPacket: ACK-only packet should not have a payload. Type: {}, PayloadSize: {}. Ignoring payload.", static_cast<uint16_t>(messageType), payloadSize);
                            payloadSize = 0;
                            payloadData = nullptr;
                        }
            
                        GamePacketHeader header;
                        header.protocolId = CURRENT_PROTOCOL_ID_VERSION;
                        header.messageType = messageType;
                        header.flags = packetFlags;
                        header.ackNumber = connectionState.highestReceivedSequenceNumberFromRemote;
                        header.ackBitfield = connectionState.receivedSequenceBitfield;
            
                        if (HasFlag(packetFlags, GamePacketFlag::IS_RELIABLE)) {
                            header.sequenceNumber = connectionState.nextOutgoingSequenceNumber++;
                            // Corrected Logging (if uncommented):
                            RF_NETWORK_TRACE("Preparing RELIABLE packet Seq: {}, Ack: {}, AckBits: 0x{:08X}, Type: {}", 
                            header.sequenceNumber, header.ackNumber, header.ackBitfield, static_cast<uint16_t>(messageType));
                        }
                        else {
                            header.sequenceNumber = 0;
                            // Corrected Logging (if uncommented):
                            RF_NETWORK_TRACE("Preparing UNRELIABLE packet, Ack: {}, AckBits: 0x{:08X}, Type: {}", 
                            header.ackNumber, header.ackBitfield, static_cast<uint16_t>(messageType));
                        }
            
                        std::vector<uint8_t> packetBuffer(GetGamePacketHeaderSize() + payloadSize);
                        std::memcpy(packetBuffer.data(), &header, GetGamePacketHeaderSize());
                        if (payloadData && payloadSize > 0) {
                            std::memcpy(packetBuffer.data() + GetGamePacketHeaderSize(), payloadData, payloadSize);
                        }
            
                        if (HasFlag(packetFlags, GamePacketFlag::IS_RELIABLE)) {
                            connectionState.unacknowledgedSentPackets.emplace_back(
                                header.sequenceNumber,
                                packetBuffer,
                                HasFlag(packetFlags, GamePacketFlag::IS_ACK_ONLY)
                            );
                            // Corrected Logging (if uncommented):
                            // RF_NETWORK_TRACE("Queued reliable packet Seq: {} for ACK. Unacked count: {}", 
                            //    header.sequenceNumber, connectionState.unacknowledgedSentPackets.size());
                        }
            
                        // This part was in my previous draft but belongs here for PrepareOutgoingPacket
                        connectionState.hasPendingAckToSend = false;
                        connectionState.lastPacketSentTimeToRemote = std::chrono::steady_clock::now();
            
                        return packetBuffer;
                    }

        // --- ProcessIncomingPacketHeader (with enhanced ACK logging from prompt 17) ---
        bool ProcessIncomingPacketHeader(
            ReliableConnectionState& connectionState,
            const GamePacketHeader& receivedHeader,
            const uint8_t* packetPayloadData,
            uint16_t packetPayloadLength,
            const uint8_t** out_payloadToProcess,
            uint16_t* out_payloadSize
        ) {
            if (out_payloadToProcess) *out_payloadToProcess = nullptr;
            if (out_payloadSize) *out_payloadSize = 0;

            uint32_t remoteAckNum = receivedHeader.ackNumber;
            uint32_t remoteAckBits = receivedHeader.ackBitfield;

            if (remoteAckNum > 0 || remoteAckBits > 0 || HasFlag(receivedHeader.flags, GamePacketFlag::IS_ACK_ONLY)) { // Log if it's an ACK or carries ACKs
                RF_NETWORK_TRACE("ACK RECV: Processing ACKs from remote: RemoteAckNum=%u, RemoteAckBits=0x%08X. Our current unacked count: %zu. HeaderFlags=0x%02X",
                    remoteAckNum, remoteAckBits, connectionState.unacknowledgedSentPackets.size(), receivedHeader.flags);
            }

            size_t preAckRemovalCount = connectionState.unacknowledgedSentPackets.size();
            int actualAckedCountThisPass = 0;

            // This lambda processes ACKs for packets *we* sent.
            connectionState.unacknowledgedSentPackets.remove_if(
                [&](const ReliableConnectionState::SentPacketInfo& sentPacket) {
                    RF_NETWORK_TRACE("ACK CHECK: Comparing our_sent_seq=%u (isAckOnly=%s) against remote_ack_num=%u, remote_ack_bits=0x%08X",
                        sentPacket.sequenceNumber, sentPacket.isAckOnly ? "true" : "false", remoteAckNum, remoteAckBits);

                    bool acknowledged = false;
                    if (sentPacket.sequenceNumber == remoteAckNum) { // Packet directly ACKed
                        acknowledged = true;
                        RF_NETWORK_INFO("ACK MATCH: Direct ACK for our_sent_seq=%u by remote_ack_num=%u. Marking for removal.",
                            sentPacket.sequenceNumber, remoteAckNum);
                    }
                    else if (sentPacket.sequenceNumber < remoteAckNum) { // Packet older than directly ACKed one, check bitfield
                        uint32_t diff = remoteAckNum - sentPacket.sequenceNumber;
                        if (diff > 0 && diff <= 32) { // diff must be > 0 for bit index to be valid
                            uint32_t bitIndex = diff - 1; // Bit 0 corresponds to remoteAckNum-1, Bit 1 to remoteAckNum-2, etc.
                            if ((remoteAckBits >> bitIndex) & 1U) {
                                acknowledged = true;
                                RF_NETWORK_INFO("ACK MATCH: Bitfield ACK for our_sent_seq=%u (diff=%u, bitIndex=%u) by remote_ack_num=%u, remote_ack_bits=0x%08X. Marking for removal.",
                                    sentPacket.sequenceNumber, diff, bitIndex, remoteAckNum, remoteAckBits);
                            }
                            else {
                                RF_NETWORK_TRACE("ACK CHECK: Bitfield NO match for our_sent_seq=%u (diff=%u, bitIndex=%u). Remote AckBits: 0x%08X, Bit to test: 0x%08X",
                                    sentPacket.sequenceNumber, diff, bitIndex, remoteAckBits, (1U << bitIndex));
                            }
                        }
                        else {
                            RF_NETWORK_TRACE("ACK CHECK: our_sent_seq=%u is too old (diff=%u > 32) to be in bitfield of remote_ack_num=%u.",
                                sentPacket.sequenceNumber, diff, remoteAckNum);
                        }
                    }
                    else { // sentPacket.sequenceNumber > remoteAckNum
                        RF_NETWORK_TRACE("ACK CHECK: our_sent_seq=%u > remote_ack_num=%u. Not ACKed by this ack number.",
                            sentPacket.sequenceNumber, remoteAckNum);
                    }

                    if (acknowledged) {
                        // This log was in your original, keeping it:
                        RF_NETWORK_TRACE("Our sent packet Seq=%u was ACKNOWLEDGED by remote. Removing.", sentPacket.sequenceNumber);
                        actualAckedCountThisPass++;
                        // TODO: Update RTT estimations here
                        // e.g., auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - sentPacket.timeSent);
                        // connectionState.RecordRTT(rtt.count());
                        return true; // Remove from list
                    }
                    return false; // Keep in list
                }
            );

            if (actualAckedCountThisPass > 0) { // Logged this way in your original
                RF_NETWORK_TRACE("Processed {} ACKs. Unacked packets remaining: {} (was {})",
                    actualAckedCountThisPass, connectionState.unacknowledgedSentPackets.size(), preAckRemovalCount);
            }
            else if (preAckRemovalCount > 0 && (remoteAckNum > 0 || remoteAckBits > 0)) {
                RF_NETWORK_TRACE("ACK PROC: No packets ACKed this pass. RemoteAckNum=%u, RemoteAckBits=0x%08X. Unacked count remains %zu.",
                    remoteAckNum, remoteAckBits, connectionState.unacknowledgedSentPackets.size());
            }

            // --- Processing of the incoming packet's sequence number (if it's reliable) ---
            // This part updates what WE will ACK back to the sender.
            bool shouldRelayToGameLogic = false;
            bool ackStateForRemoteUpdated = false; // Tracks if we received new reliable info from remote

            if (HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
                uint32_t incomingSeqNum = receivedHeader.sequenceNumber;
                RF_NETWORK_TRACE("RECV RELIABLE: Incoming reliable packet Seq=%u. Our highest_remote_seq=%u, our_ack_bits_for_them=0x%08X",
                    incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);

                if (incomingSeqNum > connectionState.highestReceivedSequenceNumberFromRemote) {
                    uint32_t diff = incomingSeqNum - connectionState.highestReceivedSequenceNumberFromRemote;
                    if (diff >= 32) {
                        connectionState.receivedSequenceBitfield = 0; // Too large a jump, previous ACKs lost from bitfield view
                    }
                    else {
                        connectionState.receivedSequenceBitfield <<= diff;
                    }
                    // Set the bit corresponding to the *old* highestReceivedSequenceNumberFromRemote
                    // This only applies if the old highest was within the new bitfield's range.
                    if (diff > 0 && diff <= 32 && connectionState.highestReceivedSequenceNumberFromRemote > 0) {
                        // If old highest was seq 5, new is seq 6 (diff 1), bit (1-1)=0 is set for seq 5.
                        // If old highest was seq 5, new is seq 7 (diff 2), bit (2-1)=1 is set for seq 5.
                        connectionState.receivedSequenceBitfield |= (1U << (diff - 1));
                    }
                    else if (connectionState.highestReceivedSequenceNumberFromRemote == 0 && incomingSeqNum == 0 && diff == 0) {
                        // This is the very first packet (seq 0) received.
                        // highestReceivedSequenceNumberFromRemote remains 0. Bitfield remains 0.
                        // Our ACK back will be ackNum=0, ackBits=0.
                    }


                    connectionState.highestReceivedSequenceNumberFromRemote = incomingSeqNum;
                    shouldRelayToGameLogic = true; // This is a new packet, process its payload
                    ackStateForRemoteUpdated = true;
                    RF_NETWORK_INFO("RECV RELIABLE: New highest remote Seq=%u. Our ACK state FOR THEM: highest_ack_to_send=%u, bits_to_send=0x%08X. Will process payload.",
                        incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);
                }
                else if (incomingSeqNum < connectionState.highestReceivedSequenceNumberFromRemote) {
                    uint32_t diff = connectionState.highestReceivedSequenceNumberFromRemote - incomingSeqNum;
                    if (diff > 0 && diff <= 32) { // Packet is within the bitfield range
                        uint32_t bitToTestOrSet = (1U << (diff - 1));
                        if (!(connectionState.receivedSequenceBitfield & bitToTestOrSet)) { // If bit not already set for this old packet
                            connectionState.receivedSequenceBitfield |= bitToTestOrSet; // Set the bit
                            shouldRelayToGameLogic = true; // Process this (new to us) out-of-order packet
                            ackStateForRemoteUpdated = true;
                            RF_NETWORK_INFO("RECV RELIABLE: Accepted out-of-order remote Seq=%u (diff=%u). Our ACK state FOR THEM: highest_ack_to_send=%u, bits_to_send=0x%08X. Will process payload.",
                                incomingSeqNum, diff, connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);
                        }
                        else { // Bit already set, it's a duplicate of an old packet we've processed
                            RF_NETWORK_TRACE("RECV RELIABLE: Duplicate OLD reliable remote Seq=%u (already in bitfield). Discarding payload.", incomingSeqNum);
                            shouldRelayToGameLogic = false;
                        }
                    }
                    else { // Packet is too old (older than highest - 32)
                        RF_NETWORK_TRACE("RECV RELIABLE: Very OLD reliable remote Seq=%u (older than highest_remote_seq %u - 32). Discarding payload.",
                            incomingSeqNum, connectionState.highestReceivedSequenceNumberFromRemote);
                        shouldRelayToGameLogic = false;
                    }
                }
                else { // incomingSeqNum == connectionState.highestReceivedSequenceNumberFromRemote
                    // This is a duplicate of the current highest *if highest is not 0*.
                    // If highest IS 0, and incoming is 0, it was handled by the '>' case (diff=0).
                    // So if they are equal here, AND highestReceivedSequenceNumberFromRemote is not 0, it's a duplicate.
                    // Or if highest IS 0, but it was NOT the first seq 0 packet (meaning bitfield might have info for -1, -2... which is not possible here)
                    // The logic in your original code for this case (from prompt 4) seemed a bit complex.
                    // Simplified: if it's equal to current highest, and we didn't just process it as new highest, it's a duplicate.
                    RF_NETWORK_TRACE("RECV RELIABLE: Duplicate of current highest remote Seq=%u. Discarding payload.", incomingSeqNum);
                    shouldRelayToGameLogic = false;
                }
            }
            else if (packetPayloadData && packetPayloadLength > 0 && !HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
                // Unreliable packet with payload
                RF_NETWORK_TRACE("RECV UNRELIABLE: Received UNRELIABLE packet with payload. Type: %s. Will process payload.", EnumNameMessageType(receivedHeader.messageType));
                shouldRelayToGameLogic = true;
            }
            else if (HasFlag(receivedHeader.flags, GamePacketFlag::IS_ACK_ONLY) && !HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
                // Purely ACK, not reliable itself. We've processed its ACK info. No payload.
                RF_NETWORK_TRACE("RECV ACK_ONLY (unreliable): Processed ACKs. No payload to relay. Header MsgType: %s", EnumNameMessageType(receivedHeader.messageType));
                shouldRelayToGameLogic = false;
            }
            else {
                RF_NETWORK_TRACE("RECV: Packet has no game logic payload to process (e.g. pure reliable ACK, or unreliable empty). Header MsgType: %s, Flags: 0x%02X.", EnumNameMessageType(receivedHeader.messageType), receivedHeader.flags);
                shouldRelayToGameLogic = false;
            }

            if (ackStateForRemoteUpdated) { // If we received a new reliable packet that updated our ACK state for them
                connectionState.hasPendingAckToSend = true;
                RF_NETWORK_TRACE("ACK STATE UPDATE: Marking hasPendingAckToSend=true for remote (because we received new reliable data Seq=%u).", receivedHeader.sequenceNumber);
            }

            if (shouldRelayToGameLogic) {
                // This check is important: only provide payload if it actually exists.
                if (packetPayloadData && packetPayloadLength > 0) {
                    if (out_payloadToProcess) *out_payloadToProcess = packetPayloadData;
                    if (out_payloadSize) *out_payloadSize = packetPayloadLength;
                    RF_NETWORK_TRACE("PAYLOAD TO PROCESS: Yes, Type: %s, Size: %u", EnumNameMessageType(receivedHeader.messageType), packetPayloadLength);
                    return true; // Payload should be processed by PacketProcessor
                }
                else if (HasFlag(receivedHeader.flags, GamePacketFlag::IS_RELIABLE)) {
                    // A reliable packet was new/valid (e.g. incremented sequence), but it had no application payload.
                    // This is fine, its ACK and sequence number processing are done.
                    RF_NETWORK_TRACE("PAYLOAD TO PROCESS: No (Reliable packet Seq=%u was new/valid but had no application payload. ACK processing done.)", receivedHeader.sequenceNumber);
                    return false;
                }
                else {
                    // Unreliable packet marked to process but no payload.
                    RF_NETWORK_TRACE("PAYLOAD TO PROCESS: No (Packet was to be processed but has no payload. Type: %s, Flags: 0x%02X)", EnumNameMessageType(receivedHeader.messageType), receivedHeader.flags);
                    return false;
                }
            }

            RF_NETWORK_TRACE("PAYLOAD TO PROCESS: No (End of function decision)");
            return false; // No game payload to process by PacketProcessor
        }

        // --- GetPacketsForRetransmission ---
        // (Assuming your existing GetPacketsForRetransmission function from prompt 4 is here)
        std::vector<std::vector<uint8_t>> GetPacketsForRetransmission(
            ReliableConnectionState& connectionState,
            std::chrono::steady_clock::time_point currentTime,
            float RTO_ms,
            int maxRetries
        ) {
            // Your existing implementation from prompt 4
            std::vector<std::vector<uint8_t>> packetsToResend;
            auto it = connectionState.unacknowledgedSentPackets.begin();
            while (it != connectionState.unacknowledgedSentPackets.end()) {
                ReliableConnectionState::SentPacketInfo& sentPacket = *it;
                auto timeSinceSent = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - sentPacket.timeSent);

                if (timeSinceSent.count() >= static_cast<long long>(RTO_ms)) {
                    if (sentPacket.retries < maxRetries) {
                        sentPacket.retries++;
                        sentPacket.timeSent = currentTime; // Update timeSent for the next RTO calculation
                        packetsToResend.push_back(sentPacket.packetData); // Resend the original packet data
                        RF_NETWORK_WARN("RETRANSMIT: Packet Seq=%u (Attempt #%d). RTO: %.0fms, TimeSinceLastSent: %lldms",
                            sentPacket.sequenceNumber, sentPacket.retries, RTO_ms, timeSinceSent.count());
                        it++;
                    }
                    else {
                        RF_NETWORK_ERROR("MAX RETRIES: Packet Seq=%u EXCEEDED MAX RETRIES (%d). Dropping packet. Consider connection unstable.",
                            sentPacket.sequenceNumber, maxRetries);
                        // TODO: Notify GameServerEngine about this connection being potentially dead/unstable.
                        // GameServerEngine might then decide to tear down the ReliableConnectionState and Player session.
                        it = connectionState.unacknowledgedSentPackets.erase(it); // Remove from list
                    }
                }
                else {
                    it++;
                }
            }
            if (!packetsToResend.empty()) {
                RF_NETWORK_TRACE("RETRANSMIT: Found %zu packets to retransmit this cycle.", packetsToResend.size());
            }
            return packetsToResend;
        }

    } // namespace Networking
} // namespace RiftForged