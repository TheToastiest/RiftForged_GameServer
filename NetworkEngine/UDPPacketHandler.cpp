// File: UDPPacketHandler.cpp
// RiftForged Game Development
// Purpose: Implementation of the UDPPacketHandler class. Handles UDP packet-level logic,
//          including custom reliability by calling functions from UDPReliabilityProtocol,
//          and bridges the Network IO layer to the application-level MessageHandler.

#include "UDPPacketHandler.h"
#include "INetworkIO.h"       // For calling m_networkIO->SendData()
#include "IMessageHandler.h"  // For calling m_messageHandler->ProcessApplicationMessage()
#include "OverlappedIOContext.h" // For the type passed in OnRawDataReceived, OnSendCompleted
#include "NetworkCommon.h"      // For S2C_Response structure
#include "UDPReliabilityProtocol.h" // For the free functions and ReliableConnectionState, GamePacketFlag
#include "../Gameplay/PlayerManager.h" // Included from UDPPacketHandler.h
#include "../Utils/Logger.h"    // Assuming your logger utility
#include "../GameServer/GameServerEngine.h"

#include <utility> // For std::move
#include <algorithm> // For std::remove_if, std::find_if

// Constants are defined in UDPPacketHandler.h or UDPReliabilityProtocol.h

namespace RiftForged {
    namespace Networking {

        // --- Constructor & Destructor ---

        UDPPacketHandler::UDPPacketHandler(INetworkIO* networkIO,
            IMessageHandler* messageHandler,
            RiftForged::Server::GameServerEngine& gameServerEngine
            )
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
            if (m_isRunning.load(std::memory_order_acquire)) { // Acquire for visibility
                RF_NETWORK_WARN("UDPPacketHandler: Already running.");
                return true;
            }
            RF_NETWORK_INFO("UDPPacketHandler: Starting...");
            m_isRunning.store(true, std::memory_order_release); // Release for visibility

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
            RF_NETWORK_INFO("UDPPacketHandler: Stopped.");
        }

        // --- INetworkIOEvents Implementation ---

        void UDPPacketHandler::OnRawDataReceived(const NetworkEndpoint& sender,
            const uint8_t* data,
            uint32_t size,
            OverlappedIOContext* context) {
            // 'context' is from UDPSocketAsync's pool, we don't manage its lifecycle here.
            RF_NETWORK_TRACE("UDPPacketHandler: OnRawDataReceived from {}:{} ({} bytes)", sender.ipAddress, sender.port, size);

            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN("UDPPacketHandler: Received data but handler is not running. Ignoring from {}.", sender.ToString());
                return;
            }

            if (size < GetGamePacketHeaderSize()) {
                RF_NETWORK_WARN("UDPPacketHandler: Received packet too small ({} bytes) from {}. Discarding.", size, sender.ToString());
                return;
            }

            GamePacketHeader receivedHeader;
            // It's crucial that GamePacketHeader is packed correctly for this memcpy.
            // Your #pragma pack(push, 1) in GamePacketHeader.h handles this.
            memcpy(&receivedHeader, data, GetGamePacketHeaderSize());

            RF_NETWORK_TRACE("UDPPacketHandler: Raw Header from {} - Proto: 0x{:X}, Type: {}, Seq: {}, Ack: {}, AckBits: 0x{:X}, Flags: 0x{:X}",
                sender.ToString(), receivedHeader.protocolId,
                EnumNameMessageType(receivedHeader.messageType),
                receivedHeader.sequenceNumber,
                receivedHeader.ackNumber, receivedHeader.ackBitfield, receivedHeader.flags);

            if (receivedHeader.protocolId != CURRENT_PROTOCOL_ID_VERSION) {
                RF_NETWORK_WARN("UDPPacketHandler: Received packet from {} with mismatched protocol ID (Expected: 0x{:X}, Got: 0x{:X}). Discarding.",
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
                RF_NETWORK_ERROR("UDPPacketHandler: Failed to get/create reliability state for {}. Discarding packet.", sender.ToString());
                return;
            }
            // No explicit lock on connState->internalStateMutex here, as the free functions will handle it.

            const uint8_t* payloadAfterGameHeader = data + GetGamePacketHeaderSize();
            uint16_t payloadAfterGameHeaderSize = static_cast<uint16_t>(size - GetGamePacketHeaderSize());

            const uint8_t* appPayloadToProcess = nullptr;
            uint16_t appPayloadSize = 0;

            // Call the free function from UDPReliabilityProtocol.cpp
            bool shouldRelayToGameLogic = RiftForged::Networking::ProcessIncomingPacketHeader(
                *connState, // Pass the ReliableConnectionState object
                receivedHeader,
                payloadAfterGameHeader,
                payloadAfterGameHeaderSize,
                &appPayloadToProcess, // Output parameter for start of FlatBuffer
                &appPayloadSize       // Output parameter for size of FlatBuffer
            );

            if (shouldRelayToGameLogic) {
                if (appPayloadToProcess && appPayloadSize > 0) {
                    RF_NETWORK_TRACE("UDPPacketHandler: Relaying app payload (Type: {}) from {} to MessageHandler. Size: {}",
                        EnumNameMessageType(receivedHeader.messageType), sender.ToString(), appPayloadSize);

                    std::optional<S2C_Response> s2c_response_opt = m_messageHandler->ProcessApplicationMessage(
                        sender,
                        receivedHeader.messageType,
                        appPayloadToProcess,
                        appPayloadSize
                    );

                    if (s2c_response_opt.has_value()) {
                        HandleResponseMessage(s2c_response_opt, sender);
                    }
                }
                else if (receivedHeader.messageType != MessageType::Unknown && !HasFlag(receivedHeader.flags, GamePacketFlag::IS_ACK_ONLY)) {
                    RF_NETWORK_WARN("UDPPacketHandler: Packet from {} (Type: {}) expected app payload for MessageHandler but was empty. AppSize: {}. Flags: {}",
                        sender.ToString(), EnumNameMessageType(receivedHeader.messageType), appPayloadSize, receivedHeader.flags);
                }
            }
            else {
                RF_NETWORK_TRACE("UDPPacketHandler: Packet from {} (Type: {}) not relayed (e.g., duplicate/pure ACK handled by reliability).",
                    sender.ToString(), EnumNameMessageType(receivedHeader.messageType));
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
            RF_NETWORK_ERROR("UDPPacketHandler: Received OnNetworkError from NetworkIO: \"{}\" (Code: {})", errorMessage, errorCode);
        }


        // --- Public Sending Interface ---

        bool UDPPacketHandler::SendReliablePacket(const NetworkEndpoint& recipient,
            MessageType messageId,
            const std::vector<uint8_t>& flatbufferPayload,
            uint8_t additionalFlags) {
            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN("UDPPacketHandler: SendReliablePacket called but handler is not running. Dropping packet to {}.", recipient.ToString());
                return false;
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(recipient);
            if (!connState) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendReliablePacket - Failed to get/create reliability state for {}. Dropping packet.", recipient.ToString());
                return false;
            }
            // No explicit lock on connState->internalStateMutex here, PrepareOutgoingPacket will handle it.

            uint8_t flags = static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | additionalFlags;

            // Call the free function from UDPReliabilityProtocol.cpp
            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                *connState, // Pass the ReliableConnectionState object
                messageId,
                flatbufferPayload.data(),
                static_cast<uint16_t>(flatbufferPayload.size()),
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendReliablePacket - PrepareOutgoingPacket returned empty for MsgType {} to {}.",
                    EnumNameMessageType(messageId), recipient.ToString());
                return false;
            }

            // The sequence number is already part of the header constructed by PrepareOutgoingPacket
            // We can log it if needed by peeking into the buffer, but connState->nextOutgoingSequenceNumber was already incremented.
            // To get the actual sequence used, you'd deserialize the header from packetBuffer or have PrepareOutgoingPacket return it.
            // For simplicity, logging before sending:
            RF_NETWORK_TRACE("UDPPacketHandler: Sending RELIABLE MsgType {} ({} bytes total) to {}.",
                EnumNameMessageType(messageId), packetBuffer.size(), recipient.ToString());

            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        bool UDPPacketHandler::SendUnreliablePacket(const NetworkEndpoint& recipient,
            MessageType messageId,
            const std::vector<uint8_t>& flatbufferPayload,
            uint8_t additionalFlags) {
            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN("UDPPacketHandler: SendUnreliablePacket called but handler is not running. Dropping packet to {}.", recipient.ToString());
                return false;
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(recipient);
            if (!connState) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendUnreliablePacket - Failed to get/create reliability state for {}. Dropping packet (needed for ACK info).", recipient.ToString());
                return false;
            }
            // No explicit lock on connState->internalStateMutex here, PrepareOutgoingPacket will handle it.

            uint8_t flags = additionalFlags & (~static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE)); // Ensure not marked reliable

            // Call the free function from UDPReliabilityProtocol.cpp
            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                *connState, // Pass state to include its latest ack info
                messageId,
                flatbufferPayload.data(),
                static_cast<uint16_t>(flatbufferPayload.size()),
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendUnreliablePacket - PrepareOutgoingPacket returned empty for MsgType {} to {}.",
                    EnumNameMessageType(messageId), recipient.ToString());
                return false;
            }

            RF_NETWORK_TRACE("UDPPacketHandler: Sending UNRELIABLE MsgType {} ({} bytes total) to {}",
                EnumNameMessageType(messageId), packetBuffer.size(), recipient.ToString());

            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        bool UDPPacketHandler::SendAckPacket(const NetworkEndpoint& recipient, ReliableConnectionState& connectionState) {
            if (!m_isRunning.load(std::memory_order_acquire)) return false;
            // No explicit lock on connectionState.internalStateMutex here, PrepareOutgoingPacket will handle it.

            RF_NETWORK_TRACE("UDPPacketHandler: Sending explicit ACK-only packet to {}. Current RemoteHighestSeq: {}, Current RemoteAckBits: 0x{:08X}",
                recipient.ToString(), connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);

            uint8_t flags = static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(GamePacketFlag::IS_ACK_ONLY);

            // Call the free function from UDPReliabilityProtocol.cpp
            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                connectionState,
                MessageType::Unknown, // No app payload for a pure ACK
                nullptr, 0,
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR("UDPPacketHandler: SendAckPacket - PrepareOutgoingPacket returned empty for ACK to {}.", recipient.ToString());
                return false;
            }
            // PrepareOutgoingPacket now handles setting hasPendingAckToSend = false and lastPacketSentTimeToRemote
            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        // --- Internal Helper for Handling Responses ---
        void UDPPacketHandler::HandleResponseMessage(const std::optional<S2C_Response>& responseOpt, const NetworkEndpoint& originalSender) {
            if (!responseOpt.has_value()) {
                return;
            }
            const S2C_Response& response = responseOpt.value();

            RF_NETWORK_DEBUG("UDPPacketHandler: Handling S2C_Response. Broadcast: {}, Recipient: [{}], Original Sender: [{}], MsgType: {}",
                response.broadcast,
                response.specific_recipient.ToString(),
                originalSender.ToString(), EnumNameMessageType(response.messageType));

            // Get data from DetachedBuffer for sending
            const uint8_t* responsePayloadData = response.data.data();
            uint16_t responsePayloadSize = static_cast<uint16_t>(response.data.size());
            // Create a temporary vector for the Send*Packet methods which expect std::vector
            std::vector<uint8_t> payloadVec(responsePayloadData, responsePayloadData + responsePayloadSize);


            if (response.broadcast) {
                std::vector<NetworkEndpoint> all_clients = m_gameServerEngine.GetAllActiveSessionEndpoints();
                // <<< MODIFICATION END >>>
                RF_NETWORK_INFO("UDPPacketHandler: Broadcasting S2C_Response MsgType {} to {} clients.", EnumNameMessageType(response.messageType), all_clients.size());
                for (const auto& client_ep : all_clients) {
                    if (client_ep.ipAddress.empty() || client_ep.port == 0) continue;
                    SendReliablePacket(client_ep, response.messageType, payloadVec); // Assuming reliable for broadcast responses
                }
            }
            else {
                NetworkEndpoint targetRecipient = response.specific_recipient;
                if (targetRecipient.ipAddress.empty() || targetRecipient.port == 0) {
                    targetRecipient = originalSender; // Default to replying to original sender
                }

                if (!targetRecipient.ipAddress.empty() && targetRecipient.port != 0) {
                    SendReliablePacket(targetRecipient, response.messageType, payloadVec);
                }
                else {
                    RF_NETWORK_ERROR("UDPPacketHandler: S2C_Response - Invalid target recipient and original sender info missing for MsgType {}.",
                        EnumNameMessageType(response.messageType));
                }
            }
        }

        // --- Private Reliability Protocol Methods ---

        std::shared_ptr<ReliableConnectionState> UDPPacketHandler::GetOrCreateReliabilityState(const NetworkEndpoint& endpoint) {
            std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex); // Protects the maps
            auto it = m_reliabilityStates.find(endpoint);
            if (it != m_reliabilityStates.end()) {
                return it->second;
            }
            else {
                RF_NETWORK_INFO("UDPPacketHandler: Creating new ReliableConnectionState for endpoint: {}", endpoint.ToString());
                auto newState = std::make_shared<ReliableConnectionState>(); // Uses default constructor from UDPReliabilityProtocol.h
                // newState->endpoint = endpoint; // If ReliableConnectionState stores its own endpoint for logging/debug
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

                std::vector<std::pair<NetworkEndpoint, std::vector<uint8_t>>> packetsToResendList;
                std::vector<NetworkEndpoint> endpointsNeedingExplicitAck;

                // Scope for m_reliabilityStatesMutex
                {
                    std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);

                    for (auto it = m_reliabilityStates.begin(); it != m_reliabilityStates.end(); /* manual inc/erase */) {
                        const NetworkEndpoint& endpoint = it->first;
                        std::shared_ptr<ReliableConnectionState> state = it->second;
                        // No explicit lock on state->internalStateMutex here, free functions handle it.

                        // 1. Check for retransmissions by calling the free function
                        std::vector<std::vector<uint8_t>> retransmitPacketsForThisEndpoint =
                            RiftForged::Networking::GetPacketsForRetransmission(*state, currentTime, DEFAULT_MAX_RETRIES_PKT);

                        for (const auto& pktData : retransmitPacketsForThisEndpoint) {
                            packetsToResendList.emplace_back(endpoint, pktData);
                        }

                        bool dropClientThisPass = false;
                        if (state->connectionDroppedByMaxRetries) { // This flag is set by GetPacketsForRetransmission
                            RF_NETWORK_WARN("UDPPacketHandler: Endpoint {} flagged for drop by MAX RETRIES.", endpoint.ToString());
                            clientsToNotifyDropped.push_back(endpoint);
                            dropClientThisPass = true;
                        }

                        // 2. Check for stale connections (only if not already being dropped)
                        if (!dropClientThisPass) {
                            auto lastSeenIt = m_endpointLastSeenTime.find(endpoint); // m_endpointLastSeenTime is updated in OnRawDataReceived
                            // Also consider state->lastPacketReceivedTimeFromRemote for more precise staleness from their side

                            bool isStale = false;
                            // If we haven't heard from them in a while AND we are not actively trying to send them anything important.
                            if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - state->lastPacketReceivedTimeFromRemote).count() > STALE_CONNECTION_TIMEOUT_SECONDS_PKT &&
                                state->unacknowledgedSentPackets.empty()) {
                                isStale = true;
                            }


                            if (isStale) {
                                RF_NETWORK_INFO("UDPPacketHandler: Endpoint {} flagged for drop due to STALENESS (no recent packets from them, nothing pending from us).", endpoint.ToString());
                                clientsToNotifyDropped.push_back(endpoint);
                                dropClientThisPass = true;
                            }
                        }

                        // 3. Check for pending explicit ACKs
                        bool justRetransmittedToThisClient = false;
                        for (const auto& pair : packetsToResendList) { if (pair.first == endpoint) { justRetransmittedToThisClient = true; break; } }

                        if (!dropClientThisPass && state->hasPendingAckToSend && !justRetransmittedToThisClient) {
                            auto timeSinceLastSentByUs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                currentTime - state->lastPacketSentTimeToRemote);
                            // Send explicit ACK if it's been a while (e.g., > 1.5 * reliability tick, to give other traffic a chance)
                            if (state->lastPacketSentTimeToRemote == std::chrono::steady_clock::time_point::min() || // Never sent anything
                                timeSinceLastSentByUs.count() > (RELIABILITY_THREAD_SLEEP_MS_PKT + (RELIABILITY_THREAD_SLEEP_MS_PKT / 2))) {
                                endpointsNeedingExplicitAck.push_back(endpoint);
                            }
                        }

                        if (dropClientThisPass) {
                            m_endpointLastSeenTime.erase(endpoint); // Clean up this map too
                            it = m_reliabilityStates.erase(it);    // Erase and get next valid iterator
                        }
                        else {
                            ++it; // Advance iterator
                        }
                    }
                } // Mutex m_reliabilityStatesMutex is released here

                // Perform network sends (retransmissions, ACKs) outside the main state lock
                for (const auto& pair : packetsToResendList) {
                    // The packetData from GetPacketsForRetransmission should ideally have updated ACK fields.
                    // Your GetPacketsForRetransmission current logic just returns sentPacket.packetData.
                    // For maximum correctness, it should reconstruct the header with current ACK info.
                    // Assuming for now it returns data ready for send.
                    RF_NETWORK_WARN("UDPPacketHandler: Retransmitting packet ({} bytes) to {}.", pair.second.size(), pair.first.ToString());
                    m_networkIO->SendData(pair.first, pair.second.data(), static_cast<uint32_t>(pair.second.size()));
                }

                for (const auto& endpoint : endpointsNeedingExplicitAck) {
                    std::shared_ptr<ReliableConnectionState> state; // Re-fetch state briefly as it might have been dropped
                    {
                        std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                        auto it = m_reliabilityStates.find(endpoint);
                        if (it != m_reliabilityStates.end()) {
                            state = it->second;
                        }
                    }
                    if (state) { // If state still exists
                        SendAckPacket(endpoint, *state); // SendAckPacket calls PrepareOutgoingPacket which handles state->hasPendingAckToSend
                    }
                }

                // Notify PlayerManager about dropped clients (outside the lock)
                if (!clientsToNotifyDropped.empty()) {
                    RF_NETWORK_INFO("UDPPacketHandler: Notifying PlayerManager about {} client(s) dropped.", clientsToNotifyDropped.size());
                    for (const auto& droppedEndpoint : clientsToNotifyDropped) {
                        m_gameServerEngine.OnClientDisconnected(droppedEndpoint);
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(RELIABILITY_THREAD_SLEEP_MS_PKT));
            }
            RF_NETWORK_INFO("UDPPacketHandler: ReliabilityManagementThread gracefully exited.");
        }

        // The following private methods are now implemented as free functions in UDPReliabilityProtocol.cpp
        // This class calls those free functions.
        // - ProcessIncomingReliabilityHeader (called by OnRawDataReceived, calls free function)
        // - PrepareOutgoingPacketBuffer (called by SendReliable/Unreliable/Ack, calls free function)
        // - GetPacketsForRetransmission (called by ReliabilityManagementThread, calls free function)

    } // namespace Networking
} // namespace RiftForged