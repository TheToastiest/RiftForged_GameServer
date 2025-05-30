// File: UDPSocketAsync.cpp
// RiftForged Game Development Team
// Copyright (c) 2025-2028 RiftForged Game Development Team

#include "UDPSocketAsync.h"
#include "../Utils/Logger.h" // <<< ENSURE THIS IS INCLUDED (directly or indirectly) for RF_... macros
#include <stdexcept>         // For std::system_error
#include <vector>
#include <cstring>           // For ZeroMemory, memcpy
#include <sstream> // For std::ostringstream

// For GetGamePacketHeaderSize, MessageType - ensure path is correct if needed by this file (likely not directly)
// #include "GamePacketHeader.h" 
// PlayerManager is passed by ref, so its header included via UDPSocketAsync.h
// #include "../Gameplay/PlayerManager.h" 

// Define NUM_WORKER_THREADS_TO_CREATE appropriately
// It's better if this is configurable or determined more robustly,
// but for now, using hardware_concurrency is a reasonable default.
static unsigned int DetermineNumWorkerThreads() {
    unsigned int num_threads = std::thread::hardware_concurrency();
    return (num_threads > 0) ? num_threads : 2; // Fallback to 2 if hardware_concurrency is 0
}
// You were using this global in the .cpp file which is fine for a single translation unit's static.
// If UDPSocketAsync could be in multiple .cpp files (not typical for a class impl), this would need to be extern or a static member.
static const unsigned int NUM_WORKER_THREADS_TO_CREATE = DetermineNumWorkerThreads();


namespace RiftForged {
    namespace Networking {

        UDPSocketAsync::UDPSocketAsync(RiftForged::GameLogic::PlayerManager& playerManager,
            PacketProcessor& packetProcessor,
            std::string listenIp, uint16_t listenPort)
            : m_playerManager(playerManager),       //
            m_packetProcessor(packetProcessor),   //
            m_listenIp(std::move(listenIp)),      //
            m_listenPort(listenPort),             //
            m_socket(INVALID_SOCKET),             //
            m_iocpHandle(NULL),                   //
            m_isRunning(false) {                  //
            RF_NETWORK_INFO("UDPSocketAsync: Constructor called for {}:{}", m_listenIp, m_listenPort); // MODIFIED Log
        }

        UDPSocketAsync::~UDPSocketAsync() {
            RF_NETWORK_INFO("UDPSocketAsync: Destructor called. Attempting to stop..."); // MODIFIED Log
            Stop(); //
        }

        bool UDPSocketAsync::IsRunning() const { //
            return m_isRunning.load(std::memory_order_relaxed); //
        }

        std::shared_ptr<ReliableConnectionState> UDPSocketAsync::GetOrCreateReliabilityState(const NetworkEndpoint& endpoint) {
            std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex); // Lock the mutex
            auto it = m_reliabilityStates.find(endpoint);
            if (it != m_reliabilityStates.end()) {
                // Found existing state
                return it->second;
            }
            else {
                // Not found, create a new state
                RF_NETWORK_INFO("UDPSocketAsync: Creating new ReliableConnectionState for endpoint: {}", endpoint.ToString());
                auto newState = std::make_shared<ReliableConnectionState>();
                m_reliabilityStates[endpoint] = newState; // Add to map
                m_endpointLastSeenTime[endpoint] = std::chrono::steady_clock::now();

                return newState;
            }
        }

        bool UDPSocketAsync::Init() {
            RF_NETWORK_INFO("UDPSocketAsync: Initializing..."); // MODIFIED Log
            WSADATA wsaData; //
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData); //
            if (result != 0) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: WSAStartup failed with error: {}", result); // MODIFIED Log
                return false;
            }
            RF_NETWORK_INFO("UDPSocketAsync: WSAStartup successful."); // MODIFIED Log

            m_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED); //
            if (m_socket == INVALID_SOCKET) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: WSASocket() failed with error: {}", WSAGetLastError()); // MODIFIED Log
                WSACleanup(); //
                return false;
            }
            RF_NETWORK_INFO("UDPSocketAsync: Socket created successfully (Socket ID: {}).", m_socket); // MODIFIED Log

            sockaddr_in serverAddr; //
            serverAddr.sin_family = AF_INET; //
            serverAddr.sin_port = htons(m_listenPort); //
            if (inet_pton(AF_INET, m_listenIp.c_str(), &serverAddr.sin_addr) != 1) { //
                RF_NETWORK_CRITICAL("UDPSocketAsync: inet_pton failed for IP {}. Error: {}", m_listenIp, WSAGetLastError()); // MODIFIED Log
                closesocket(m_socket); WSACleanup(); return false; //
            }

            if (bind(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) { //
                RF_NETWORK_CRITICAL("UDPSocketAsync: bind() failed with error: {}", WSAGetLastError()); // MODIFIED Log
                closesocket(m_socket); WSACleanup(); return false; //
            }
            RF_NETWORK_INFO("UDPSocketAsync: Socket bound successfully to {}:{}", m_listenIp, m_listenPort); // MODIFIED Log

            m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); //
            if (m_iocpHandle == NULL) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: CreateIoCompletionPort (for IOCP itself) failed with error: {}", GetLastError()); // MODIFIED Log
                closesocket(m_socket); WSACleanup(); return false; //
            }
            RF_NETWORK_DEBUG("UDPSocketAsync: IOCP created successfully."); // MODIFIED Log to DEBUG

            if (CreateIoCompletionPort((HANDLE)m_socket, m_iocpHandle, (ULONG_PTR)0, 0) == NULL) { //
                RF_NETWORK_CRITICAL("UDPSocketAsync: CreateIoCompletionPort (associating socket) failed with error: {}", GetLastError()); // MODIFIED Log
                CloseHandle(m_iocpHandle); m_iocpHandle = NULL; //
                closesocket(m_socket); WSACleanup(); return false; //
            }
            RF_NETWORK_DEBUG("UDPSocketAsync: Socket associated with IOCP successfully."); // MODIFIED Log to DEBUG

            try {
                m_receiveContextPool.reserve(MAX_PENDING_RECEIVES_IOCP); //
                for (int i = 0; i < MAX_PENDING_RECEIVES_IOCP; ++i) {
                    m_receiveContextPool.emplace_back(std::make_unique<OverlappedIOContext>(IOOperationType::Recv, DEFAULT_UDP_BUFFER_SIZE_IOCP)); //
                    m_freeReceiveContexts.push_back(m_receiveContextPool.back().get()); //
                }
                RF_NETWORK_INFO("UDPSocketAsync: Receive context pool initialized with {} contexts.", m_freeReceiveContexts.size()); // MODIFIED Log
            }
            catch (const std::bad_alloc& e) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: Failed to allocate memory for receive context pool: {}", e.what()); // MODIFIED Log
                if (m_iocpHandle) CloseHandle(m_iocpHandle); m_iocpHandle = NULL; //
                closesocket(m_socket); WSACleanup(); return false; //
            }

            RF_NETWORK_INFO("UDPSocketAsync: Initialization successful."); // MODIFIED Log
            return true;
        }

        bool UDPSocketAsync::Start() {
            if (m_socket == INVALID_SOCKET || m_iocpHandle == NULL) { //
                RF_NETWORK_ERROR("UDPSocketAsync: Cannot start. Socket not initialized or IOCP handle is null."); // MODIFIED Log
                return false;
            }
            if (m_isRunning.load()) { //
                RF_NETWORK_WARN("UDPSocketAsync: Already running."); // MODIFIED Log
                return true;
            }

            RF_NETWORK_INFO("UDPSocketAsync: Starting..."); // MODIFIED Log
            m_isRunning = true; //

            // Using the static const NUM_WORKER_THREADS_TO_CREATE defined at the top
            m_workerThreads.reserve(NUM_WORKER_THREADS_TO_CREATE); //
            for (unsigned int i = 0; i < NUM_WORKER_THREADS_TO_CREATE; ++i) {
                try {
                    m_workerThreads.emplace_back(&UDPSocketAsync::WorkerThread, this); //
                }
                catch (const std::system_error& e) {
                    RF_NETWORK_CRITICAL("UDPSocketAsync: Failed to create worker thread {}: {}", i, e.what()); // MODIFIED Log
                    Stop(); return false; //
                }
            }
            RF_NETWORK_INFO("UDPSocketAsync: {} worker threads created.", m_workerThreads.size()); // MODIFIED Log

            try {
                m_reliabilityThread = std::thread(&UDPSocketAsync::ReliabilityManagementThread, this);
                RF_NETWORK_INFO("UDPSocketAsync: Reliability management thread created.");
            }
            catch (const std::system_error& e) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: Failed to create reliability management thread: {}", e.what());
                Stop(); // Cleanup if thread creation fails
                return false;
            }

            int successfullyPosted = 0; //
            for (int i = 0; i < MAX_PENDING_RECEIVES_IOCP; ++i) { //
                OverlappedIOContext* pContext = GetFreeReceiveContext(); //
                if (!pContext) {
                    RF_NETWORK_ERROR("UDPSocketAsync: Start - Failed to get free receive context for initial post {}.", i); // MODIFIED Log
                    break;
                }
                if (!PostReceive(pContext)) { //
                    RF_NETWORK_ERROR("UDPSocketAsync: Start - Failed to post initial receive operation {}. Error: {}", i, WSAGetLastError()); // MODIFIED Log
                    ReturnReceiveContext(pContext); //
                }
                else {
                    successfullyPosted++; //
                }
            }

            if (successfullyPosted == 0 && MAX_PENDING_RECEIVES_IOCP > 0) { //
                RF_NETWORK_CRITICAL("UDPSocketAsync: CRITICAL - Failed to post ANY initial receive operations."); // MODIFIED Log
                Stop(); return false; //
            }
            RF_NETWORK_INFO("UDPSocketAsync: Successfully posted {} initial receive operations. Server is listening.", successfullyPosted); // MODIFIED Log
            return true;
        }

        void UDPSocketAsync::Stop() {
            if (!m_isRunning.exchange(false)) { return; } // 
            RF_NETWORK_INFO("UDPSocketAsync: Stopping..."); // MODIFIED Log

            if (m_reliabilityThread.joinable()) {
                RF_NETWORK_INFO("UDPSocketAsync: Joining reliability management thread...");
                m_reliabilityThread.join();
                RF_NETWORK_INFO("UDPSocketAsync: Reliability management thread joined.");
            }

            if (m_iocpHandle) { //
                for (size_t i = 0; i < m_workerThreads.size(); ++i) {
                    PostQueuedCompletionStatus(m_iocpHandle, 0, 0, NULL); //
                }
            }
            RF_NETWORK_DEBUG("UDPSocketAsync: Shutdown signals posted to IOCP for {} worker threads.", m_workerThreads.size()); // MODIFIED Log

            if (m_socket != INVALID_SOCKET) { //
                SOCKET tempSock = m_socket; //
                m_socket = INVALID_SOCKET;  // Prevent further use by worker threads quickly
                shutdown(tempSock, SD_BOTH); //
                closesocket(tempSock); //
                RF_NETWORK_INFO("UDPSocketAsync: Socket closed."); // MODIFIED Log
            }

            RF_NETWORK_INFO("UDPSocketAsync: Joining worker threads..."); // MODIFIED Log
            for (auto& thread : m_workerThreads) { //
                if (thread.joinable()) { //
                    thread.join(); //
                }
            }
            m_workerThreads.clear(); //
            RF_NETWORK_INFO("UDPSocketAsync: All worker threads joined."); // MODIFIED Log

            if (m_iocpHandle) { //
                CloseHandle(m_iocpHandle); //
                m_iocpHandle = NULL; //
                RF_NETWORK_INFO("UDPSocketAsync: IOCP handle closed."); // MODIFIED Log
            }

            {
                std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                m_reliabilityStates.clear();
                RF_NETWORK_DEBUG("UDPSocketAsync: Reliability states cleared.");
                m_endpointLastSeenTime.clear();
                RF_NETWORK_DEBUG("UDPSocketAsync: Endpoint last seen times cleared.");
            }

            {
                std::lock_guard<std::mutex> lock(m_receiveContextMutex); //
                m_freeReceiveContexts.clear(); //
            }
            m_receiveContextPool.clear(); // unique_ptrs will handle deletion
            RF_NETWORK_DEBUG("UDPSocketAsync: Receive context pool cleared."); // MODIFIED Log


            WSACleanup(); //
            RF_NETWORK_INFO("UDPSocketAsync: Stopped successfully."); // MODIFIED Log
        }

        OverlappedIOContext* UDPSocketAsync::GetFreeReceiveContext() {
            std::lock_guard<std::mutex> lock(m_receiveContextMutex); //
            if (m_freeReceiveContexts.empty()) { //
                RF_NETWORK_WARN("UDPSocketAsync: No free receive contexts available in pool."); // MODIFIED Log
                return nullptr;
            }
            OverlappedIOContext* pContext = m_freeReceiveContexts.front(); //
            m_freeReceiveContexts.pop_front(); //
            return pContext;
        }

        void UDPSocketAsync::ReturnReceiveContext(OverlappedIOContext* pContext) {
            if (!pContext) return; //
            std::lock_guard<std::mutex> lock(m_receiveContextMutex); //
            m_freeReceiveContexts.push_back(pContext); //
        }

        bool UDPSocketAsync::PostReceive(OverlappedIOContext* pRecvContext) {
            if (!pRecvContext) { //
                RF_NETWORK_CRITICAL("UDPSocketAsync::PostReceive: ERROR - pRecvContext is null."); // MODIFIED Log
                return false;
            }
            if (m_socket == INVALID_SOCKET) { //
                RF_NETWORK_ERROR("UDPSocketAsync::PostReceive: ERROR - Invalid socket, cannot post receive."); // MODIFIED Log
                return false;
            }

            pRecvContext->ResetForReceive(); //

            DWORD dwFlags = 0; //
            int result = WSARecvFrom( //
                m_socket,
                &(pRecvContext->wsaBuf),
                1,
                NULL,
                &dwFlags,
                (SOCKADDR*)&(pRecvContext->remoteAddrNative),
                &(pRecvContext->remoteAddrNativeLen),
                &(pRecvContext->overlapped),
                NULL
            );

            if (result == SOCKET_ERROR) { //
                int errorCode = WSAGetLastError(); //
                if (errorCode != WSA_IO_PENDING) { //
                    RF_NETWORK_ERROR("UDPSocketAsync::PostReceive: WSARecvFrom failed immediately with error: {} for context: {}", errorCode, (void*)pRecvContext); // MODIFIED Log
                    return false;
                }
                // RF_NETWORK_TRACE("UDPSocketAsync::PostReceive: WSARecvFrom pending for context {}", (void*)pRecvContext); // Optional: log pending
            }
            else {
                // RF_NETWORK_TRACE("UDPSocketAsync::PostReceive: WSARecvFrom completed immediately for context {}", (void*)pRecvContext); // Optional: log immediate completion
            }
            return true;
        }

        void UDPSocketAsync::WorkerThread() {
            std::ostringstream oss_thread_id_start; // Create ostringstream
            oss_thread_id_start << std::this_thread::get_id(); // Stream the ID
            RF_NETWORK_INFO("UDPSocketAsync: Worker thread started (ID: {})", oss_thread_id_start.str()); // Log the string

            OverlappedIOContext* pIoContext = nullptr; //
            DWORD bytesTransferred = 0; //
            ULONG_PTR completionKey = 0; //

            while (m_isRunning.load(std::memory_order_relaxed)) {
                pIoContext = nullptr;
                bytesTransferred = 0;

                BOOL bSuccess = GetQueuedCompletionStatus( //
                    m_iocpHandle,
                    &bytesTransferred,
                    &completionKey,
                    (LPOVERLAPPED*)&pIoContext,
                    //INFINITE
                    100 // timeout in MS
                );

                // It's good practice to get the current thread's ID once if logging it multiple times in an iteration
                std::ostringstream current_tid_oss;
                current_tid_oss << std::this_thread::get_id();
                std::string current_tid_str = current_tid_oss.str();

                if (!bSuccess) {
                    DWORD errorCode = GetLastError();
                    if (errorCode == WAIT_TIMEOUT) {
                        // This is an expected timeout due to the 100ms in GetQueuedCompletionStatus.
                        // Continue the loop to check m_isRunning.
                        continue;
                    }

                    // An actual error occurred.
                    RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread {} - GetQueuedCompletionStatus returned FALSE. WinError: {}. Context: {}, OpType: {}",
                        current_tid_str, errorCode, (void*)pIoContext, (pIoContext ? static_cast<int>(pIoContext->operationType) : -1));

                    if (pIoContext) { // If context is available, try to handle it
                        if (pIoContext->operationType == IOOperationType::Recv) {
                            RF_NETWORK_WARN("WorkerThread {}: Failed Recv Op. Attempting to re-post context {}", current_tid_str, (void*)pIoContext);
                            if (m_isRunning.load() && !PostReceive(pIoContext)) { // Only repost if running
                                RF_NETWORK_CRITICAL("WorkerThread {}: CRITICAL - Failed to re-post Recv context {} after I/O error. Returning to pool.", current_tid_str, (void*)pIoContext);
                                ReturnReceiveContext(pIoContext);
                            }
                            else if (!m_isRunning.load()) {
                                ReturnReceiveContext(pIoContext); // Not running, just return
                            }
                        }
                        else if (pIoContext->operationType == IOOperationType::Send) {
                            RF_NETWORK_ERROR("WorkerThread {}: Failed Send Op. Deleting context {}", current_tid_str, (void*)pIoContext);
                            delete pIoContext; // Send contexts are heap-allocated
                        }
                    }
                    else if (errorCode == ERROR_ABANDONED_WAIT_0 || !m_isRunning.load(std::memory_order_relaxed)) {
                        // IOCP Handle was closed, or we are shutting down
                        RF_NETWORK_INFO("UDPSocketAsync: WorkerThread {} - IOCP Handle likely closed or shutdown initiated (Error: {}). Exiting.", current_tid_str, errorCode);
                        break; // Exit the while loop
                    }
                    continue; // Continue to next iteration after handling error
                }

                // If GetQueuedCompletionStatus returns TRUE, but pIoContext is NULL, it's a signal to shut down.
                if (pIoContext == NULL) {
                    RF_NETWORK_INFO("UDPSocketAsync: WorkerThread {} received NULL context (explicit shutdown signal). Exiting.", current_tid_str);
                    break; // Exit the while loop
                }

                // Successful I/O
                switch (pIoContext->operationType) { //
                case IOOperationType::Recv: //
                {
                    if (bytesTransferred > 0) {
                        NetworkEndpoint sender_endpoint; //
                        char senderIpBuffer[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &(pIoContext->remoteAddrNative.sin_addr), senderIpBuffer, INET_ADDRSTRLEN)) {
                            sender_endpoint.ipAddress = senderIpBuffer;
                            sender_endpoint.port = ntohs(pIoContext->remoteAddrNative.sin_port);

                            {
                                std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                                m_endpointLastSeenTime[sender_endpoint] = std::chrono::steady_clock::now();
                                RF_NETWORK_TRACE("WorkerThread {}: Updated lastSeenTime for {}", current_tid_str, sender_endpoint.ToString());
                            }

                            RF_NETWORK_TRACE("WorkerThread {}: Received {} bytes from {}", current_tid_str, bytesTransferred, sender_endpoint.ToString()); // MODIFIED Log
                            //Modify Here
                            if (bytesTransferred < GetGamePacketHeaderSize()) { // Ensure packet is large enough
                                RF_NETWORK_WARN("WorkerThread {}: Received packet too small ({} bytes) from {}. Discarding.",
                                    current_tid_str, bytesTransferred, sender_endpoint.ToString());
                            }
                            else {
                                GamePacketHeader receivedHeader; // Deserialize the header
                                memcpy(&receivedHeader, pIoContext->buffer.data(), GetGamePacketHeaderSize());

                                // You might want to add a protocol ID check here if you have one:
                                // if (receivedHeader.protocolId != YOUR_PROTOCOL_ID) { ... discard ... }

                                // Get pointer to payload data and its length
                                const uint8_t* packetPayloadData = reinterpret_cast<const uint8_t*>(pIoContext->buffer.data()) + GetGamePacketHeaderSize();
                                uint16_t packetPayloadLength = static_cast<uint16_t>(bytesTransferred - GetGamePacketHeaderSize());

                                // Get or create the reliability state for this sender
                                auto connectionState = GetOrCreateReliabilityState(sender_endpoint);
                                if (!connectionState) {
                                    RF_NETWORK_ERROR("WorkerThread {}: Failed to get/create reliability state for {}. Discarding packet.", current_tid_str, sender_endpoint.ToString());
                                }
                                else {
                                    const uint8_t* appPayloadToProcess = nullptr; // Will point to the actual game payload if valid
                                    uint16_t appPayloadSize = 0;                  // Size of the game payload

                                    // Process the header using the reliability protocol.
                                    // This handles ACKs for packets we sent, and updates sequence numbers for packets received from them.
                                    // It returns true if this packet's payload is new and should be given to game logic.
                                    bool shouldRelayToGameLogic = ProcessIncomingPacketHeader(
                                        *connectionState,       // The reliability state for this sender
                                        receivedHeader,         // The deserialized header of the incoming packet
                                        packetPayloadData,      // Pointer to the payload part of the incoming packet
                                        packetPayloadLength,    // Length of the payload part
                                        &appPayloadToProcess,   // Output: pointer to the application payload if valid
                                        &appPayloadSize         // Output: size of the application payload
                                    );

                                    // If the reliability protocol says this packet's payload is for the game logic...
                                    if (shouldRelayToGameLogic) {
                                        RF_NETWORK_TRACE("WorkerThread {}: Relaying packet from {} (Type: {}) to PacketProcessor.",
                                            current_tid_str, sender_endpoint.ToString(), EnumNameMessageType(receivedHeader.messageType));

                                        // Call PacketProcessor with the ORIGINAL FULL packet data and length.
                                        // This is because your PacketProcessor::ProcessIncomingRawPacket expects the full frame,
                                        // and the reliability protocol has only determined *if* this frame should be processed further.
                                        std::optional<S2C_Response> s2c_response =
                                            m_packetProcessor.ProcessIncomingRawPacket(
                                                pIoContext->buffer.data(),          // Original full packet data
                                                static_cast<int>(bytesTransferred), // Original full packet length
                                                sender_endpoint);

                                        if (s2c_response.has_value()) {
                                            RF_NETWORK_DEBUG("WorkerThread {}: S2C_Response to send. Broadcast: {}, Recipient: [{}], C2S Sender: [{}], MsgType: {}",
                                                current_tid_str,
                                                s2c_response->broadcast,
                                                s2c_response->specific_recipient.ToString(),
                                                sender_endpoint.ToString(),
                                                static_cast<int>(s2c_response->messageType));

                                            // GamePacketHeader s2c_header(s2c_response->messageType); // This was old way
                                            // Now, SendReliableTo will handle creating the full packet with reliability info.
                                            // The s2c_response->data is the *application payload*.

                                            uint8_t responseFlags = 0; // Set any additional flags if needed (e.g. IS_HEARTBEAT from s2c_response if you add it there)

                                            if (s2c_response->broadcast) {
                                                std::vector<NetworkEndpoint> all_clients = m_playerManager.GetAllActiveClientEndpoints();
                                                RF_NETWORK_DEBUG("WorkerThread {}: Broadcasting S2C MsgType {} reliably to {} client(s).",
                                                    current_tid_str, static_cast<int>(s2c_response->messageType), all_clients.size());
                                                if (all_clients.empty() && s2c_response->messageType != MessageType::S2C_Pong) {
                                                    RF_NETWORK_WARN("WorkerThread {}: Broadcast requested, but PlayerManager found no active clients.", current_tid_str);
                                                }
                                                for (const auto& client_ep : all_clients) {
                                                    if (client_ep.ipAddress.empty() || client_ep.port == 0) {
                                                        RF_NETWORK_WARN("WorkerThread {}: BROADCAST SKIPPING INVALID ENDPOINT from PlayerManager: [{}]", current_tid_str, client_ep.ToString());
                                                        continue;
                                                    }
                                                    // Send the response reliably
                                                    SendReliableTo(client_ep,
                                                        s2c_response->messageType,
                                                        reinterpret_cast<const uint8_t*>(s2c_response->data.data()),
                                                        static_cast<uint16_t>(s2c_response->data.size()),
                                                        responseFlags);
                                                }
                                            }
                                            else { // Not a broadcast, send to specific recipient
                                                if (!s2c_response->specific_recipient.ipAddress.empty() && s2c_response->specific_recipient.port != 0) {
                                                    RF_NETWORK_DEBUG("WorkerThread {}: Sending S2C MsgType {} reliably to specific: [{}]",
                                                        current_tid_str, static_cast<int>(s2c_response->messageType), s2c_response->specific_recipient.ToString());
                                                    // Send the response reliably
                                                    SendReliableTo(s2c_response->specific_recipient,
                                                        s2c_response->messageType,
                                                        reinterpret_cast<const uint8_t*>(s2c_response->data.data()),
                                                        static_cast<uint16_t>(s2c_response->data.size()),
                                                        responseFlags);
                                                }
                                                else {
                                                    RF_NETWORK_ERROR("WorkerThread {}: ERROR - S2C_Response specific_recipient is INVALID. Original C2S sender: [{}]",
                                                        current_tid_str, sender_endpoint.ToString());
                                                }
                                            }
                                        }
                                    }
                                    else { // shouldRelayToGameLogic was false
                                        RF_NETWORK_TRACE("WorkerThread {}: Packet from {} (Type: {}) not relayed to game logic by reliability protocol (e.g. duplicate, pure ACK for us, etc.).",
                                            current_tid_str, sender_endpoint.ToString(), EnumNameMessageType(receivedHeader.messageType));
                                    }
                                } // End of `else` for `if (!connectionState)`
                            } // End of `else` for `if (bytesTransferred < GetGamePacketHeaderSize())`
                            //Modify End
                        }
                        else {
                            RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread {} - inet_ntop failed for received packet. Error: {}", current_tid_str, WSAGetLastError()); // MODIFIED Log
                        }
                    }
                    else if (bytesTransferred == 0) {
                        NetworkEndpoint sender_endpoint;
                        RF_NETWORK_WARN("UDPSocketAsync: WorkerThread {} - Received 0 bytes on a Recv operation (UDP). From: {}", current_tid_str, sender_endpoint.ToString()); // MODIFIED Log
                    }

                    if (m_isRunning.load(std::memory_order_relaxed)) {
                        if (!PostReceive(pIoContext)) {
                            RF_NETWORK_CRITICAL("UDPSocketAsync: WorkerThread {} - CRITICAL: Failed to re-post WSARecvFrom. Context: {}. Error: {}",
                                current_tid_str, (void*)pIoContext, WSAGetLastError()); // MODIFIED Log
                            ReturnReceiveContext(pIoContext); // Return to pool if cannot re-post
                        }
                        else {
                            RF_NETWORK_TRACE("WorkerThread {}: Successfully re-posted Recv context {}", current_tid_str, (void*)pIoContext); // MODIFIED Log (changed to TRACE)
                        }
                    }
                    else { // Not running anymore, just return the context
                        ReturnReceiveContext(pIoContext);
                    }
                }
                break;

                case IOOperationType::Send: //
                    RF_NETWORK_TRACE("UDPSocketAsync: WorkerThread {} - Send operation completed. Context: {}, Bytes: {}", // MODIFIED Log
                        current_tid_str, (void*)pIoContext, bytesTransferred);
                    delete pIoContext;
                    pIoContext = nullptr;
                    break;

                default:
                    RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread {} - Dequeued completed op with Unknown/None type. Context: {}, OpType: {}", // MODIFIED Log
                        current_tid_str, (void*)pIoContext, (pIoContext ? static_cast<int>(pIoContext->operationType) : -1));
                    if (pIoContext) {
                        //if (pIoContext->operationType != IOOperationType::Send) {
                        //    RF_NETWORK_WARN("WorkerThread {}: Attempting to return unexpected context {} as Recv.", current_tid_str, (void*)pIoContext); // MODIFIED Log
                        //    ReturnReceiveContext(pIoContext);
                        //}
                        //else 
                        {
                            RF_NETWORK_ERROR("WorkerThread {}: Deleting unexpected Send context {}", current_tid_str, (void*)pIoContext); // MODIFIED Log
                            delete pIoContext;
                        }
                    }
                    break;
                }
            }

            std::ostringstream exit_tid_oss; // Create ostringstream
            exit_tid_oss << std::this_thread::get_id(); // Stream the ID
            RF_NETWORK_INFO("UDPSocketAsync: Worker thread {} exiting gracefully.", exit_tid_oss.str()); // MODIFIED Log
        }


        bool UDPSocketAsync::SendRawTo(const NetworkEndpoint& recipient, const char* data, int length) {
            if (m_socket == INVALID_SOCKET) { //
                RF_NETWORK_ERROR("UDPSocketAsync::SendRawTo: Socket not valid. Cannot send to {}.", recipient.ToString()); // MODIFIED Log
                return false;
            }
            if (length <= 0 || !data) { //
                RF_NETWORK_ERROR("UDPSocketAsync::SendRawTo: Invalid data or length ({}) for sending to {}.", length, recipient.ToString()); // MODIFIED Log
                return false;
            }

            // For IOCP, sends are also overlapped. This creates a new context for each send.
            // This context will be deleted by the worker thread when the send completes.
            OverlappedIOContext* sendContext = new (std::nothrow) OverlappedIOContext(IOOperationType::Send, static_cast<size_t>(length)); //
            if (!sendContext) {
                RF_NETWORK_CRITICAL("UDPSocketAsync::SendRawTo: Failed to allocate memory for send context to {}.", recipient.ToString()); // MODIFIED Log
                return false;
            }

            memcpy(sendContext->buffer.data(), data, length); //
            sendContext->wsaBuf.len = length; //

            sendContext->remoteAddrNative.sin_family = AF_INET; //
            sendContext->remoteAddrNative.sin_port = htons(recipient.port); //
            if (inet_pton(AF_INET, recipient.ipAddress.c_str(), &(sendContext->remoteAddrNative.sin_addr)) != 1) { //
                RF_NETWORK_ERROR("UDPSocketAsync::SendRawTo: inet_pton failed for IP {} to {}. Error: {}", recipient.ipAddress, recipient.ToString(), WSAGetLastError()); // MODIFIED Log
                delete sendContext; return false; //
            }

            // RF_NETWORK_TRACE("UDPSocketAsync::SendRawTo: Attempting WSASendTo {} bytes to {}", length, recipient.ToString()); // Optional: Trace send attempt

            int result = WSASendTo(m_socket, &(sendContext->wsaBuf), 1, NULL, 0, //
                (SOCKADDR*)&(sendContext->remoteAddrNative), sizeof(sendContext->remoteAddrNative),
                &(sendContext->overlapped), NULL);

            if (result == SOCKET_ERROR) { //
                int errorCode = WSAGetLastError(); //
                if (errorCode != WSA_IO_PENDING) { //
                    RF_NETWORK_ERROR("UDPSocketAsync::SendRawTo: WSASendTo failed immediately to {} with error: {}", recipient.ToString(), errorCode); // MODIFIED Log
                    delete sendContext; return false; //
                }
                // RF_NETWORK_TRACE("UDPSocketAsync::SendRawTo: WSASendTo pending for {}", recipient.ToString()); // Optional: Trace pending send
            }
            else {
                // RF_NETWORK_TRACE("UDPSocketAsync::SendRawTo: WSASendTo completed immediately for {}", recipient.ToString()); // Optional: Trace immediate send completion
                // Even if it completes immediately, a completion packet is queued to IOCP if associated.
                // The sendContext will be deleted by the worker thread.
            }
            return true;
        }

        bool UDPSocketAsync::SendReliableTo(const NetworkEndpoint& recipient,
            MessageType messageType,
            const uint8_t* payloadData,
            uint16_t payloadSize,
            uint8_t additionalFlags) { // e.g., IS_HEARTBEAT

            if (!m_isRunning.load(std::memory_order_relaxed)) {
                RF_NETWORK_WARN("SendReliableTo: Not running, cannot send packet to {}.", recipient.ToString());
                return false;
            }

            // Get (or create if it's the first time sending to this recipient) the reliability state.
            auto connectionState = GetOrCreateReliabilityState(recipient);
            if (!connectionState) { // Should not happen with GetOrCreate logic but good practice
                RF_NETWORK_ERROR("SendReliableTo: Could not get/create reliability state for {}. Packet not sent.", recipient.ToString());
                return false;
            }

            // Combine the IS_RELIABLE flag with any other flags passed in.
            uint8_t packetFlags = static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | additionalFlags;

            // Use the reliability protocol to prepare the full packet buffer.
            // This will add the necessary reliability headers (sequence number, ack, ack bitfield).
            std::vector<uint8_t> packetBuffer = PrepareOutgoingPacket(
                *connectionState,    // The reliability state for this connection
                messageType,         // The type of message being sent
                payloadData,         // The actual game data payload
                payloadSize,         // Size of the game data payload
                packetFlags          // Flags, including IS_RELIABLE
            );

            if (packetBuffer.empty()) {
                // PrepareOutgoingPacket might return an empty vector on certain errors (e.g., null payload for non-ack).
                RF_NETWORK_WARN("SendReliableTo: PrepareOutgoingPacket returned empty buffer for MsgType {} to {}. Not sending.",
                    EnumNameMessageType(messageType), recipient.ToString());
                return false;
            }

            RF_NETWORK_TRACE("SendReliableTo: Sending reliable MsgType {} ({} bytes total) to {}",
                EnumNameMessageType(messageType), packetBuffer.size(), recipient.ToString());

            // Send the fully prepared packet (header + payload) using the raw send function.
            return SendRawTo(recipient, reinterpret_cast<const char*>(packetBuffer.data()), static_cast<int>(packetBuffer.size()));
        }

        bool UDPSocketAsync::SendUnreliableTo(const NetworkEndpoint& recipient,
            MessageType messageType,
            const uint8_t* payloadData,
            uint16_t payloadSize,
            uint8_t additionalFlags) {

            if (!m_isRunning.load(std::memory_order_relaxed)) {
                RF_NETWORK_WARN("SendUnreliableTo: Not running, cannot send packet to {}.", recipient.ToString());
                return false;
            }

            // We still need the connection state to get current ACK info to send to the remote.
            auto connectionState = GetOrCreateReliabilityState(recipient);
            if (!connectionState) {
                RF_NETWORK_ERROR("SendUnreliableTo: Could not get/create reliability state for {}. Packet not sent.", recipient.ToString());
                return false;
            }

            // Ensure the IS_RELIABLE flag is NOT set for unreliable packets.
            // We take additionalFlags and explicitly remove IS_RELIABLE if it was accidentally set.
            uint8_t packetFlags = additionalFlags & (~static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE));

            // Prepare the packet. This will include current ACK info from connectionState.
            std::vector<uint8_t> packetBuffer = PrepareOutgoingPacket(
                *connectionState,
                messageType,
                payloadData,
                payloadSize,
                packetFlags // IS_RELIABLE is not set here
            );

            if (packetBuffer.empty()) {
                RF_NETWORK_WARN("SendUnreliableTo: PrepareOutgoingPacket returned empty buffer for MsgType {} to {}. Not sending.",
                    EnumNameMessageType(messageType), recipient.ToString());
                return false;
            }

            RF_NETWORK_TRACE("SendUnreliableTo: Sending unreliable MsgType {} ({} bytes total) to {}",
                EnumNameMessageType(messageType), packetBuffer.size(), recipient.ToString());

            return SendRawTo(recipient, reinterpret_cast<const char*>(packetBuffer.data()), static_cast<int>(packetBuffer.size()));
        }

        void UDPSocketAsync::ReliabilityManagementThread() {
            std::ostringstream oss_thread_id_start;
            oss_thread_id_start << std::this_thread::get_id();
            RF_NETWORK_INFO("UDPSocketAsync: ReliabilityManagementThread started (ID: {})", oss_thread_id_start.str());

            while (m_isRunning.load(std::memory_order_relaxed)) {
                auto currentTime = std::chrono::steady_clock::now();

                // Temporary storage for packets to resend and ACKs to send.
                // We collect them first, then send outside the lock to minimize lock holding time.
                std::vector<std::pair<NetworkEndpoint, std::vector<uint8_t>>> packetsToResendGlobal;
                std::vector<std::pair<NetworkEndpoint, std::vector<uint8_t>>> acksToSendGlobal;

                // Scope for locking m_reliabilityStatesMutex
                {
                    std::lock_guard<std::mutex> lock(m_reliabilityStatesMutex);
                    // Iterate over all known reliability states (one per remote endpoint)
                    for (auto it = m_reliabilityStates.begin(); it != m_reliabilityStates.end(); /* manual increment below */) {
                        const NetworkEndpoint& endpoint = it->first;
                        std::shared_ptr<ReliableConnectionState> state_ptr = it->second;

                        if (!state_ptr) { // Should not happen if managed correctly
                            RF_NETWORK_WARN("ReliabilityManagementThread: Found null state_ptr for endpoint {}, removing.", endpoint.ToString());
                            m_endpointLastSeenTime.erase(endpoint); // Also remove from the last seen map
                            it = m_reliabilityStates.erase(it);
                            continue;
                        }

                        // 1. Get packets for retransmission
                        // This function checks unacknowledgedSentPackets in the state_ptr.
                        std::vector<std::vector<uint8_t>> retransmitList = GetPacketsForRetransmission(
                            *state_ptr,        // The connection state
                            currentTime,       // Current time for RTO check
                            DEFAULT_RTO_MS,    // Retransmission Timeout
                            DEFAULT_MAX_RETRIES // Max retries before dropping
                        );
                        // Add any packets needing retransmission to our global list
                        for (const auto& packetData : retransmitList) {
                            packetsToResendGlobal.emplace_back(endpoint, packetData);
                        }

                        // 2. Check if an ACK-only packet needs to be sent.
                        // The hasPendingAckToSend flag in the state is set by ProcessIncomingPacketHeader
                        // when we receive a reliable packet from the remote.
                        // PrepareOutgoingPacket (called below or by SendReliableTo) will clear this flag
                        // because it includes the ACK info in the outgoing packet.
                        // We only need to send an *explicit* ACK if no other data is going out soon.

                        bool justRetransmittedToThisEndpoint = false;
                        for (const auto& p_resend : packetsToResendGlobal) {
                            if (p_resend.first == endpoint) {
                                justRetransmittedToThisEndpoint = true;
                                break;
                            }
                        }

                        if (state_ptr->hasPendingAckToSend && !justRetransmittedToThisEndpoint) {
                            // Heuristic: Only send an explicit ACK if we haven't sent *any* packet to them
                            // for a short while (e.g., more than twice the reliability thread's sleep interval).
                            // This prevents sending ACKs too aggressively if data packets are also flowing,
                            // as those data packets will already carry the ACK.
                            auto timeSinceLastSentByUs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                currentTime - state_ptr->lastPacketSentTimeToRemote);

                            if (timeSinceLastSentByUs.count() > (RELIABILITY_THREAD_SLEEP_MS * 2)) {
                                RF_NETWORK_TRACE("ReliabilityThread: Pending ACK for {}. Preparing ACK-only packet.", endpoint.ToString());
                                // Prepare an ACK-only packet. It will be reliable itself.
                                // MessageType can be a generic one, or a specific ACK type if you have one.
                                // The IS_ACK_ONLY flag is the important part.
                                std::vector<uint8_t> ackPacket = PrepareOutgoingPacket(
                                    *state_ptr,           // Connection state (will provide ackNum, ackBitfield)
                                    MessageType::Unknown, // Use your existing MessageType::Unknown
                                    nullptr,              // No payload for ACK-only
                                    0,                    // Payload size is 0
                                    static_cast<uint8_t>(GamePacketFlag::IS_RELIABLE) | static_cast<uint8_t>(GamePacketFlag::IS_ACK_ONLY)
                                );
                                if (!ackPacket.empty()) {
                                    acksToSendGlobal.emplace_back(endpoint, ackPacket);
                                    // Note: PrepareOutgoingPacket internally sets state_ptr->hasPendingAckToSend = false
                                    // and updates state_ptr->lastPacketSentTimeToRemote.
                                }
                            }
                        }

                        bool isConnectionStale = false;
                        if (state_ptr->unacknowledgedSentPackets.empty()) {
                            auto lastSeenIt = m_endpointLastSeenTime.find(endpoint);
                            if (lastSeenIt != m_endpointLastSeenTime.end()) {
                                auto timeSinceLastSeen = std::chrono::duration_cast<std::chrono::seconds>(
                                    currentTime - lastSeenIt->second);
                                if (timeSinceLastSeen.count() > STALE_CONNECTION_TIMEOUT_SECONDS) {
                                    isConnectionStale = true;
                                }
                            }
                            else {
                                // Endpoint exists in reliabilityStates but not in lastSeenTime.
                                // This might happen if it was just added but hasn't sent anything yet.
                                // Or, it could be an anomaly. For staleness, we need a valid lastSeenTime.
                                // To be safe, if it's missing here, assume it's very old or treat as stale after a shorter grace.
                                // For now, we'll only mark stale if lastSeenTime confirms it.
                                // If it's a new connection, it won't be stale yet anyway.
                                RF_NETWORK_WARN("ReliabilityManagementThread: Endpoint {} in reliabilityStates but not in m_endpointLastSeenTime. Cannot determine staleness from last seen time.", endpoint.ToString());
                            }
                        }

                        if (isConnectionStale) {
                            RF_NETWORK_INFO("ReliabilityManagementThread: Stale connection detected for endpoint {}. Removing state.", endpoint.ToString());
                            m_endpointLastSeenTime.erase(endpoint);   // Remove from last seen map
                            it = m_reliabilityStates.erase(it); // Erase from reliability states map and get next valid iterator
                        }
                        else {
                            ++it; // Only increment iterator if no erase occurred
                        }
                    }
                } // Mutex is unlocked here

                // Send collected retransmissions (outside the lock)
                for (const auto& pair : packetsToResendGlobal) {
                    RF_NETWORK_WARN("ReliabilityThread: Retransmitting {} bytes to {}", pair.second.size(), pair.first.ToString());
                    SendRawTo(pair.first, reinterpret_cast<const char*>(pair.second.data()), static_cast<int>(pair.second.size()));
                }

                // Send collected pure ACKs (outside the lock)
                for (const auto& pair : acksToSendGlobal) {
                    RF_NETWORK_TRACE("ReliabilityThread: Sending explicit ACK-only packet ({} bytes) to {}", pair.second.size(), pair.first.ToString());
                    SendRawTo(pair.first, reinterpret_cast<const char*>(pair.second.data()), static_cast<int>(pair.second.size()));
                }

                // Sleep for a short duration before checking again.
                std::this_thread::sleep_for(std::chrono::milliseconds(RELIABILITY_THREAD_SLEEP_MS));
            } // End of while(m_isRunning)

            std::ostringstream exit_tid_oss;
            exit_tid_oss << std::this_thread::get_id();
            RF_NETWORK_INFO("UDPSocketAsync: ReliabilityManagementThread {} exiting gracefully.", exit_tid_oss.str());
        }

    } // namespace Networking
} // namespace RiftForged