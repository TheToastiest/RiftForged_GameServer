// File: UDPPacketHandler.cpp
// RiftForged Game Development
// Purpose: Implementation of the UDPPacketHandler class. Handles UDP packet-level logic,
//          including custom reliability by calling functions from UDPReliabilityProtocol,
//          and bridges the Network IO layer to the application-level MessageHandler.

#include "UDPPacketHandler.h"
#include "INetworkIO.h"           // For calling m_networkIO->SendData()
#include "IMessageHandler.h"      // For calling m_messageHandler->ProcessApplicationMessage() (PacketProcessor)
#include "OverlappedIOContext.h"  // For the type passed in OnRawDataReceived, OnSendCompleted
#include "NetworkCommon.h"        // For S2C_Response structure
#include "UDPReliabilityProtocol.h" // For the free functions and ReliableConnectionState, GamePacketFlag, GamePacketHeader

// Include FlatBuffers generated headers to access payload enums and verify functions
#include "../FlatBuffers/V0.0.4/riftforged_c2s_udp_messages_generated.h" // For C2S_UDP_Payload and root message
#include "../FlatBuffers/V0.0.4/riftforged_s2c_udp_messages_generated.h" // For S2C_UDP_Payload (used in HandleResponseMessage)

#include "../GameServer/GameServerEngine.h" // For m_gameServerEngine (session management, PlayerManager access)
#include "../Gameplay/PlayerManager.h"     // For getting ActivePlayer via GameServerEngine
#include "../Gameplay/ActivePlayer.h"      // For RiftForged::GameLogic::ActivePlayer
#include "../Utils/Logger.h"          // For RF_NETWORK_... macros

#include <utility>     // For std::move
#include <algorithm>   // For std::remove_if, std::find_if
#include <stdexcept>   // For std::invalid_argument
#include <fmt/core.h>  // For FMT_STRING - ensure this is available

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
                // Note: Logger might not be initialized if this throws super early,
                // but critical errors should attempt to log.
                RF_NETWORK_CRITICAL(FMT_STRING("UDPPacketHandler: INetworkIO dependency is null!"));
                throw std::invalid_argument("INetworkIO cannot be null in UDPPacketHandler constructor");
            }
            if (!m_messageHandler) {
                RF_NETWORK_CRITICAL(FMT_STRING("UDPPacketHandler: IMessageHandler dependency is null!"));
                throw std::invalid_argument("IMessageHandler cannot be null in UDPPacketHandler constructor");
            }
            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Instance created."));
        }

        UDPPacketHandler::~UDPPacketHandler() {
            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Destructor called. Ensuring Stop()."));
            Stop();
        }

        // --- Public Control Methods ---

        bool UDPPacketHandler::Start() {
            if (m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Already running."));
                return true;
            }
            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Starting..."));
            m_isRunning.store(true, std::memory_order_release);

            try {
                m_reliabilityThread = std::thread(&UDPPacketHandler::ReliabilityManagementThread, this);
                RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Reliability management thread created and started."));
            }
            catch (const std::system_error& e) {
                RF_NETWORK_CRITICAL(FMT_STRING("UDPPacketHandler: Failed to create reliability management thread: {}"), e.what());
                m_isRunning.store(false, std::memory_order_relaxed);
                return false;
            }
            return true;
        }

        void UDPPacketHandler::Stop() {
            bool alreadyStoppingOrStopped = !m_isRunning.exchange(false, std::memory_order_acq_rel);
            if (alreadyStoppingOrStopped) {
                RF_NETWORK_DEBUG(FMT_STRING("UDPPacketHandler: Stop called but already not running or stop initiated."));
                // If thread was created but start failed, or if stop was called multiple times,
                // we still might want to try joining if joinable.
                if (m_reliabilityThread.joinable() && m_reliabilityThread.get_id() != std::this_thread::get_id()) {
                    RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Attempting to join reliability thread on redundant stop call..."));
                    m_reliabilityThread.join();
                    RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Reliability thread joined on redundant stop call."));
                }
                return;
            }

            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Stopping reliability management thread..."));
            if (m_reliabilityThread.joinable()) {
                // Ensure the thread is not trying to join itself if Stop() is called from the thread
                if (m_reliabilityThread.get_id() == std::this_thread::get_id()) {
                    RF_NETWORK_CRITICAL(FMT_STRING("UDPPacketHandler::Stop() called from reliability thread itself! Cannot join."));
                }
                else {
                    m_reliabilityThread.join();
                    RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Reliability management thread joined."));
                }
            }
            else {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Reliability thread was not joinable upon stop."));
            }


            // Clean up reliability states upon stop
            {
                std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                m_reliabilityStates.clear();
                m_endpointLastSeenTime.clear();
            }
            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Reliability states and last seen times cleared."));
            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Stopped."));
        }

        // --- INetworkIOEvents Implementation ---

        void UDPPacketHandler::OnRawDataReceived(const NetworkEndpoint& sender,
            const uint8_t* data,
            uint32_t size,
            OverlappedIOContext* context) {
            RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: OnRawDataReceived from {} ({} bytes)"), sender.ToString(), size);

            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Received data but handler is not running. Ignoring from {}."), sender.ToString());
                return;
            }

            if (size < GetGamePacketHeaderSize()) {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Received packet too small ({} bytes) from {}. Discarding."), size, sender.ToString());
                return;
            }

            GamePacketHeader receivedHeader;
            memcpy(&receivedHeader, data, GetGamePacketHeaderSize());

            RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Raw Header from {} - Proto: 0x{:X}, Seq: {}, Ack: {}, AckBits: 0x{:08X}, Flags: 0x{:X}"),
                sender.ToString(), receivedHeader.protocolId,
                receivedHeader.sequenceNumber,
                receivedHeader.ackNumber, receivedHeader.ackBitfield, receivedHeader.flags);

            if (receivedHeader.protocolId != CURRENT_PROTOCOL_ID_VERSION) {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Received packet from {} with mismatched protocol ID (Expected: 0x{:X}, Got: 0x{:X}). Discarding."),
                    sender.ToString(), CURRENT_PROTOCOL_ID_VERSION, receivedHeader.protocolId);
                return;
            }

            {
                std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                m_endpointLastSeenTime[sender] = std::chrono::steady_clock::now();
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(sender);
            if (!connState) {
                RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: Failed to get/create reliability state for {}. Discarding packet."), sender.ToString());
                return;
            }

            const uint8_t* payloadAfterGameHeader = data + GetGamePacketHeaderSize();
            uint16_t payloadAfterGameHeaderSize = static_cast<uint16_t>(size - GetGamePacketHeaderSize());

            const uint8_t* appPayloadToProcess = nullptr;
            uint16_t appPayloadSize = 0;

            bool shouldRelayToGameLogic = RiftForged::Networking::ProcessIncomingPacketHeader(
                *connState,
                receivedHeader,
                payloadAfterGameHeader,
                payloadAfterGameHeaderSize,
                &appPayloadToProcess,
                &appPayloadSize
            );

            if (shouldRelayToGameLogic) {
                if (appPayloadToProcess && appPayloadSize > 0) {
                    RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Relaying app payload from {} to MessageHandler. Size: {} bytes."),
                        sender.ToString(), appPayloadSize);

                    RiftForged::GameLogic::ActivePlayer* player = nullptr;
                    UDP::C2S::C2S_UDP_Payload c2s_payload_type = UDP::C2S::C2S_UDP_Payload_NONE;

                    if (appPayloadToProcess && appPayloadSize >= (sizeof(uint32_t) * 2)) { // Min size for a FB root table
                        flatbuffers::Verifier verifier(appPayloadToProcess, appPayloadSize);
                        if (UDP::C2S::VerifyRoot_C2S_UDP_MessageBuffer(verifier)) {
                            const UDP::C2S::Root_C2S_UDP_Message* root_c2s_msg = UDP::C2S::GetRoot_C2S_UDP_Message(appPayloadToProcess);
                            if (root_c2s_msg && root_c2s_msg->payload()) {
                                c2s_payload_type = root_c2s_msg->payload_type();
                            }
                            else {
                                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Valid FlatBuffer root from {} but no payload field present or root_c2s_msg is null."), sender.ToString());
                                // If no payload type, treat as if player is not needed for dispatch to PacketProcessor,
                                // which will then handle it based on its internal logic (likely drop if not JoinRequest)
                            }
                        }
                        else {
                            RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: FlatBuffer verification failed for payload from {}. Not processing for player lookup."), sender.ToString());
                            return; // Invalid FB, don't pass to message handler
                        }
                    }
                    else if (appPayloadToProcess && appPayloadSize > 0) {
                        RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Payload from {} too small to be a valid FlatBuffer (size {}). Not processing for player lookup."), sender.ToString(), appPayloadSize);
                        return; // Too small, don't pass to message handler
                    }


                    if (c2s_payload_type != UDP::C2S::C2S_UDP_Payload_JoinRequest) {
                        uint64_t playerId = m_gameServerEngine.GetPlayerIdForEndpoint(sender);
                        RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: For endpoint {}, GameServerEngine returned PlayerID {}. (MsgType: {})"),
                            sender.ToString(), playerId, UDP::C2S::EnumNameC2S_UDP_Payload(c2s_payload_type));
                        if (playerId != 0) {
                            player = m_gameServerEngine.GetPlayerManager().FindPlayerById(playerId);
                            if (!player) {
                                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Endpoint {} has PlayerID {} but ActivePlayer object not found. Dropping msg type {}."),
                                    sender.ToString(), playerId, UDP::C2S::EnumNameC2S_UDP_Payload(c2s_payload_type));
                                // PacketProcessor will also drop it if player is null and it's not JoinRequest,
                                // but logging here helps identify where the ActivePlayer* was lost.
                            }
                            else {
                                RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Found ActivePlayer (ID: {}) for endpoint {} for message type {}."),
                                    player->playerId, sender.ToString(), UDP::C2S::EnumNameC2S_UDP_Payload(c2s_payload_type));
                            }
                        }
                        else {
                            RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: No PlayerID found for endpoint {} for message type {}. Passing nullptr player to PacketProcessor."),
                                sender.ToString(), UDP::C2S::EnumNameC2S_UDP_Payload(c2s_payload_type));
                        }
                    }
                    else {
                        RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Message from {} is C2S_JoinRequest. Player context will be nullptr for PacketProcessor."), sender.ToString());
                    }

                    std::optional<S2C_Response> s2c_response_opt = m_messageHandler->ProcessApplicationMessage(
                        sender,
                        appPayloadToProcess,
                        appPayloadSize,
                        player
                    );

                    if (s2c_response_opt.has_value()) {
                        HandleResponseMessage(s2c_response_opt);
                    }
                }
                else {
                    RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: ProcessIncomingPacketHeader indicated relay, but no app payload provided from {}. Header Flags: 0x{:X}"),
                        sender.ToString(), receivedHeader.flags);
                }
            }
            else {
                RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Packet from {} not relayed by reliability protocol (e.g., duplicate, pure ACK). Header Flags: 0x{:X}"),
                    sender.ToString(), receivedHeader.flags);
            }
        }

        void UDPPacketHandler::OnSendCompleted(OverlappedIOContext* context,
            bool success,
            uint32_t bytesSent) {
            if (success) {
                RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: NetworkIO reported send of {} bytes completed successfully. Context: {}"), bytesSent, static_cast<void*>(context));
            }
            else {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: NetworkIO reported send operation failed. Context: {}"), static_cast<void*>(context));
            }
        }

        void UDPPacketHandler::OnNetworkError(const std::string& errorMessage, int errorCode) {
            RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: Received OnNetworkError from NetworkIO: \"{}\" (Code: {})"), errorMessage, errorCode);
        }

        // --- Public Sending Interface ---

        bool UDPPacketHandler::SendReliablePacket(const NetworkEndpoint& recipient,
            UDP::S2C::S2C_UDP_Payload flatbufferPayloadType,
            const flatbuffers::DetachedBuffer& flatbufferPayload,
            uint8_t additionalFlags) {
            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: SendReliablePacket called but handler is not running. Dropping packet to {}."), recipient.ToString());
                return false;
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(recipient);
            if (!connState) {
                RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: SendReliablePacket - Failed to get/create reliability state for {}. Dropping packet."), recipient.ToString());
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
                RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: SendReliablePacket - PrepareOutgoingPacket returned empty for FB type {} to {}."),
                    UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), recipient.ToString());
                return false;
            }

            RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Sending RELIABLE FB Type {} ({} bytes total) to {}."),
                UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), packetBuffer.size(), recipient.ToString());

            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        bool UDPPacketHandler::SendUnreliablePacket(const NetworkEndpoint& recipient,
            UDP::S2C::S2C_UDP_Payload flatbufferPayloadType,
            const flatbuffers::DetachedBuffer& flatbufferPayload,
            uint8_t additionalFlags) {
            if (!m_isRunning.load(std::memory_order_acquire)) {
                RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: SendUnreliablePacket called but handler is not running. Dropping packet to {}."), recipient.ToString());
                return false;
            }

            std::shared_ptr<ReliableConnectionState> connState = GetOrCreateReliabilityState(recipient);
            if (!connState) {
                // Unreliable packets still need connState for current ACK info to send.
                RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: SendUnreliablePacket - Failed to get/create reliability state for {}. Dropping packet."), recipient.ToString());
                return false;
            }

            uint8_t flags = additionalFlags & (~static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE));
            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                *connState,
                flatbufferPayload.data(),
                static_cast<uint16_t>(flatbufferPayload.size()),
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: SendUnreliablePacket - PrepareOutgoingPacket returned empty for FB Type {} to {}."),
                    UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), recipient.ToString());
                return false;
            }

            RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Sending UNRELIABLE FB Type {} ({} bytes total) to {}."),
                UDP::S2C::EnumNameS2C_UDP_Payload(flatbufferPayloadType), packetBuffer.size(), recipient.ToString());

            return m_networkIO->SendData(recipient, packetBuffer.data(), static_cast<uint32_t>(packetBuffer.size()));
        }

        bool UDPPacketHandler::SendAckPacket(const NetworkEndpoint& recipient, ReliableConnectionState& connectionState) {
            if (!m_isRunning.load(std::memory_order_acquire)) return false;

            RF_NETWORK_TRACE(FMT_STRING("UDPPacketHandler: Sending explicit ACK-only packet to {}. Current RemoteHighestSeq: {}, Current RemoteAckBits: 0x{:08X}"),
                recipient.ToString(), connectionState.highestReceivedSequenceNumberFromRemote, connectionState.receivedSequenceBitfield);

            uint8_t flags = static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(GamePacketFlag::IS_ACK_ONLY);
            std::vector<uint8_t> packetBuffer = RiftForged::Networking::PrepareOutgoingPacket(
                connectionState,
                nullptr, 0,
                flags
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: SendAckPacket - PrepareOutgoingPacket returned empty for ACK to {}."), recipient.ToString());
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

            RF_NETWORK_DEBUG(FMT_STRING("UDPPacketHandler: Handling S2C_Response. Broadcast: {}, Recipient: [{}], MsgType: {}"),
                response.broadcast ? "true" : "false",
                response.specific_recipient.ToString(), // Assuming S2C_Response recipient is not optional if not broadcast
                UDP::S2C::EnumNameS2C_UDP_Payload(response.flatbuffer_payload_type));

            const flatbuffers::DetachedBuffer& payloadData = response.data;
            UDP::S2C::S2C_UDP_Payload payloadType = response.flatbuffer_payload_type;

            if (response.broadcast) {
                std::vector<NetworkEndpoint> all_clients = m_gameServerEngine.GetAllActiveSessionEndpoints();
                RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Broadcasting S2C_Response MsgType {} to {} clients."),
                    UDP::S2C::EnumNameS2C_UDP_Payload(payloadType), all_clients.size());
                for (const auto& client_ep : all_clients) {
                    if (client_ep.ipAddress.empty() || client_ep.port == 0) continue;
                    // Assuming reliable for most broadcast game messages. Adjust flags if needed.
                    SendReliablePacket(client_ep, payloadType, payloadData);
                }
            }
            else {
                NetworkEndpoint targetRecipient = response.specific_recipient;
                if (!targetRecipient.ipAddress.empty() && targetRecipient.port != 0) {
                    // Assuming reliable for direct responses to clients. Adjust flags if needed.
                    SendReliablePacket(targetRecipient, payloadType, payloadData);
                }
                else {
                    RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: S2C_Response - Invalid target recipient for MsgType {}. Cannot send."),
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
                RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Creating new ReliableConnectionState for endpoint: {}."), endpoint.ToString());
                try {
                    auto newState = std::make_shared<ReliableConnectionState>();
                    m_reliabilityStates[endpoint] = newState;
                    m_endpointLastSeenTime[endpoint] = std::chrono::steady_clock::now(); // Initialize last seen time
                    return newState;
                }
                catch (const std::bad_alloc& e) {
                    RF_NETWORK_CRITICAL(FMT_STRING("UDPPacketHandler: Failed to allocate ReliableConnectionState for {}: {}"), endpoint.ToString(), e.what());
                    return nullptr;
                }
            }
        }

        void UDPPacketHandler::ReliabilityManagementThread() {
            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: ReliabilityManagementThread started."));
            std::vector<NetworkEndpoint> clientsToNotifyDropped;

            while (m_isRunning.load(std::memory_order_acquire)) {
                auto currentTime = std::chrono::steady_clock::now();
                clientsToNotifyDropped.clear();

                std::vector<std::pair<NetworkEndpoint, std::vector<uint8_t>>> packetsToResendList;
                std::vector<NetworkEndpoint> endpointsNeedingExplicitAck;

                {
                    std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                    for (auto it = m_reliabilityStates.begin(); it != m_reliabilityStates.end(); /* manual increment/erase */) {
                        const NetworkEndpoint& endpoint = it->first;
                        std::shared_ptr<ReliableConnectionState> state = it->second;
                        bool dropClientThisPass = false;

                        // Must check if state is valid, though make_shared should succeed or throw.
                        if (!state) {
                            RF_NETWORK_ERROR(FMT_STRING("UDPPacketHandler: Null ReliableConnectionState found in map for endpoint {}. Removing entry."), endpoint.ToString());
                            m_endpointLastSeenTime.erase(endpoint);
                            it = m_reliabilityStates.erase(it);
                            continue;
                        }

                        // 1. Check for retransmissions
                        std::vector<std::vector<uint8_t>> retransmitsForEndpoint =
                            RiftForged::Networking::GetPacketsForRetransmission(*state, currentTime);
                        for (const auto& pktData : retransmitsForEndpoint) {
                            packetsToResendList.emplace_back(endpoint, pktData);
                        }
                        if (state->connectionDroppedByMaxRetries) {
                            RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Endpoint {} flagged for drop by MAX RETRIES."), endpoint.ToString());
                            dropClientThisPass = true;
                        }

                        // 2. Check for stale connections
                        if (!dropClientThisPass) {
                            if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - state->lastPacketReceivedTimeFromRemote).count() > STALE_CONNECTION_TIMEOUT_SECONDS_PKT &&
                                state->unacknowledgedSentPackets.empty()) { // Only if we are not waiting for their ACKs
                                RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Endpoint {} flagged for drop due to STALENESS."), endpoint.ToString());
                                dropClientThisPass = true;
                            }
                        }

                        // 3. Check for pending explicit ACKs
                        if (!dropClientThisPass && state->hasPendingAckToSend) {
                            endpointsNeedingExplicitAck.push_back(endpoint);
                        }

                        if (dropClientThisPass) {
                            clientsToNotifyDropped.push_back(endpoint);
                            m_endpointLastSeenTime.erase(endpoint);
                            it = m_reliabilityStates.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                } // Mutex m_reliabilityStatesMutex released

                // Perform network sends outside the main state lock
                for (const auto& pair : packetsToResendList) {
                    RF_NETWORK_WARN(FMT_STRING("UDPPacketHandler: Retransmitting packet ({} bytes) to {}."), pair.second.size(), pair.first.ToString());
                    m_networkIO->SendData(pair.first, pair.second.data(), static_cast<uint32_t>(pair.second.size()));
                }

                for (const auto& endpoint : endpointsNeedingExplicitAck) {
                    std::shared_ptr<ReliableConnectionState> state;
                    {
                        std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                        auto it_find = m_reliabilityStates.find(endpoint);
                        if (it_find != m_reliabilityStates.end()) {
                            state = it_find->second;
                        }
                    }
                    if (state) {
                        RiftForged::Networking::TrySendAckOnlyPacket(
                            *state,
                            currentTime,
                            [this, &endpoint](const std::vector<uint8_t>& packetData) {
                                // This lambda is called by TrySendAckOnlyPacket, which itself already holds the
                                // ReliableConnectionState's internal mutex when calling PrepareOutgoingPacketUnlocked.
                                // The actual SendData call is thread-safe.
                                m_networkIO->SendData(endpoint, packetData.data(), static_cast<uint32_t>(packetData.size()));
                            }
                        );
                    }
                }

                if (!clientsToNotifyDropped.empty()) {
                    RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: Notifying GameServerEngine about {} client(s) dropped."), clientsToNotifyDropped.size());
                    for (const auto& droppedEndpoint : clientsToNotifyDropped) {
                        m_gameServerEngine.OnClientDisconnected(droppedEndpoint);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(RELIABILITY_THREAD_SLEEP_MS_PKT));
            }
            RF_NETWORK_INFO(FMT_STRING("UDPPacketHandler: ReliabilityManagementThread gracefully exited."));
        }

    } // namespace Networking
} // namespace RiftForged