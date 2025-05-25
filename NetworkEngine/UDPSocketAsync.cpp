// File: UDPSocketAsync.cpp
// RiftForged Game Development Team
// Copyright (c) 2025-2028 RiftForged Game Development Team

#include "UDPSocketAsync.h"
#include "../Utils/Logger.h" // <<< ENSURE THIS IS INCLUDED (directly or indirectly) for RF_... macros
#include <stdexcept>         // For std::system_error
#include <vector>
#include <cstring>           // For ZeroMemory, memcpy

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

            while (true) {
                pIoContext = nullptr;
                bytesTransferred = 0;

                BOOL bSuccess = GetQueuedCompletionStatus( //
                    m_iocpHandle,
                    &bytesTransferred,
                    &completionKey,
                    (LPOVERLAPPED*)&pIoContext,
                    INFINITE
                );

                // It's good practice to get the current thread's ID once if logging it multiple times in an iteration
                std::ostringstream current_tid_oss;
                current_tid_oss << std::this_thread::get_id();
                std::string current_tid_str = current_tid_oss.str();

                if (!m_isRunning.load(std::memory_order_relaxed)) {
                    if (pIoContext) {
                        if (pIoContext->operationType == IOOperationType::Recv) { //
                            RF_NETWORK_DEBUG("WorkerThread {}: Returning Recv context {} during shutdown.", current_tid_str, (void*)pIoContext); // MODIFIED Log
                            ReturnReceiveContext(pIoContext);
                        }
                        else if (pIoContext->operationType == IOOperationType::Send) { //
                            RF_NETWORK_DEBUG("WorkerThread {}: Deleting Send context {} during shutdown.", current_tid_str, (void*)pIoContext); // MODIFIED Log
                            delete pIoContext;
                        }
                    }
                    RF_NETWORK_INFO("UDPSocketAsync: WorkerThread {} detected m_isRunning is false, exiting.", current_tid_str); // MODIFIED Log
                    break;
                }

                if (pIoContext == NULL) { // Explicit shutdown signal from Stop()
                    RF_NETWORK_INFO("UDPSocketAsync: WorkerThread {} received NULL context (explicit shutdown signal). Exiting.", current_tid_str); // MODIFIED Log
                    break;
                }

                if (!bSuccess) {
                    DWORD errorCode = GetLastError();
                    RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread {} - GetQueuedCompletionStatus returned FALSE. WinError: {}. Context: {}, OpType: {}",
                        current_tid_str, errorCode, (void*)pIoContext, (pIoContext ? static_cast<int>(pIoContext->operationType) : -1)); // MODIFIED Log

                    if (pIoContext) {
                        if (pIoContext->operationType == IOOperationType::Recv) { //
                            RF_NETWORK_WARN("WorkerThread {}: Failed Recv Op. Attempting to re-post context {}", current_tid_str, (void*)pIoContext); // MODIFIED Log
                            if (!PostReceive(pIoContext)) {
                                RF_NETWORK_CRITICAL("WorkerThread {}: CRITICAL - Failed to re-post Recv context {} after I/O error. Returning to pool.", current_tid_str, (void*)pIoContext); // MODIFIED Log
                                ReturnReceiveContext(pIoContext);
                            }
                        }
                        else if (pIoContext->operationType == IOOperationType::Send) { //
                            RF_NETWORK_ERROR("WorkerThread {}: Failed Send Op. Deleting context {}", current_tid_str, (void*)pIoContext); // MODIFIED Log
                            delete pIoContext;
                        }
                    }
                    else if (errorCode == ERROR_ABANDONED_WAIT_0) {
                        RF_NETWORK_INFO("UDPSocketAsync: WorkerThread {} - IOCP Handle closed (ERROR_ABANDONED_WAIT_0). Exiting.", current_tid_str); // MODIFIED Log
                        break;
                    }
                    continue;
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

                            RF_NETWORK_TRACE("WorkerThread {}: Received {} bytes from {}", current_tid_str, bytesTransferred, sender_endpoint.ToString()); // MODIFIED Log

                            std::optional<S2C_Response> s2c_response =
                                m_packetProcessor.ProcessIncomingRawPacket( //
                                    pIoContext->buffer.data(), static_cast<int>(bytesTransferred), sender_endpoint);

                            if (s2c_response.has_value()) {
                                RF_NETWORK_DEBUG("WorkerThread {}: S2C_Response to send. Broadcast: {}, Recipient: [{}], C2S Sender: [{}], MsgType: {}", // MODIFIED Log
                                    current_tid_str,
                                    s2c_response->broadcast,
                                    s2c_response->specific_recipient.ToString(),
                                    sender_endpoint.ToString(),
                                    static_cast<int>(s2c_response->messageType));

                                GamePacketHeader s2c_header(s2c_response->messageType); //
                                // TODO: Populate reliability fields in s2c_header

                                size_t headerSize = GetGamePacketHeaderSize(); //
                                std::vector<char> sendPacketBuffer(headerSize + s2c_response->data.size());
                                memcpy(sendPacketBuffer.data(), &s2c_header, headerSize);
                                memcpy(sendPacketBuffer.data() + headerSize, s2c_response->data.data(), s2c_response->data.size());

                                if (s2c_response->broadcast) {
                                    std::vector<NetworkEndpoint> all_clients = m_playerManager.GetAllActiveClientEndpoints(); //
                                    RF_NETWORK_DEBUG("WorkerThread {}: Broadcasting S2C MsgType {} to {} client(s).", // MODIFIED Log
                                        current_tid_str, static_cast<int>(s2c_header.messageType), all_clients.size());
                                    if (all_clients.empty() && s2c_response->messageType != MessageType::S2C_Pong) {
                                        RF_NETWORK_WARN("WorkerThread {}: Broadcast requested, but PlayerManager found no active clients.", current_tid_str); // MODIFIED Log
                                    }
                                    for (const auto& client_ep : all_clients) {
                                        if (client_ep.ipAddress.empty() || client_ep.port == 0) {
                                            RF_NETWORK_WARN("WorkerThread {}: BROADCAST SKIPPING INVALID ENDPOINT from PlayerManager: [{}]", current_tid_str, client_ep.ToString()); // MODIFIED Log
                                            continue;
                                        }
                                        if (!SendTo(client_ep, sendPacketBuffer.data(), static_cast<int>(sendPacketBuffer.size()))) {
                                            // SendTo logs its own failures via RF_NETWORK_ERROR
                                        }
                                    }
                                }
                                else {
                                    if (!s2c_response->specific_recipient.ipAddress.empty() && s2c_response->specific_recipient.port != 0) {
                                        RF_NETWORK_DEBUG("WorkerThread {}: Sending S2C MsgType {} to specific: [{}]", // MODIFIED Log
                                            current_tid_str, static_cast<int>(s2c_header.messageType), s2c_response->specific_recipient.ToString());
                                        if (!SendTo(s2c_response->specific_recipient, sendPacketBuffer.data(), static_cast<int>(sendPacketBuffer.size()))) {
                                            // SendTo logs its own failures
                                        }
                                    }
                                    else {
                                        RF_NETWORK_ERROR("WorkerThread {}: ERROR - S2C_Response specific_recipient is INVALID. Original C2S sender: [{}]", // MODIFIED Log
                                            current_tid_str, sender_endpoint.ToString());
                                    }
                                }
                            }
                        }
                        else {
                            RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread {} - inet_ntop failed for received packet. Error: {}", current_tid_str, WSAGetLastError()); // MODIFIED Log
                        }
                    }
                    else if (bytesTransferred == 0) {
                        NetworkEndpoint sender_endpoint;
                        RF_NETWORK_WARN("UDPSocketAsync: WorkerThread {} - Received 0 bytes on a Recv operation (UDP). From: {}", current_tid_str, sender_endpoint.ToString()); // MODIFIED Log
                    }

                    if (!PostReceive(pIoContext)) {
                        RF_NETWORK_CRITICAL("UDPSocketAsync: WorkerThread {} - CRITICAL: Failed to re-post WSARecvFrom. Context: {}. Error: {}",
                            current_tid_str, (void*)pIoContext, WSAGetLastError()); // MODIFIED Log
                        ReturnReceiveContext(pIoContext);
                    }
                    else {
                        RF_NETWORK_TRACE("WorkerThread {}: Successfully re-posted Recv context {}", current_tid_str, (void*)pIoContext); // MODIFIED Log (changed to TRACE)
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
                        if (pIoContext->operationType != IOOperationType::Send) {
                            RF_NETWORK_WARN("WorkerThread {}: Attempting to return unexpected context {} as Recv.", current_tid_str, (void*)pIoContext); // MODIFIED Log
                            ReturnReceiveContext(pIoContext);
                        }
                        else {
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


        bool UDPSocketAsync::SendTo(const NetworkEndpoint& recipient, const char* data, int length) {
            if (m_socket == INVALID_SOCKET) { //
                RF_NETWORK_ERROR("UDPSocketAsync::SendTo: Socket not valid. Cannot send to {}.", recipient.ToString()); // MODIFIED Log
                return false;
            }
            if (length <= 0 || !data) { //
                RF_NETWORK_ERROR("UDPSocketAsync::SendTo: Invalid data or length ({}) for sending to {}.", length, recipient.ToString()); // MODIFIED Log
                return false;
            }

            // For IOCP, sends are also overlapped. This creates a new context for each send.
            // This context will be deleted by the worker thread when the send completes.
            OverlappedIOContext* sendContext = new (std::nothrow) OverlappedIOContext(IOOperationType::Send, static_cast<size_t>(length)); //
            if (!sendContext) {
                RF_NETWORK_CRITICAL("UDPSocketAsync::SendTo: Failed to allocate memory for send context to {}.", recipient.ToString()); // MODIFIED Log
                return false;
            }

            memcpy(sendContext->buffer.data(), data, length); //
            sendContext->wsaBuf.len = length; //

            sendContext->remoteAddrNative.sin_family = AF_INET; //
            sendContext->remoteAddrNative.sin_port = htons(recipient.port); //
            if (inet_pton(AF_INET, recipient.ipAddress.c_str(), &(sendContext->remoteAddrNative.sin_addr)) != 1) { //
                RF_NETWORK_ERROR("UDPSocketAsync::SendTo: inet_pton failed for IP {} to {}. Error: {}", recipient.ipAddress, recipient.ToString(), WSAGetLastError()); // MODIFIED Log
                delete sendContext; return false; //
            }

            // RF_NETWORK_TRACE("UDPSocketAsync::SendTo: Attempting WSASendTo {} bytes to {}", length, recipient.ToString()); // Optional: Trace send attempt

            int result = WSASendTo(m_socket, &(sendContext->wsaBuf), 1, NULL, 0, //
                (SOCKADDR*)&(sendContext->remoteAddrNative), sizeof(sendContext->remoteAddrNative),
                &(sendContext->overlapped), NULL);

            if (result == SOCKET_ERROR) { //
                int errorCode = WSAGetLastError(); //
                if (errorCode != WSA_IO_PENDING) { //
                    RF_NETWORK_ERROR("UDPSocketAsync::SendTo: WSASendTo failed immediately to {} with error: {}", recipient.ToString(), errorCode); // MODIFIED Log
                    delete sendContext; return false; //
                }
                // RF_NETWORK_TRACE("UDPSocketAsync::SendTo: WSASendTo pending for {}", recipient.ToString()); // Optional: Trace pending send
            }
            else {
                // RF_NETWORK_TRACE("UDPSocketAsync::SendTo: WSASendTo completed immediately for {}", recipient.ToString()); // Optional: Trace immediate send completion
                // Even if it completes immediately, a completion packet is queued to IOCP if associated.
                // The sendContext will be deleted by the worker thread.
            }
            return true;
        }

    } // namespace Networking
} // namespace RiftForged