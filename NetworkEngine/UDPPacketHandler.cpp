// File: UDPPacketHandler.cpp
// RiftForged Game Development
// Purpose: Implementation of the UDPPacketHandler class. Handles UDP packet-level logic,
//          including custom reliability by calling functions from UDPReliabilityProtocol,
//          and bridges the Network IO layer to the application-level MessageHandler.

#include "UDPPacketHandler.h"
#include "INetworkIO.h"            // For calling m_networkIO->SendData()
#include "IMessageHandler.h"       // For calling m_messageHandler->ProcessApplicationMessage()
#include "OverlappedIOContext.h"   // For the type passed in OnRawDataReceived, OnSendCompleted
#include "NetworkCommon.h"         // For S2C_Response structure (now using FB S2C payload type)
#include "UDPReliabilityProtocol.h" // For the free functions and ReliableConnectionState, GamePacketFlag

// Include FlatBuffers generated headers to access payload enums and verify functions
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S_UDP_Payload
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_UDP_Payload

#include "../GameServer/GameServerEngine.h" // For m_gameServerEngine.OnClientDisconnected and GetAllActiveSessionEndpoints
#include "../Gameplay/ActivePlayer.h"       // For RiftForged::GameLogic::ActivePlayer
#include "../Utils/Logger.h"        // Assuming your logger utility

#include <utility>      // For std::move
#include <algorithm>    // For std::remove_if, std::find_if
#include <stdexcept>    // For std::invalid_argument

// Constants are defined in UDPPacketHandler.h or UDPReliabilityProtocol.h


namespace RiftForged {
    namespace Networking {

        // --- Constructor & Destructor ---

        UDPPacketHandler::UDPPacketHandler(INetworkIO* networkIO,
            IMessageHandler* messageHandler,
            RiftForged::Server::GameServerEngine& gameServerEngine)
            : m_networkIO(networkIO),
            m_messageHandler(messageHandler),
            m_gameServerEngine(gameServerEngine),
            m_isRunning(false) {
            if (!m_networkIO) {
                RF_NETWORK_CRITICAL("UDPPacketHandler: INetworkIO dependency is null!");
                throw std::invalid_argument("INetworkIO cannot be null in UDPPacketHandler constructor");
            }
            if (!m_messageHandler) {
                RF_NETWORK_CRITICAL("UDPPacketHandler: IMessageHandler dependency is null!");
                throw std::invalid_argument("IMessageHandler cannot be null in UDPPacketHandler constructor");
            }
            RF_NETWORK_INFO("UDPPacketHandler: Instance created.");
        }

        UDPPacketHandler::~UDPPacketHandler() {
            RF_NETWORK_INFO("UDPPacketHandler: Destructor called. Ensuring Stop().");
            Stop();
        }

        // --- Public Control Methods ---

        bool UDPPacketHandler::Start() {
            if (m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN("UDPPacketHandler: Already running.");
                return true;
            }
            RF_NETWORK_INFO("UDPPacketHandler: Starting...");
            m_isRunning.store(true, std::memory_order_release);

            try {
                m_reliabilityThread = std::thread(&UDPPacketHandler::ReliabilityManagementThread, this);
                RF_NETWORK_INFO("UDPPacketHandler: Reliability management thread created and started.");
            }
            catch (const std::system_error& e) {
                RF_NETWORK_CRITICAL("UDPPacketHandler: Failed to create reliability management thread: {}", e.what());
                m_isRunning.store(false, std::memory_order_relaxed);
                return false;
            }
            return true;
        }

        void UDPPacketHandler::Stop() {
            if (!m_isRunning.exchange(false, std::memory_order_acq_rel)) {
                RF_NETWORK_DEBUG("UDPPacketHandler: Stop called but already not running or stop initiated.");
                return;
            }
            RF_NETWORK_INFO("UDPPacketHandler: Stopping reliability management thread...");
            if (m_reliabilityThread.joinable()) {
                m_reliabilityThread.join();
                RF_NETWORK_INFO("UDPPacketHandler: Reliability management thread joined.");
            }

            // Clean up reliability states upon stop
            {
                std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                m_reliabilityStates.clear();
                m_endpointLastSeenTime.clear();
            }
            RF_NETWORK_INFO("UDPPacketHandler: Reliability states and last seen times cleared.");
            RF_NETWORK_INFO("UDPPacketHandler: Stopped.");
        }

        // --- INetworkIOEvents Implementation ---

        void UDPPacketHandler::OnRawDataReceived(const NetworkEndpoint& sender,
            const uint8_t* data,
            uint32_t size,
            OverlappedIOContext* context) {
            // 'context' is from UDPSocketAsync's pool, we don't manage its lifecycle here.
            RF_NETWORK_TRACE("UDPPacketHandler: OnRawDataReceived from %s ({} bytes)", sender.ToString(), size);

            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN("UDPPacketHandler: Received data but handler is not running. Ignoring from %s.", sender.ToString());
                return;
            }

            if (size < GetGamePacketHeaderSize()) {
                RF_NETWORK_WARN("UDPPacketHandler: Received packet too small ({} bytes) from %s. Discarding.", size, sender.ToString());
                return;
            }

            GamePacketHeader receivedHeader;
            // It's crucial that GamePacketHeader is packed correctly for this memcpy.
            memcpy(&receivedHeader, data, GetGamePacketHeaderSize());

            RF_NETWORK_TRACE("UDPPacketHandler: Raw Header from %s - Proto: 0x%X, Seq: %u, Ack: %u, AckBits: 0x%08X, Flags: 0x%X",
                sender.ToString(), receivedHeader.protocolId,
                receivedHeader.sequenceNumber,
                receivedHeader.ackNumber, receivedHeader.ackBitfield, receivedHeader.flags);

            if (receivedHeader.protocolId != CURRENT_PROTOCOL_ID_VERSION) {
                RF_NETWORK_WARN("UDPPacketHandler: Received packet from %s with mismatched protocol ID (Expected: 0x%X, Got: 0x%X). Discarding.",
                    sender.ToString(), CURRENT_PROTOCOL_ID_VERSION, receivedHeader.protocolId);
                return;
            }

            // Update last seen time for this endpoint, used for stale connection detection
            {
                std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                m_endpointLastSeenTime[sender] = std::chrono::steady_clock::now();
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(sender);
            if (!connState) {
                RF_NETWORK_ERROR("UDPPacketHandler: Failed to get/create reliability state for %s. Discarding packet.", sender.ToString());
                return;
            }

            const uint8_t* payloadAfterGameHeader = data + GetGamePacketHeaderSize();
            uint16_t payloadAfterGameHeaderSize = static_cast<uint16_t>(size - GetGamePacketHeaderSize());

            const uint8_t* appPayloadToProcess = nullptr;
            uint16_t appPayloadSize = 0;

            // Call the free function from UDPReliabilityProtocol.cpp to process reliability aspects.
            // This function now determines if the packet's payload should be passed to the application layer.
            bool shouldRelayToGameLogic = RiftForged::Networking::ProcessIncomingPacketHeader(
                *connState,
                receivedHeader,
                payloadAfterGameHeader,
                payloadAfterGameHeaderSize,
                &appPayloadToProcess,
                &appPayloadSize
            );

            if (shouldRelayToGameLogic) {
                // Check if a FlatBuffer payload is actually present and valid for processing.
                if (appPayloadToProcess && appPayloadSize > 0) {
                    RF_NETWORK_TRACE("UDPPacketHandler: Relaying app payload from %s to MessageHandler. Size: %u bytes.",
                        sender.ToString(), appPayloadSize);

                    // --- IMPORTANT: Get the ActivePlayer associated with this sender endpoint ---
                    // This is the main addition to this function.
                    RiftForged::GameLogic::ActivePlayer* player = nullptr; // Initialize to nullptr

                    // Pass FlatBuffer payload directly to the message handler
                    std::optional<S2C_Response> s2c_response_opt = m_messageHandler->ProcessApplicationMessage(
                        sender,
                        appPayloadToProcess,
                        appPayloadSize,
                        player // Pass the retrieved ActivePlayer pointer
                    );

                    if (s2c_response_opt.has_value()) {
                        HandleResponseMessage(s2c_response_opt);
                    }
                }
                else {
                    // This case should ideally not happen if ProcessIncomingPacketHeader is perfectly aligned,
                    // as it should only return true if there's a valid app payload.
                    RF_NETWORK_WARN("UDPPacketHandler: ProcessIncomingPacketHeader returned true, but no app payload was provided. This might indicate an issue.");
                }
            }
            else {
                RF_NETWORK_TRACE("UDPPacketHandler: Packet from %s not relayed (e.g., duplicate, pure ACK, or invalid for application processing).",
                    sender.ToString());
            }
        }

        void UDPPacketHandler::OnSendCompleted(OverlappedIOContext* context,
            bool success,
            uint32_t bytesSent) {
            if (success) {
                RF_NETWORK_TRACE("UDPPacketHandler: NetworkIO reported send of {} bytes completed successfully. Context: {}", bytesSent, (void*)context);
            }
            else {
                RF_NETWORK_WARN("UDPPacketHandler: NetworkIO reported send operation failed. Context: {}", (void*)context);
            }
            // Note: The sendContext (pIoContext in UDPSocketAsync::WorkerThread for Send) is deleted by UDPSocketAsync
            // after this callback.
        }

        void UDPPacketHandler::OnNetworkError(const std::string& errorMessage, int errorCode) {
            RF_NETWORK_ERROR("UDPPacketHandler: Received OnNetworkError from NetworkIO: \"%s\" (Code: %d)", errorMessage.c_str(), errorCode);
        }


        // --- Public Sending Interface ---

        bool UDPPacketHandler::SendReliablePacket(const NetworkEndpoint& recipient,
            UDP::S2C::S2C_UDP_Payload flatbufferPayloadType,
            const flatbuffers::DetachedBuffer& flatbufferPayload,
            uint8_t additionalFlags) {
            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN("UDPPacketHandler: SendReliablePacket called but handler is not running. Dropping packet to %s.", recipient.ToString());
                return false;
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(recipient);
            if (!connState) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendReliablePacket - Failed to get/create reliability state for %s. Dropping packet.", recipient.ToString());
                return false;
            }

            uint8_t flags = static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | additionalFlags;

            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                *connState,
                flatbufferPayload.data(),
                static_cast<uint16_t>(flatbufferPayload.size()),
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendReliablePacket - PrepareOutgoingPacket returned empty for FB type %s to %s.",
                    UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), recipient.ToString());
                return false;
            }

            RF_NETWORK_TRACE("UDPPacketHandler: Sending RELIABLE FB Type %s (%u bytes total) to %s.",
                UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), packetBuffer.size(), recipient.ToString());

            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        bool UDPPacketHandler::SendUnreliablePacket(const NetworkEndpoint& recipient,
            UDP::S2C::S2C_UDP_Payload flatbufferPayloadType,
            const flatbuffers::DetachedBuffer& flatbufferPayload,
            uint8_t additionalFlags) {
            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN("UDPPacketHandler: SendUnreliablePacket called but handler is not running. Dropping packet to %s.", recipient.ToString());
                return false;
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(recipient);
            if (!connState) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendUnreliablePacket - Failed to get/create reliability state for %s. Dropping packet (needed for ACK info).", recipient.ToString());
                return false;
            }

            uint8_t flags = additionalFlags & (~static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE)); // Ensure not marked reliable

            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                *connState,
                flatbufferPayload.data(),
                static_cast<uint16_t>(flatbufferPayload.size()),
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendUnreliablePacket - PrepareOutgoingPacket returned empty for FB Type %s to %s.",
                    UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), recipient.ToString());
                return false;
            }

            RF_NETWORK_TRACE("UDPPacketHandler: Sending UNRELIABLE FB Type %s (%u bytes total) to %s.",
                UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), packetBuffer.size(), recipient.ToString());

            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        bool UDPPacketHandler::SendAckPacket(const NetworkEndpoint& recipient, ReliableConnectionState& connectionState) {
            if (!m_isRunning.load(std::memory_order_acquire)) return false;

            RF_NETWORK_TRACE("UDPPacketHandler: Sending explicit ACK-only packet to %s. Current RemoteHighestSeq: %u, Current RemoteAckBits: 0x%08X",
                recipient.ToString(), connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);

            uint8_t flags = static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(GamePacketFlag::IS_ACK_ONLY);

            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                connectionState,
                nullptr, 0, // No application payload for a pure ACK
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendAckPacket - PrepareOutgoingPacket returned empty for ACK to %s.", recipient.ToString());
                return false;
            }
            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        // --- Internal Helper for Handling Responses ---
        void UDPPacketHandler::HandleResponseMessage(const std::optional<S2C_Response>& responseOpt) {
            if (!responseOpt.has_value()) {
                return;
            }
            const S2C_Response& response = responseOpt.value();

            // Log using the FlatBuffers S2C payload type
            RF_NETWORK_DEBUG("UDPPacketHandler: Handling S2C_Response. Broadcast: %s, Recipient: [%s], MsgType: %s",
                response.broadcast ? "true" : "false",
                response.specific_recipient.ToString(),
                UDP::S2C::EnumNameS2C_UDP_Payload(response.flatbuffer_payload_type));

            // Use the data and type from the S2C_Response object
            const flatbuffers::DetachedBuffer& payloadData = response.data;
            UDP::S2C::S2C_UDP_Payload payloadType = response.flatbuffer_payload_type;


            if (response.broadcast) {
                std::vector<NetworkEndpoint> all_clients = m_gameServerEngine.GetAllActiveSessionEndpoints();
                RF_NETWORK_INFO("UDPPacketHandler: Broadcasting S2C_Response MsgType %s to %zu clients.",
                    UDP::S2C::EnumNameS2C_UDP_Payload(payloadType), all_clients.size());
                for (const auto& client_ep : all_clients) {
                    if (client_ep.ipAddress.empty() || client_ep.port == 0) continue;
                    // Assume reliable for broadcast game messages for now.
                    // If some broadcasts can be unreliable, add logic here.
                    SendReliablePacket(client_ep, payloadType, payloadData);
                }
            }
            else {
                NetworkEndpoint targetRecipient = response.specific_recipient;
                if (!targetRecipient.ipAddress.empty() && targetRecipient.port != 0) {
                    // Assuming reliable for direct responses to clients.
                    // If some direct responses can be unreliable, add logic here.
                    SendReliablePacket(targetRecipient, payloadType, payloadData);
                }
                else {
                    RF_NETWORK_ERROR("UDPPacketHandler: S2C_Response - Invalid target recipient for MsgType %s. Cannot send.",
                        UDP::S2C::EnumNameS2C_UDP_Payload(payloadType));
                }
            }
        }

        // --- Private Reliability Protocol Methods ---

        std::shared_ptr<ReliableConnectionState> UDPPacketHandler::GetOrCreateReliabilityState(const NetworkEndpoint& endpoint) {
            std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
            auto it = m_reliabilityStates.find(endpoint);
            if (it != m_reliabilityStates.end()) {
                return it->second;
            }
            else {
                RF_NETWORK_INFO("UDPPacketHandler: Creating new ReliableConnectionState for endpoint: %s", endpoint.ToString());
                auto newState = std::make_shared<ReliableConnectionState>();
                m_reliabilityStates[endpoint] = newState;
                m_endpointLastSeenTime[endpoint] = std::chrono::steady_clock::now();
                return newState;
            }
        }

        void UDPPacketHandler::ReliabilityManagementThread() {
            RF_NETWORK_INFO("UDPPacketHandler: ReliabilityManagementThread started.");
            std::vector<NetworkEndpoint> clientsToNotifyDropped;

            while (m_isRunning.load(std::memory_order_acquire)) {
                auto currentTime = std::chrono::steady_clock::now();
                clientsToNotifyDropped.clear();

                // List to store packets that need retransmission, along with their target endpoint.
                std::vector<std::pair<NetworkEndpoint, std::vector<uint8_t>>> packetsToResendList;
                // List to store endpoints for which an explicit ACK needs to be sent.
                std::vector<NetworkEndpoint> endpointsNeedingExplicitAck;

                // Scope for m_reliabilityStatesMutex to safely iterate and modify connection states.
                {
                    std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);

                    // Iterate through all active client connections.
                    for (auto it = m_reliabilityStates.begin(); it != m_reliabilityStates.end(); /* manual increment/erase */) {
                        const NetworkEndpoint& endpoint = it->first;
                        std::shared_ptr<ReliableConnectionState> state = it->second;

                        // 1. Check for retransmissions.
                        // Call the free function from UDPReliabilityProtocol to get timed-out packets.
                        std::vector<std::vector<uint8_t>> retransmitPacketsForThisEndpoint =
                            RiftForged::Networking::GetPacketsForRetransmission(*state, currentTime);

                        for (const auto& pktData : retransmitPacketsForThisEndpoint) {
                            packetsToResendList.emplace_back(endpoint, pktData);
                        }

                        // Check if the connection needs to be dropped due to excessive retries
                        bool dropClientThisPass = state->connectionDroppedByMaxRetries;
                        if (dropClientThisPass) {
                            RF_NETWORK_WARN("UDPPacketHandler: Endpoint %s flagged for drop by MAX RETRIES.", endpoint.ToString());
                            clientsToNotifyDropped.push_back(endpoint);
                        }

                        // 2. Check for stale connections (only if not already being dropped by retries).
                        if (!dropClientThisPass) {
                            // Check if no packets have been received from the remote for a long time
                            // AND we are not actively waiting for an ACK from them (unacknowledgedSentPackets is empty).
                            if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - state->lastPacketReceivedTimeFromRemote).count() > STALE_CONNECTION_TIMEOUT_SECONDS_PKT &&
                                state->unacknowledgedSentPackets.empty()) {
                                RF_NETWORK_INFO("UDPPacketHandler: Endpoint %s flagged for drop due to STALENESS (no recent packets from them, nothing pending from us).", endpoint.ToString());
                                clientsToNotifyDropped.push_back(endpoint);
                                dropClientThisPass = true;
                            }
                        }

                        // 3. Check for pending explicit ACKs.
                        // Only send explicit ACK if there's one pending and we haven't recently sent a data packet
                        // that would piggyback the ACK.
                        if (!dropClientThisPass && state->hasPendingAckToSend) {
                            // The TrySendAckOnlyPacket helper function in UDPReliabilityProtocol will handle
                            // the delay logic for sending the ACK, so we just need to provide the send function.
                            endpointsNeedingExplicitAck.push_back(endpoint);
                        }

                        // Handle connection removal if flagged for drop.
                        if (dropClientThisPass) {
                            m_endpointLastSeenTime.erase(endpoint); // Clean up last seen time map
                            it = m_reliabilityStates.erase(it);      // Erase from reliability states map and get next iterator
                        }
                        else {
                            ++it; // Move to the next connection if not erased.
                        }
                    }
                } // Mutex m_reliabilityStatesMutex is released here.

                // --- Perform network sends outside the main state lock to prevent deadlocks ---
                // Retransmit packets.
                for (const auto& pair : packetsToResendList) {
                    RF_NETWORK_WARN("UDPPacketHandler: Retransmitting packet ({} bytes) to %s.", pair.second.size(), pair.first.ToString());
                    m_networkIO->SendData(pair.first, pair.second.data(), static_cast<uint32_t>(pair.second.size()));
                }

                // Send explicit ACK-only packets.
                for (const auto& endpoint : endpointsNeedingExplicitAck) {
                    std::shared_ptr<ReliableConnectionState> state;
                    // Re-acquire lock to safely get shared_ptr (it might have been removed if it was also a "drop" client)
                    {
                        std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                        auto it = m_reliabilityStates.find(endpoint);
                        if (it != m_reliabilityStates.end()) {
                            state = it->second;
                        }
                    }
                    if (state) { // Only send if the connection state still exists (i.e., not dropped this cycle)
                        // Using a lambda to pass the send function to TrySendAckOnlyPacket
                        RiftForged::Networking::TrySendAckOnlyPacket(
                            *state,
                            currentTime,
                            [this, &endpoint](const std::vector<uint8_t>& packetData) {
                                // This lambda is executed on the reliability thread, but calls into m_networkIO (thread-safe).
                                RF_NETWORK_TRACE("UDPPacketHandler: Sending explicit ACK-only packet to %s (via TrySendAckOnlyPacket).", endpoint.ToString());
                                m_networkIO->SendData(endpoint, packetData.data(), static_cast<uint32_t>(packetData.size()));
                            }
                        );
                    }
                }

                // Notify GameServerEngine about dropped clients (outside the lock).
                if (!clientsToNotifyDropped.empty()) {
                    RF_NETWORK_INFO("UDPPacketHandler: Notifying GameServerEngine about %zu client(s) dropped.", clientsToNotifyDropped.size());
                    for (const auto& droppedEndpoint : clientsToNotifyDropped) {
                        m_gameServerEngine.OnClientDisconnected(droppedEndpoint);
                    }
                }

                // Sleep to control the frequency of this management thread.
                std::this_thread::sleep_for(std::chrono::milliseconds(RELIABILITY_THREAD_SLEEP_MS_PKT));
            }
            RF_NETWORK_INFO("UDPPacketHandler: ReliabilityManagementThread gracefully exited.");
        }

    } // namespace Networking
} // namespace RiftForged