// File: UDPSocketAsync.cpp (Refactored)
// RiftForged Game Development Team
// Copyright (c) 2023-2025 RiftForged Game Development Team
// Description: Implements INetworkIO for asynchronous UDP socket operations using IOCP.

#include "UDPSocketAsync.h"     // Should now include the refactored header
#include "INetworkIOEvents.h"   // For m_eventHandler calls
#include "OverlappedIOContext.h"// For IOOperationType and OverlappedIOContext struct
#include "../Utils/Logger.h"    // Ensure this path is correct for RF_... macros
#include <stdexcept>            // For std::system_error
#include <vector>
#include <cstring>              // For ZeroMemory, memcpy
#include <sstream>              // For std::ostringstream

// These constants are specific to UDPSocketAsync's pooling strategy,
// so they are fine here or in its header.
// const int DEFAULT_UDP_BUFFER_SIZE_IOCP was in the original UDPSocketAsync.h
// const int MAX_PENDING_RECEIVES_IOCP was in the original UDPSocketAsync.h

// DetermineNumWorkerThreads and NUM_WORKER_THREADS_TO_CREATE can remain as they are,
// as they are specific to this IOCP implementation's threading model.
static unsigned int DetermineNumWorkerThreads() {
    unsigned int num_threads = 4;  //std::thread::hardware_concurrency();
    // Ensure at least 1 thread, fallback to a reasonable number if detection fails.
    if (num_threads == 0) num_threads = 4;
    return num_threads;
}
// This should ideally be a member or configurable, but static const is from original.
static const unsigned int NUM_WORKER_THREADS_TO_CREATE = DetermineNumWorkerThreads();


namespace RiftForged {
    namespace Networking {

        // Constructor: Initializes members. IP/Port and handler are set in Init.
        UDPSocketAsync::UDPSocketAsync()
            : m_listenIp(""),
            m_listenPort(0),
            m_eventHandler(nullptr), // Will be set in Init()
            m_socket(INVALID_SOCKET),
            m_iocpHandle(NULL),
            m_isRunning(false)
            // MAX_PENDING_RECEIVES_IOCP and DEFAULT_UDP_BUFFER_SIZE_IOCP are const members from header
        {
            RF_NETWORK_INFO("UDPSocketAsync: Constructor called.");
        }

        UDPSocketAsync::~UDPSocketAsync() {
            RF_NETWORK_INFO("UDPSocketAsync: Destructor called. Attempting to stop...");
            Stop(); // Ensure Stop is called to clean up resources
        }

        bool UDPSocketAsync::IsRunning() const {
            return m_isRunning.load(std::memory_order_acquire); // Acquire for visibility
        }

        // Init method from INetworkIO interface
        bool UDPSocketAsync::Init(const std::string& listenIp, uint16_t listenPort, INetworkIOEvents* eventHandler) {
            RF_NETWORK_INFO("UDPSocketAsync: Initializing for {}:{}...", listenIp, listenPort);

            if (m_isRunning.load(std::memory_order_relaxed)) {
                RF_NETWORK_WARN("UDPSocketAsync: Already initialized and potentially running. Please Stop first.");
                return false; // Or handle re-initialization if desired
            }
            if (!eventHandler) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: Initialization failed - INetworkIOEvents handler is null.");
                return false;
            }

            m_eventHandler = eventHandler;
            m_listenIp = listenIp;
            m_listenPort = listenPort;

            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: WSAStartup failed with error: {}", result);
                m_eventHandler->OnNetworkError("WSAStartup failed", result);
                return false;
            }
            RF_NETWORK_INFO("UDPSocketAsync: WSAStartup successful.");

            m_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
            if (m_socket == INVALID_SOCKET) {
                int errorCode = WSAGetLastError();
                RF_NETWORK_CRITICAL("UDPSocketAsync: WSASocket() failed with error: {}", errorCode);
                m_eventHandler->OnNetworkError("WSASocket failed", errorCode);
                WSACleanup();
                return false;
            }
            RF_NETWORK_INFO("UDPSocketAsync: Socket created successfully (Socket ID: {}).", m_socket);

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(m_listenPort);
            if (inet_pton(AF_INET, m_listenIp.c_str(), &serverAddr.sin_addr) != 1) {
                int errorCode = WSAGetLastError(); // Specific to Windows for inet_pton, consider logging GetLastError() as well.
                RF_NETWORK_CRITICAL("UDPSocketAsync: inet_pton failed for IP {}. Error: {}", m_listenIp, errorCode);
                m_eventHandler->OnNetworkError("inet_pton failed for listen IP", errorCode);
                closesocket(m_socket); m_socket = INVALID_SOCKET;
                WSACleanup();
                return false;
            }

            if (bind(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                int errorCode = WSAGetLastError();
                RF_NETWORK_CRITICAL("UDPSocketAsync: bind() failed with error: {}", errorCode);
                m_eventHandler->OnNetworkError("bind failed", errorCode);
                closesocket(m_socket); m_socket = INVALID_SOCKET;
                WSACleanup();
                return false;
            }
            RF_NETWORK_INFO("UDPSocketAsync: Socket bound successfully to {}:{}", m_listenIp, m_listenPort);

            m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
            if (m_iocpHandle == NULL) {
                int errorCode = GetLastError();
                RF_NETWORK_CRITICAL("UDPSocketAsync: CreateIoCompletionPort (for IOCP itself) failed with error: {}", errorCode);
                m_eventHandler->OnNetworkError("CreateIoCompletionPort failed (IOCP handle)", errorCode);
                closesocket(m_socket); m_socket = INVALID_SOCKET;
                WSACleanup();
                return false;
            }
            RF_NETWORK_DEBUG("UDPSocketAsync: IOCP created successfully.");

            if (CreateIoCompletionPort((HANDLE)m_socket, m_iocpHandle, (ULONG_PTR)0, 0) == NULL) {
                int errorCode = GetLastError();
                RF_NETWORK_CRITICAL("UDPSocketAsync: CreateIoCompletionPort (associating socket) failed with error: {}", errorCode);
                m_eventHandler->OnNetworkError("CreateIoCompletionPort failed (socket association)", errorCode);
                CloseHandle(m_iocpHandle); m_iocpHandle = NULL;
                closesocket(m_socket); m_socket = INVALID_SOCKET;
                WSACleanup();
                return false;
            }
            RF_NETWORK_DEBUG("UDPSocketAsync: Socket associated with IOCP successfully.");

            try {
                m_receiveContextPool.reserve(MAX_PENDING_RECEIVES_IOCP);
                for (int i = 0; i < MAX_PENDING_RECEIVES_IOCP; ++i) {
                    // Assuming DEFAULT_UDP_BUFFER_SIZE_IOCP is a const member or accessible static const
                    m_receiveContextPool.emplace_back(std::make_unique<OverlappedIOContext>(IOOperationType::Recv, DEFAULT_UDP_BUFFER_SIZE_IOCP));
                    m_freeReceiveContexts.push_back(m_receiveContextPool.back().get());
                }
                RF_NETWORK_INFO("UDPSocketAsync: Receive context pool initialized with {} contexts.", m_freeReceiveContexts.size());
            }
            catch (const std::bad_alloc& e) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: Failed to allocate memory for receive context pool: {}", e.what());
                m_eventHandler->OnNetworkError("Failed to allocate receive context pool");
                if (m_iocpHandle) { CloseHandle(m_iocpHandle); m_iocpHandle = NULL; }
                if (m_socket != INVALID_SOCKET) { closesocket(m_socket); m_socket = INVALID_SOCKET; }
                WSACleanup();
                return false;
            }

            RF_NETWORK_INFO("UDPSocketAsync: Initialization successful.");
            return true;
        }

        bool UDPSocketAsync::Start() {
            if (m_socket == INVALID_SOCKET || m_iocpHandle == NULL) {
                RF_NETWORK_ERROR("UDPSocketAsync: Cannot start. Socket not initialized or IOCP handle is null.");
                return false;
            }
            if (!m_eventHandler) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: Cannot start. Event handler is null (was Init called and successful?).");
                return false;
            }
            if (m_isRunning.load(std::memory_order_relaxed)) {
                RF_NETWORK_WARN("UDPSocketAsync: Already running.");
                return true; // Or false if starting again is an error
            }

            RF_NETWORK_INFO("UDPSocketAsync: Starting...");
            m_isRunning = true; // Set before starting threads and posting receives

            m_workerThreads.reserve(NUM_WORKER_THREADS_TO_CREATE);
            for (unsigned int i = 0; i < NUM_WORKER_THREADS_TO_CREATE; ++i) {
                try {
                    m_workerThreads.emplace_back(&UDPSocketAsync::WorkerThread, this);
                }
                catch (const std::system_error& e) {
                    RF_NETWORK_CRITICAL("UDPSocketAsync: Failed to create worker thread {}: {}", i, e.what());
                    m_eventHandler->OnNetworkError("Failed to create worker thread", i); // Pass thread index or similar context
                    Stop(); // Attempt to clean up what has been done
                    return false;
                }
            }
            RF_NETWORK_INFO("UDPSocketAsync: {} worker threads created.", m_workerThreads.size());

            // REMOVED: ReliabilityManagementThread creation

            int successfullyPosted = 0;
            for (int i = 0; i < MAX_PENDING_RECEIVES_IOCP; ++i) {
                OverlappedIOContext* pContext = GetFreeReceiveContextInternal();
                if (!pContext) {
                    RF_NETWORK_ERROR("UDPSocketAsync: Start - Failed to get free receive context for initial post {}.", i);
                    break; // No more contexts to post
                }
                if (!PostReceiveInternal(pContext)) {
                    RF_NETWORK_ERROR("UDPSocketAsync: Start - Failed to post initial receive operation {}. Error: {}", i, WSAGetLastError());
                    ReturnReceiveContextInternal(pContext); // Return context on failure
                }
                else {
                    successfullyPosted++;
                }
            }

            if (successfullyPosted == 0 && MAX_PENDING_RECEIVES_IOCP > 0) {
                RF_NETWORK_CRITICAL("UDPSocketAsync: CRITICAL - Failed to post ANY initial receive operations.");
                m_eventHandler->OnNetworkError("Failed to post any initial receive operations");
                Stop(); // Attempt to clean up
                return false;
            }
            RF_NETWORK_INFO("UDPSocketAsync: Successfully posted {} initial receive operations. Server is listening.", successfullyPosted);
            return true;
        }

        void UDPSocketAsync::Stop() {
            if (!m_isRunning.exchange(false, std::memory_order_acq_rel)) { // Acquire-release for synchronization
                RF_NETWORK_INFO("UDPSocketAsync: Stop called but already not running or stop initiated.");
                return;
            }
            RF_NETWORK_INFO("UDPSocketAsync: Stopping...");

            // REMOVED: ReliabilityManagementThread joining

            // Signal worker threads to exit by posting completion statuses
            if (m_iocpHandle != NULL) { // Check if IOCP handle is valid
                for (size_t i = 0; i < m_workerThreads.size(); ++i) {
                    PostQueuedCompletionStatus(m_iocpHandle, 0, 0, NULL);
                }
                RF_NETWORK_DEBUG("UDPSocketAsync: Shutdown signals posted to IOCP for {} worker threads.", m_workerThreads.size());
            }


            // Close the socket to interrupt any pending operations blocking in WSARecvFrom/WSASendTo
            // (though with IOCP, operations should complete with an error)
            if (m_socket != INVALID_SOCKET) {
                SOCKET tempSock = m_socket; // Store for use after invalidating member
                m_socket = INVALID_SOCKET;  // Mark as invalid to prevent further use
                // Shutdown can help unblock threads, though closesocket is often sufficient for UDP here
                // For UDP, shutdown might not be as critical as for TCP, but it doesn't hurt.
                shutdown(tempSock, SD_BOTH);
                closesocket(tempSock);
                RF_NETWORK_INFO("UDPSocketAsync: Socket closed.");
            }

            RF_NETWORK_INFO("UDPSocketAsync: Joining worker threads...");
            for (auto& thread : m_workerThreads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            m_workerThreads.clear();
            RF_NETWORK_INFO("UDPSocketAsync: All worker threads joined.");

            if (m_iocpHandle != NULL) { // Check again before closing
                CloseHandle(m_iocpHandle);
                m_iocpHandle = NULL;
                RF_NETWORK_INFO("UDPSocketAsync: IOCP handle closed.");
            }

            // REMOVED: Clearing of reliability-related maps

            // Clear receive context pool
            {
                std::lock_guard<std::mutex> lock(m_receiveContextMutex);
                // Contexts in m_freeReceiveContexts are raw pointers to objects owned by m_receiveContextPool.
                // Clearing m_receiveContextPool will delete the unique_ptrs, which deletes the objects.
                m_freeReceiveContexts.clear();
            }
            m_receiveContextPool.clear();
            RF_NETWORK_DEBUG("UDPSocketAsync: Receive context pool cleared.");

            WSACleanup();
            RF_NETWORK_INFO("UDPSocketAsync: Stopped successfully.");
            // Optional: if (m_eventHandler) m_eventHandler->OnNetworkIOShutdown();
        }

        // Internal helper, renamed from GetFreeReceiveContext
        OverlappedIOContext* UDPSocketAsync::GetFreeReceiveContextInternal() {
            std::lock_guard<std::mutex> lock(m_receiveContextMutex);
            if (m_freeReceiveContexts.empty()) {
                RF_NETWORK_WARN("UDPSocketAsync: No free receive contexts available in pool.");
                // Potentially create a new one on-demand if design allows, or handle exhaustion.
                // For now, returns nullptr as per original logic.
                return nullptr;
            }
            OverlappedIOContext* pContext = m_freeReceiveContexts.front();
            m_freeReceiveContexts.pop_front();
            return pContext;
        }

        // Internal helper, renamed from ReturnReceiveContext
        void UDPSocketAsync::ReturnReceiveContextInternal(OverlappedIOContext* pContext) {
            if (!pContext) return;
            std::lock_guard<std::mutex> lock(m_receiveContextMutex);
            m_freeReceiveContexts.push_back(pContext);
        }


        // Internal helper, renamed from PostReceive
        bool UDPSocketAsync::PostReceiveInternal(OverlappedIOContext* pRecvContext) {
            if (!pRecvContext) {
                RF_NETWORK_CRITICAL("UDPSocketAsync::PostReceiveInternal: ERROR - pRecvContext is null.");
                return false;
            }
            if (m_socket == INVALID_SOCKET) { // Check if socket is still valid
                RF_NETWORK_ERROR("UDPSocketAsync::PostReceiveInternal: ERROR - Invalid socket, cannot post receive.");
                // Potentially return the context to the pool if it cannot be posted
                ReturnReceiveContextInternal(pRecvContext);
                return false;
            }

            pRecvContext->ResetForReceive(); // Ensure context is pristine for a new receive

            DWORD dwFlags = 0; // Must be 0 for WSARecvFrom with UDP
            int result = WSARecvFrom(
                m_socket,
                &(pRecvContext->wsaBuf),
                1,                       // Number of WSABUF structures
                NULL,                    // lpNumberOfBytesRecvd - for synchronous, NULL for overlapped
                &dwFlags,                // lpFlags - used to modify behavior, usually 0 for UDP
                (SOCKADDR*)&(pRecvContext->remoteAddrNative),
                &(pRecvContext->remoteAddrNativeLen),
                &(pRecvContext->overlapped),
                NULL                     // lpCompletionRoutine - NULL for IOCP
            );

            if (result == SOCKET_ERROR) {
                int errorCode = WSAGetLastError();
                if (errorCode != WSA_IO_PENDING) {
                    RF_NETWORK_ERROR("UDPSocketAsync::PostReceiveInternal: WSARecvFrom failed immediately with error: {} for context: {}", errorCode, (void*)pRecvContext);
                    // The GetQueuedCompletionStatus will pick up this error for this context.
                    // However, the context might not be automatically re-queued by GQCS handling if it fails here.
                    // Consider returning it to the pool or attempting a re-post later if appropriate.
                    // For now, let GQCS error handling manage the context.
                    // If this PostReceiveInternal fails, the context is NOT yet associated with an IOCP operation that will complete.
                    // So, if it's not WSA_IO_PENDING, it means the post itself failed.
                    ReturnReceiveContextInternal(pRecvContext); // Return context if the post itself fails
                    return false;
                }
                // RF_NETWORK_TRACE("UDPSocketAsync::PostReceiveInternal: WSARecvFrom pending for context {}", (void*)pRecvContext);
            }
            else {
                // RF_NETWORK_TRACE("UDPSocketAsync::PostReceiveInternal: WSARecvFrom completed immediately for context {}", (void*)pRecvContext);
                // Even if it completes immediately, IOCP will queue a completion packet.
            }
            return true;
        }

        void UDPSocketAsync::WorkerThread() {
            std::ostringstream oss_thread_id_start;
            oss_thread_id_start << std::this_thread::get_id();
            RF_NETWORK_INFO("UDPSocketAsync: Worker thread started (ID: {})", oss_thread_id_start.str());

            OverlappedIOContext* pIoContext = nullptr;
            DWORD bytesTransferred = 0;
            ULONG_PTR completionKey = 0; // Not heavily used for single-socket IOCP unless differentiating operations

            while (m_isRunning.load(std::memory_order_acquire)) { // Acquire ensures visibility of m_isRunning
                pIoContext = nullptr;
                bytesTransferred = 0;

                BOOL bSuccess = GetQueuedCompletionStatus(
                    m_iocpHandle,
                    &bytesTransferred,
                    &completionKey,
                    (LPOVERLAPPED*)&pIoContext, // Cast to LPOVERLAPPED*
                    100 // Timeout in ms to allow checking m_isRunning periodically
                );

                // For detailed logging, construct thread ID string if needed
                // std::string current_tid_str; 
                // #ifdef RF_DETAILED_LOGGING
                // std::ostringstream current_tid_oss; current_tid_oss << std::this_thread::get_id(); current_tid_str = current_tid_oss.str();
                // #endif

                if (!bSuccess) { // GetQueuedCompletionStatus failed
                    DWORD errorCode = GetLastError();
                    if (errorCode == WAIT_TIMEOUT) {
                        continue; // Expected timeout, loop to check m_isRunning
                    }

                    // An actual error occurred on GetQueuedCompletionStatus
                    // RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread {} - GetQueuedCompletionStatus returned FALSE. WinError: {}. Context: {}, OpType: {}",
                    //                  current_tid_str, errorCode, (void*)pIoContext, (pIoContext ? static_cast<int>(pIoContext->operationType) : -1));

                    if (pIoContext != NULL) { // If context is available, an operation failed
                        if (pIoContext->operationType == IOOperationType::Recv) {
                            RF_NETWORK_WARN("WorkerThread: Failed Recv Op in GQCS. Error: {}. Context {}", errorCode, (void*)pIoContext);
                            // Attempt to re-post the receive operation if server is still running
                            if (m_isRunning.load(std::memory_order_relaxed) && PostReceiveInternal(pIoContext)) {
                                // Successfully re-posted
                            }
                            else {
                                RF_NETWORK_CRITICAL("WorkerThread: CRITICAL - Failed to re-post Recv context {} after I/O error {} or server stopping. Returning to pool.", (void*)pIoContext, errorCode);
                                ReturnReceiveContextInternal(pIoContext); // Return to pool if cannot re-post
                            }
                        }
                        else if (pIoContext->operationType == IOOperationType::Send) {
                            RF_NETWORK_ERROR("WorkerThread: Failed Send Op in GQCS. Error: {}. Context {}", errorCode, (void*)pIoContext);
                            if (m_eventHandler) m_eventHandler->OnSendCompleted(pIoContext, false, 0);
                            delete pIoContext; // Send contexts are heap-allocated
                        }
                        else {
                            RF_NETWORK_ERROR("WorkerThread: Unknown operation type in failed GQCS. Context {}", (void*)pIoContext);
                            // Decide how to handle unknown context types on error - potentially delete if heap allocated.
                        }
                        pIoContext = nullptr; // Context has been handled
                    }
                    else { // pIoContext is NULL - GQCS failed without a context (e.g., IOCP handle closed)
                        RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread - GetQueuedCompletionStatus failed without a context. WinError: {}. Assuming shutdown.", errorCode);
                        if (m_eventHandler) m_eventHandler->OnNetworkError("GQCS failed without context", errorCode);
                        // This often indicates the IOCP port itself is being closed, so the thread should exit.
                        if (!m_isRunning.load(std::memory_order_relaxed)) { // Double check if we are explicitly stopping
                            break; // Exit loop if server is stopping
                        }
                        // If server is supposedly running but this happens, it's a critical error.
                        // Depending on the error, might need to break or log and continue cautiously.
                        // For ERROR_ABANDONED_WAIT_0 or ERROR_INVALID_HANDLE, definitely break.
                        if (errorCode == ERROR_ABANDONED_WAIT_0 || errorCode == ERROR_INVALID_HANDLE) {
                            break;
                        }
                    }
                    continue; // Continue to next iteration after handling error
                }

                // GetQueuedCompletionStatus succeeded (bSuccess is TRUE)
                if (pIoContext == NULL) {
                    // This is an explicit shutdown signal posted by Stop() (PostQueuedCompletionStatus with NULL context)
                    RF_NETWORK_INFO("UDPSocketAsync: WorkerThread received NULL context (explicit shutdown signal). Exiting.");
                    break; // Exit the while loop
                }

                // Successful I/O operation has completed
                switch (pIoContext->operationType) {
                case IOOperationType::Recv:
                {
                    if (bytesTransferred > 0) {
                        NetworkEndpoint sender_endpoint;
                        char senderIpBuffer[INET_ADDRSTRLEN]; // Ensure INET_ADDRSTRLEN is sufficient

                        if (inet_ntop(AF_INET, &(pIoContext->remoteAddrNative.sin_addr), senderIpBuffer, INET_ADDRSTRLEN)) {
                            sender_endpoint.ipAddress = senderIpBuffer;
                            sender_endpoint.port = ntohs(pIoContext->remoteAddrNative.sin_port);

                            // RF_NETWORK_TRACE("WorkerThread {}: Received {} bytes from {}", current_tid_str, bytesTransferred, sender_endpoint.ToString());

                            // --- HAND OFF TO EVENT HANDLER (PacketHandler) ---
                            if (m_eventHandler) {
                                m_eventHandler->OnRawDataReceived(sender_endpoint,
                                    reinterpret_cast<const uint8_t*>(pIoContext->buffer.data()),
                                    bytesTransferred,
                                    pIoContext); // Pass context for informational purposes
                            }
                            // --- ALL FORMER PACKET/MESSAGE PROCESSING LOGIC IS REMOVED FROM HERE ---

                        }
                        else { // inet_ntop failed
                            int ntopErrorCode = WSAGetLastError(); // Or other error code depending on inet_ntop variant
                            RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread - inet_ntop failed for received packet. Error: {}", ntopErrorCode);
                            if (m_eventHandler) m_eventHandler->OnNetworkError("inet_ntop failed", ntopErrorCode);
                        }
                    }
                    else if (bytesTransferred == 0) {
                        // For UDP, receiving 0 bytes is unusual but possible if an empty datagram was sent.
                        // For TCP, 0 bytes means graceful disconnect.
                        RF_NETWORK_WARN("UDPSocketAsync: WorkerThread - Received 0 bytes on a Recv operation (UDP). Context: {}", (void*)pIoContext);
                        // Still, pass it to the handler, it might be a keep-alive or special signal.
                        // Or, if 0-byte datagrams are always invalid, handle here. For now, assume handler might want it.
                        NetworkEndpoint sender_endpoint; // Still need to identify sender if possible
                        char senderIpBuffer[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &(pIoContext->remoteAddrNative.sin_addr), senderIpBuffer, INET_ADDRSTRLEN)) {
                            sender_endpoint.ipAddress = senderIpBuffer;
                            sender_endpoint.port = ntohs(pIoContext->remoteAddrNative.sin_port);
                            if (m_eventHandler) {
                                m_eventHandler->OnRawDataReceived(sender_endpoint, nullptr, 0, pIoContext);
                            }
                        }
                    }
                    // else: bytesTransferred < 0 is not possible from GQCS (DWORD)

                    // Always re-post the receive context if the server is running
                    if (m_isRunning.load(std::memory_order_relaxed)) {
                        if (!PostReceiveInternal(pIoContext)) {
                            RF_NETWORK_CRITICAL("UDPSocketAsync: WorkerThread - CRITICAL: Failed to re-post WSARecvFrom. Context: {}. Error: {}",
                                (void*)pIoContext, WSAGetLastError());
                            ReturnReceiveContextInternal(pIoContext); // Return to pool if cannot re-post
                        }
                        else {
                            //RF_NETWORK_TRACE("WorkerThread {}: Successfully re-posted Recv context {}", current_tid_str, (void*)pIoContext);
                        }
                    }
                    else { // Server is stopping
                        ReturnReceiveContextInternal(pIoContext);
                    }
                    pIoContext = nullptr; // Context has been handled (re-posted or returned)
                    break;
                } // End of case IOOperationType::Recv

                case IOOperationType::Send:
                {
                    //RF_NETWORK_TRACE("UDPSocketAsync: WorkerThread {} - Send operation completed. Context: {}, Bytes: {}",
                                      //current_tid_str, (void*)pIoContext, bytesTransferred);
                    if (m_eventHandler) {
                        // Pass true for success, as bSuccess was true to get here.
                        // bytesTransferred from GQCS is the actual number of bytes sent.
                        m_eventHandler->OnSendCompleted(pIoContext, true, bytesTransferred);
                    }
                    delete pIoContext; // Send contexts are allocated with new, delete after completion
                    pIoContext = nullptr;
                    break;
                } // End of case IOOperationType::Send

                default:
                    RF_NETWORK_ERROR("UDPSocketAsync: WorkerThread - Dequeued completed op with Unknown/None type. Context: {}, OpType: {}",
                        (void*)pIoContext, (pIoContext ? static_cast<int>(pIoContext->operationType) : -1));
                    if (m_eventHandler) m_eventHandler->OnNetworkError("Unknown operation type dequeued", (pIoContext ? static_cast<int>(pIoContext->operationType) : -1));
                    // If context is not null and not a send context, how to clean it up?
                    // This case should ideally not be reached if opType is always set correctly.
                    if (pIoContext) {
                        // Assuming unknown contexts might be dynamic like send contexts for safety,
                        // or if they were from a pool, we need a way to return them.
                        // Since Recv contexts are pooled and Send are new'd, an unknown type here is problematic.
                        RF_NETWORK_ERROR("WorkerThread: Deleting unexpected context {} due to unknown type.", (void*)pIoContext);
                        delete pIoContext;
                        pIoContext = nullptr;
                    }
                    break;
                } // End switch on operationType
            } // End while(m_isRunning)

            std::ostringstream exit_tid_oss;
            exit_tid_oss << std::this_thread::get_id();
            RF_NETWORK_INFO("UDPSocketAsync: Worker thread {} exiting gracefully.", exit_tid_oss.str());
        }


        // This is the implementation of INetworkIO::SendData
        // It was formerly SendRawTo, signature and some logic adapted.
        bool UDPSocketAsync::SendData(const NetworkEndpoint& recipient, const uint8_t* data, uint32_t size) {
            if (m_socket == INVALID_SOCKET) {
                RF_NETWORK_ERROR("UDPSocketAsync::SendData: Socket not valid. Cannot send to {}.", recipient.ToString());
                return false;
            }
            if (size == 0 || !data) {
                RF_NETWORK_ERROR("UDPSocketAsync::SendData: Invalid data or size ({}) for sending to {}.", size, recipient.ToString());
                // Sending 0-byte UDP datagrams is possible, so allow if intended. For now, let's assume size > 0.
                // If size == 0 is valid, remove this check or adjust.
                if (size == 0) {
                    RF_NETWORK_WARN("UDPSocketAsync::SendData: Attempting to send 0 bytes to {}. Proceeding if this is intentional.", recipient.ToString());
                }
                else if (!data) { // data is null but size > 0
                    RF_NETWORK_ERROR("UDPSocketAsync::SendData: Data is null but size {} > 0 for sending to {}.", size, recipient.ToString());
                    return false;
                }
            }

            // Create a new OverlappedIOContext for each send operation.
            // This context will be deleted by the worker thread when the send completes.
            OverlappedIOContext* sendContext = nullptr;
            try {
                sendContext = new OverlappedIOContext(IOOperationType::Send, static_cast<size_t>(size));
            }
            catch (const std::bad_alloc& e) {
                RF_NETWORK_CRITICAL("UDPSocketAsync::SendData: Failed to allocate memory for send context to {}: {}", recipient.ToString(), e.what());
                return false;
            }

            if (size > 0) { // Only copy if there's data to copy
                memcpy(sendContext->buffer.data(), data, size);
            }
            sendContext->wsaBuf.len = size; // wsaBuf.buf already points to sendContext->buffer.data()

            // Set up recipient address
            sendContext->remoteAddrNative.sin_family = AF_INET;
            sendContext->remoteAddrNative.sin_port = htons(recipient.port);
            if (inet_pton(AF_INET, recipient.ipAddress.c_str(), &(sendContext->remoteAddrNative.sin_addr)) != 1) {
                RF_NETWORK_ERROR("UDPSocketAsync::SendData: inet_pton failed for IP {} to {}. Error: {}", recipient.ipAddress, recipient.ToString(), WSAGetLastError());
                delete sendContext;
                return false;
            }
            sendContext->remoteAddrNativeLen = sizeof(sockaddr_in);


            RF_NETWORK_TRACE("UDPSocketAsync::SendData: Attempting WSASendTo {} bytes to {}", size, recipient.ToString());

            int result = WSASendTo(m_socket, &(sendContext->wsaBuf), 1, NULL, 0,
                (SOCKADDR*)&(sendContext->remoteAddrNative), sendContext->remoteAddrNativeLen,
                &(sendContext->overlapped), NULL);

            if (result == SOCKET_ERROR) {
                int errorCode = WSAGetLastError();
                if (errorCode != WSA_IO_PENDING) {
                    RF_NETWORK_ERROR("UDPSocketAsync::SendData: WSASendTo failed immediately to {} with error: {}", recipient.ToString(), errorCode);
                    // Notify handler of failed send attempt
                    if (m_eventHandler) m_eventHandler->OnSendCompleted(sendContext, false, 0); // Pass context
                    delete sendContext;
                    return false;
                }
                RF_NETWORK_TRACE("UDPSocketAsync::SendData: WSASendTo pending for {}", recipient.ToString());
            }
            else {
                RF_NETWORK_TRACE("UDPSocketAsync::SendData: WSASendTo completed immediately for {}", recipient.ToString());
                // If WSASendTo completes immediately, a completion packet is still queued to the IOCP
                // if the socket is associated with it. The worker thread will handle its deletion via OnSendCompleted.
            }
            return true;
        }


    } // namespace Networking
} // namespace RiftForged