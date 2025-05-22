#include "UDPSocketAsync.h"
#include <iostream> // Replace with your actual logging system
#include <stdexcept> // For std::system_error
#include <vector>
#include <cstring>

// For GetGamePacketHeaderSize, MessageType - ensure path is correct
#include "GamePacketHeader.h" 
#include "../Gameplay/PlayerManager.h" // For PlayerManager

// MAX_WORKER_THREADS can be defined here, or taken from config, or based on hardware.
// For simplicity, let's use hardware_concurrency.
// If you #define it in a header, ensure that header is included.
unsigned int NUM_WORKER_THREADS_TO_CREATE = std::thread::hardware_concurrency(); // Define globally or pass as param
//const unsigned int NUM_WORKER_THREADS_TO_CREATE = 6;

namespace RiftForged {
    namespace Networking {

        UDPSocketAsync::UDPSocketAsync(RiftForged::GameLogic::PlayerManager& playerManager,
            PacketProcessor& packetProcessor,
            std::string listenIp, uint16_t listenPort)
            : m_playerManager(playerManager), // Initialize PlayerManager
            m_packetProcessor(packetProcessor),
            m_listenIp(std::move(listenIp)),
            m_listenPort(listenPort),
            m_socket(INVALID_SOCKET),
            m_iocpHandle(NULL),
            m_isRunning(false) {
            std::cout << "UDPSocketAsync: Constructor called for " << m_listenIp << ":" << m_listenPort << std::endl;
        }

        UDPSocketAsync::~UDPSocketAsync() {
            std::cout << "UDPSocketAsync: Destructor called. Attempting to stop..." << std::endl;
            Stop();
        }

        bool UDPSocketAsync::Init() {
            std::cout << "UDPSocketAsync: Initializing..." << std::endl;
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                std::cerr << "UDPSocketAsync: WSAStartup failed with error: " << result << std::endl;
                return false;
            }
            std::cout << "UDPSocketAsync: WSAStartup successful." << std::endl;

            m_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
            if (m_socket == INVALID_SOCKET) {
                std::cerr << "UDPSocketAsync: WSASocket() failed with error: " << WSAGetLastError() << std::endl;
                WSACleanup();
                return false;
            }
            std::cout << "UDPSocketAsync: Socket created successfully (Socket ID: " << m_socket << ")." << std::endl;

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(m_listenPort);
            if (inet_pton(AF_INET, m_listenIp.c_str(), &serverAddr.sin_addr) != 1) {
                std::cerr << "UDPSocketAsync: inet_pton failed for IP " << m_listenIp << ". Error: " << WSAGetLastError() << std::endl;
                closesocket(m_socket); WSACleanup(); return false;
            }

            if (bind(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                std::cerr << "UDPSocketAsync: bind() failed with error: " << WSAGetLastError() << std::endl;
                closesocket(m_socket); WSACleanup(); return false;
            }
            std::cout << "UDPSocketAsync: Socket bound successfully to " << m_listenIp << ":" << m_listenPort << std::endl;

            m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
            if (m_iocpHandle == NULL) {
                std::cerr << "UDPSocketAsync: CreateIoCompletionPort (for IOCP itself) failed with error: " << GetLastError() << std::endl;
                closesocket(m_socket); WSACleanup(); return false;
            }
            std::cout << "UDPSocketAsync: IOCP created successfully." << std::endl;

            if (CreateIoCompletionPort((HANDLE)m_socket, m_iocpHandle, (ULONG_PTR)0, 0) == NULL) {
                std::cerr << "UDPSocketAsync: CreateIoCompletionPort (associating socket) failed with error: " << GetLastError() << std::endl;
                CloseHandle(m_iocpHandle); m_iocpHandle = NULL;
                closesocket(m_socket); WSACleanup(); return false;
            }
            std::cout << "UDPSocketAsync: Socket associated with IOCP successfully." << std::endl;

            try {
                m_receiveContextPool.reserve(MAX_PENDING_RECEIVES_IOCP);
                for (int i = 0; i < MAX_PENDING_RECEIVES_IOCP; ++i) {
                    m_receiveContextPool.emplace_back(std::make_unique<OverlappedIOContext>(IOOperationType::Recv, DEFAULT_UDP_BUFFER_SIZE_IOCP));
                    m_freeReceiveContexts.push_back(m_receiveContextPool.back().get());
                }
                std::cout << "UDPSocketAsync: Receive context pool initialized with " << m_freeReceiveContexts.size() << " contexts." << std::endl;
            }
            catch (const std::bad_alloc& e) {
                std::cerr << "UDPSocketAsync: Failed to allocate memory for receive context pool: " << e.what() << std::endl;
                if (m_iocpHandle) CloseHandle(m_iocpHandle); m_iocpHandle = NULL;
                closesocket(m_socket); WSACleanup(); return false;
            }

            std::cout << "UDPSocketAsync: Initialization successful." << std::endl;
            return true;
        }

        bool UDPSocketAsync::Start() {
            if (m_socket == INVALID_SOCKET || m_iocpHandle == NULL) {
                std::cerr << "UDPSocketAsync: Cannot start. Socket not initialized or IOCP handle is null." << std::endl;
                return false;
            }
            if (m_isRunning.load()) {
                std::cout << "UDPSocketAsync: Already running." << std::endl;
                return true;
            }

            std::cout << "UDPSocketAsync: Starting..." << std::endl;
            m_isRunning = true;

            unsigned int numThreadsToActualStart = NUM_WORKER_THREADS_TO_CREATE; // Use the const defined at top of file or from config
            if (numThreadsToActualStart == 0) numThreadsToActualStart = 2; // Fallback

            m_workerThreads.reserve(numThreadsToActualStart);
            for (unsigned int i = 0; i < numThreadsToActualStart; ++i) {
                try {
                    m_workerThreads.emplace_back(&UDPSocketAsync::WorkerThread, this);
                }
                catch (const std::system_error& e) {
                    std::cerr << "UDPSocketAsync: Failed to create worker thread " << i << ": " << e.what() << std::endl;
                    Stop(); return false;
                }
            }
            std::cout << "UDPSocketAsync: " << m_workerThreads.size() << " worker threads created." << std::endl;

            int successfullyPosted = 0;
            for (int i = 0; i < MAX_PENDING_RECEIVES_IOCP; ++i) {
                OverlappedIOContext* pContext = GetFreeReceiveContext();
                if (!pContext) {
                    std::cerr << "UDPSocketAsync: Start - Failed to get free receive context for initial post " << i << "." << std::endl;
                    break;
                }
                if (!PostReceive(pContext)) {
                    std::cerr << "UDPSocketAsync: Start - Failed to post initial receive operation " << i << ". Error: " << WSAGetLastError() << std::endl;
                    ReturnReceiveContext(pContext);
                }
                else {
                    successfullyPosted++;
                }
            }

            if (successfullyPosted == 0 && MAX_PENDING_RECEIVES_IOCP > 0) {
                std::cerr << "UDPSocketAsync: CRITICAL - Failed to post ANY initial receive operations." << std::endl;
                Stop(); return false;
            }
            std::cout << "UDPSocketAsync: Successfully posted " << successfullyPosted << " initial receive operations. Server is listening." << std::endl;
            return true;
        }

        void UDPSocketAsync::Stop() {
            if (!m_isRunning.exchange(false)) { return; } // Ensure stop logic runs only once
            std::cout << "UDPSocketAsync: Stopping..." << std::endl;

            if (m_iocpHandle) {
                for (size_t i = 0; i < m_workerThreads.size(); ++i) { // Signal each thread
                    PostQueuedCompletionStatus(m_iocpHandle, 0, 0, NULL);
                }
            }
            std::cout << "UDPSocketAsync: Shutdown signals posted." << std::endl;

            if (m_socket != INVALID_SOCKET) {
                SOCKET tempSock = m_socket;
                m_socket = INVALID_SOCKET; // Prevent further use by worker threads quickly
                shutdown(tempSock, SD_BOTH);
                closesocket(tempSock);
                std::cout << "UDPSocketAsync: Socket closed." << std::endl;
            }

            std::cout << "UDPSocketAsync: Joining worker threads..." << std::endl;
            for (auto& thread : m_workerThreads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            m_workerThreads.clear();
            std::cout << "UDPSocketAsync: All worker threads joined." << std::endl;

            if (m_iocpHandle) {
                CloseHandle(m_iocpHandle);
                m_iocpHandle = NULL;
                std::cout << "UDPSocketAsync: IOCP handle closed." << std::endl;
            }

            { // Explicitly clear the free list
                std::lock_guard<std::mutex> lock(m_receiveContextMutex);
                m_freeReceiveContexts.clear();
            }
            m_receiveContextPool.clear(); // unique_ptrs will handle deletion

            WSACleanup();
            std::cout << "UDPSocketAsync: Stopped successfully." << std::endl;
        }

        OverlappedIOContext* UDPSocketAsync::GetFreeReceiveContext() {
            std::lock_guard<std::mutex> lock(m_receiveContextMutex);
            if (m_freeReceiveContexts.empty()) {
                std::cerr << "UDPSocketAsync: No free receive contexts available in pool." << std::endl;
                return nullptr;
            }
            OverlappedIOContext* pContext = m_freeReceiveContexts.front();
            m_freeReceiveContexts.pop_front();
            // ResetForReceive is called in PostReceive before WSARecvFrom
            return pContext;
        }

        void UDPSocketAsync::ReturnReceiveContext(OverlappedIOContext* pContext) {
            if (!pContext) return;
            // Optionally, do a light reset here if not done elsewhere before pushing
            // pContext->ResetForReceive(); // Or ensure PostReceive always does it
            std::lock_guard<std::mutex> lock(m_receiveContextMutex);
            m_freeReceiveContexts.push_back(pContext);
        }

        // In UDPSocketAsync.cpp

        bool UDPSocketAsync::PostReceive(OverlappedIOContext* pRecvContext) {
            if (!pRecvContext) {
                std::cerr << "UDPSocketAsync::PostReceive: ERROR - pRecvContext is null." << std::endl;
                return false;
            }
            if (m_socket == INVALID_SOCKET) {
                std::cerr << "UDPSocketAsync::PostReceive: ERROR - Invalid socket, cannot post receive." << std::endl;
                // Do not return context here, the caller (WorkerThread or Start) should handle it if this fails.
                return false;
            }

            // Call ResetForReceive to prepare the context for a new WSARecvFrom operation
            pRecvContext->ResetForReceive();
            // Now pRecvContext->overlapped is zeroed, pRecvContext->remoteAddrNativeLen is set, etc.

            DWORD dwFlags = 0; // For WSARecvFrom, flags are usually 0 on input for basic receive
            int result = WSARecvFrom(
                m_socket,
                &(pRecvContext->wsaBuf),     // Pointer to an array of WSABUF structures
                1,                           // Number of WSABUF structures in the array
                NULL,                        // lpNumberOfBytesRecvd - NULL for overlapped, get from GQCS
                &dwFlags,                    // (Input/Output) Pointer to flags. 
                (SOCKADDR*)&(pRecvContext->remoteAddrNative), // (Output) Pointer to buffer to store source address
                &(pRecvContext->remoteAddrNativeLen),      // (Input/Output) Pointer to size of source address buffer
                &(pRecvContext->overlapped), // Pointer to an OVERLAPPED structure
                NULL                         // lpCompletionRoutine - NULL for IOCP
            );

            if (result == SOCKET_ERROR) {
                int errorCode = WSAGetLastError();
                if (errorCode != WSA_IO_PENDING) {
                    std::cerr << "UDPSocketAsync::PostReceive: WSARecvFrom failed immediately with error: " << errorCode
                        << " for context: " << pRecvContext << std::endl;
                    return false; // The operation didn't even queue
                }
                // WSA_IO_PENDING is expected: operation successfully queued.
            }
            // If result is 0, operation completed immediately (rare for UDP recv, but possible).
            // In both WSA_IO_PENDING and immediate success (0), a completion packet will be queued to IOCP.
            // std::cout << "UDPSocketAsync::PostReceive: Successfully posted WSARecvFrom for context " << pRecvContext << std::endl;
            return true;
        }

        // --- WorkerThread with refined S2C_Response handling and logging ---
        void UDPSocketAsync::WorkerThread() {
            std::cout << "UDPSocketAsync: Worker thread started (ID: " << std::this_thread::get_id() << ")." << std::endl;
            OverlappedIOContext* pIoContext = nullptr;
            DWORD bytesTransferred = 0;
            ULONG_PTR completionKey = 0; // Key associated with the socket if set during CreateIoCompletionPort

            while (true) { // Main loop for the worker thread
                pIoContext = nullptr; // Reset at the start of each loop iteration
                bytesTransferred = 0;
                // completionKey will be set by GetQueuedCompletionStatus

                // Wait for a completion packet
                BOOL bSuccess = GetQueuedCompletionStatus(
                    m_iocpHandle,
                    &bytesTransferred,
                    &completionKey,
                    (LPOVERLAPPED*)&pIoContext, // Receives a pointer to our OverlappedIOContext
                    INFINITE                    // Block indefinitely until an I/O completion or manual posting
                );

                // Primary shutdown check: if m_isRunning is false, we should exit.
                // This catches cases where Stop() was called while this thread was blocked in GQCS.
                if (!m_isRunning.load()) {
                    if (pIoContext) { // An operation completed during shutdown
                        if (pIoContext->operationType == IOOperationType::Recv) {
                            std::cout << "WorkerThread " << std::this_thread::get_id() << ": Returning Recv context " << pIoContext << " during shutdown." << std::endl;
                            ReturnReceiveContext(pIoContext);
                        }
                        else if (pIoContext->operationType == IOOperationType::Send) {
                            std::cout << "WorkerThread " << std::this_thread::get_id() << ": Deleting Send context " << pIoContext << " during shutdown." << std::endl;
                            delete pIoContext;
                        }
                    }
                    std::cout << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " detected m_isRunning is false, exiting." << std::endl;
                    break;
                }

                // Check for explicit shutdown signal from Stop() (NULL context posted)
                if (pIoContext == NULL) {
                    // This is the most reliable signal from Stop() using PostQueuedCompletionStatus(m_iocpHandle, 0, 0, NULL)
                    std::cout << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " received NULL context (explicit shutdown signal). Exiting." << std::endl;
                    break;
                }

                // --- Handle GetQueuedCompletionStatus Failure ---
                if (!bSuccess) {
                    DWORD errorCode = GetLastError(); // Get error *immediately*
                    std::cerr << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id()
                        << " - GetQueuedCompletionStatus returned FALSE. Actual WinError: " << errorCode;
                    if (pIoContext) { // Error is associated with a specific I/O operation
                        std::cerr << " for OpType: " << static_cast<int>(pIoContext->operationType)
                            << ", Context: " << pIoContext;
                    }
                    std::cerr << std::endl;

                    if (pIoContext) {
                        if (pIoContext->operationType == IOOperationType::Recv) {
                            // An existing WSARecvFrom operation failed.
                            // We MUST try to re-post a receive to keep the server listening.
                            // Re-posting the *same* context might be okay if the error was transient.
                            std::cerr << "WorkerThread " << std::this_thread::get_id() << ": Failed Recv Op. Attempting to re-post context " << pIoContext << std::endl;
                            if (!PostReceive(pIoContext)) {
                                std::cerr << "WorkerThread " << std::this_thread::get_id() << ": CRITICAL - Failed to re-post Recv context " << pIoContext << " after I/O error. Returning to pool." << std::endl;
                                ReturnReceiveContext(pIoContext);
                                // At this point, we have one less pending receive operation. This needs monitoring.
                            }
                        }
                        else if (pIoContext->operationType == IOOperationType::Send) {
                            // A WSASendTo operation failed. Clean up its context.
                            std::cerr << "WorkerThread " << std::this_thread::get_id() << ": Failed Send Op. Deleting context " << pIoContext << std::endl;
                            delete pIoContext;
                        }
                    }
                    else if (errorCode == ERROR_ABANDONED_WAIT_0) {
                        // The IOCP handle itself was closed. This worker thread can do nothing more.
                        std::cout << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " - IOCP Handle closed (ERROR_ABANDONED_WAIT_0). Exiting." << std::endl;
                        break;
                    }
                    continue; // Try to get the next completion packet.
                }

                // --- Process Successful I/O Completion ---
                // At this point, bSuccess is TRUE, and pIoContext should be valid and point to our OverlappedIOContext.
                if (pIoContext == nullptr) { // Should have been caught by shutdown signal check, but as a safeguard.
                    std::cerr << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " - GQCS success but pIoContext is NULL (and not shutdown signal). This is unexpected. Continuing." << std::endl;
                    continue;
                }

                NetworkEndpoint current_c2s_sender_endpoint; // Store the sender of the C2S message for this cycle

                switch (pIoContext->operationType) {
                case IOOperationType::Recv:
                { // Scope for local variables
                    bool processed_successfully = false;
                    if (bytesTransferred > 0) {
                        char senderIpBuffer[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &(pIoContext->remoteAddrNative.sin_addr), senderIpBuffer, INET_ADDRSTRLEN)) {
                            current_c2s_sender_endpoint.ipAddress = senderIpBuffer;
                            current_c2s_sender_endpoint.port = ntohs(pIoContext->remoteAddrNative.sin_port);
                            processed_successfully = true; // Successfully got sender info

                            // std::cout << "WorkerThread " << std::this_thread::get_id() << ": Received " << bytesTransferred << " bytes from " << current_c2s_sender_endpoint.ToString() << std::endl;

                            std::optional<S2C_Response> s2c_response =
                                m_packetProcessor.ProcessIncomingRawPacket(
                                    pIoContext->buffer.data(), static_cast<int>(bytesTransferred), current_c2s_sender_endpoint);

                            if (s2c_response.has_value()) {
                                std::cout << "WorkerThread " << std::this_thread::get_id()
                                    << ": S2C_Response to send. Broadcast: " << s2c_response->broadcast
                                    << ", Specific Recipient in S2C_Response: [" << s2c_response->specific_recipient.ToString() << "]"
                                    << ", Original C2S Sender: [" << current_c2s_sender_endpoint.ToString() << "]"
                                    << ", MsgType: " << static_cast<int>(s2c_response->messageType) << std::endl;

                                GamePacketHeader s2c_header(s2c_response->messageType);
                                // TODO: Populate reliability fields in s2c_header (seq, ack, ack_bits)
                                // This would likely involve m_playerManager or a ReliabilityManager
                                // to get next outgoing sequence for target, and current ACK info for target.
                                // s2c_header.sequenceNumber = m_playerManager.GetNextOutgoingSequenceFor(target_endpoint);

                                size_t headerSize = GetGamePacketHeaderSize();
                                std::vector<char> sendPacketBuffer(headerSize + s2c_response->data.size());
                                memcpy(sendPacketBuffer.data(), &s2c_header, headerSize);
                                memcpy(sendPacketBuffer.data() + headerSize, s2c_response->data.data(), s2c_response->data.size());

                                // VVVVVV TEMPORARY MODIFICATION FOR DEBUGGING ERROR 1234 VVVVVV
                                bool force_specific_send_for_this_message_type_test = false;
                                if (s2c_response->messageType == RiftForged::Networking::MessageType::S2C_RiftStepInitiated) {
                                    std::cout << "WORKER_THREAD: [DEBUG] S2C_RiftStepInitiatedMsg was for broadcast, OVERRIDING TO SPECIFIC RECIPIENT (originator) ONLY FOR TEST." << std::endl;
                                    force_specific_send_for_this_message_type_test = true;
                                }
                                // ^^^^^^ END TEMPORARY MODIFICATION FOR DEBUGGING ERROR 1234 ^^^^^^


                                if (s2c_response->broadcast) {
                                    std::vector<NetworkEndpoint> all_clients = m_playerManager.GetAllActiveClientEndpoints();
                                    std::cout << "WorkerThread " << std::this_thread::get_id() << ": Broadcasting S2C MsgType "
                                        << static_cast<int>(s2c_header.messageType) << " to " << all_clients.size() << " client(s)." << std::endl;
                                    if (all_clients.empty()) {
                                        std::cout << "WorkerThread " << std::this_thread::get_id() << ": Broadcast requested, but PlayerManager found no active clients." << std::endl;
                                    }
                                    for (const auto& client_ep : all_clients) {
                                        if (client_ep.ipAddress.empty() || client_ep.port == 0) {
                                            std::cerr << "WorkerThread " << std::this_thread::get_id() << ": BROADCAST SKIPPING INVALID ENDPOINT from PlayerManager: [" << client_ep.ToString() << "]" << std::endl;
                                            continue;
                                        }
                                        // std::cout << "WorkerThread " << std::this_thread::get_id() << ": Broadcasting to: [" << client_ep.ToString() << "]" << std::endl;
                                        if (!SendTo(client_ep, sendPacketBuffer.data(), static_cast<int>(sendPacketBuffer.size()))) {
                                            // SendTo itself logs inet_pton or WSASendTo failures
                                        }
                                    }
                                }
                                else { // Send to specific recipient
                                    if (!s2c_response->specific_recipient.ipAddress.empty() && s2c_response->specific_recipient.port != 0) {
                                        // std::cout << "WorkerThread " << std::this_thread::get_id() << ": Sending S2C to specific: [" << s2c_response->specific_recipient.ToString() << "]" << std::endl;
                                        if (!SendTo(s2c_response->specific_recipient, sendPacketBuffer.data(), static_cast<int>(sendPacketBuffer.size()))) {
                                            // SendTo logs its own failures
                                        }
                                    }
                                    else {
                                        std::cerr << "WorkerThread " << std::this_thread::get_id()
                                            << ": ERROR - S2C_Response specific_recipient is INVALID. Original C2S sender was: ["
                                            << current_c2s_sender_endpoint.ToString() << "]" << std::endl;
                                    }
                                }
                            }
                        }
                        else {
                            std::cerr << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " - inet_ntop failed for received packet. Error: " << WSAGetLastError() << std::endl;
                            processed_successfully = false; // Indicate an issue processing this packet's sender
                        }
                    }
                    else if (bytesTransferred == 0) {
                        std::cout << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " - Received 0 bytes on a Recv operation (UDP)." << std::endl;
                        processed_successfully = true; // 0 bytes is a valid (though unusual) datagram
                    }
                    // Else (bytesTransferred < 0) would be an error caught by !bSuccess

                    // CRITICAL: Re-post this receive context for another operation.
                    // This happens regardless of whether the received data was useful or resulted in an S2C send,
                    // as long as the receive operation itself (GQCS) didn't report a critical error for this context.
                    // std::cout << "WorkerThread " << std::this_thread::get_id() << ": Attempting to re-post Recv context " << pIoContext << std::endl;
                    if (!PostReceive(pIoContext)) {
                        std::cerr << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " - CRITICAL: Failed to re-post WSARecvFrom. Context: "
                            << pIoContext << ". Error Code: " << WSAGetLastError() << std::endl;
                        ReturnReceiveContext(pIoContext);
                    }
                    else {
                        // std::cout << "WorkerThread " << std::this_thread::get_id() << ": Successfully re-posted Recv context " << pIoContext << std::endl;
                    }
                } // End scope for Recv
                break;

                case IOOperationType::Send:
                    // An asynchronous send operation (from UDPSocketAsync::SendTo) has completed.
                    // std::cout << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " - Send operation completed. Context: " << pIoContext << ", Bytes: " << bytesTransferred << std::endl;
                    delete pIoContext; // Context was new'ed in SendTo
                    pIoContext = nullptr;
                    break;

                default: // Should include IOOperationType::None
                    std::cerr << "UDPSocketAsync: WorkerThread " << std::this_thread::get_id() << " - Dequeued completed operation with Unknown or None type. Context: "
                        << pIoContext << ", OpType: " << static_cast<int>(pIoContext->operationType) << std::endl;
                    // This is an unexpected state. Try to determine if it was a receive context to avoid leak.
                    if (pIoContext && pIoContext->operationType != IOOperationType::Send) { // If it's not explicitly a send, assume it might be a miscategorized receive context
                        std::cerr << "WorkerThread " << std::this_thread::get_id() << ": Attempting to return unexpected context " << pIoContext << " as Recv." << std::endl;
                        ReturnReceiveContext(pIoContext);
                    }
                    else if (pIoContext) { // It was identified as send or unknown but not Recv
                        std::cerr << "WorkerThread " << std::this_thread::get_id() << ": Deleting unexpected context " << pIoContext << std::endl;
                        delete pIoContext;
                    }
                    break;
                }
            }
            std::cout << "UDPSocketAsync: Worker thread " << std::this_thread::get_id() << " exiting gracefully." << std::endl;
        }

        bool UDPSocketAsync::SendTo(const NetworkEndpoint& recipient, const char* data, int length) {
            if (m_socket == INVALID_SOCKET /* || !m_isRunning.load() */) { // Allow sends during shutdown? Probably not critical.
                std::cerr << "UDPSocketAsync::SendTo: Socket not valid or server not fully running." << std::endl;
                return false;
            }
            if (length <= 0 || !data) { return false; }

            OverlappedIOContext* sendContext = new (std::nothrow) OverlappedIOContext(IOOperationType::Send, static_cast<size_t>(length));
            if (!sendContext) { std::cerr << "UDPSocketAsync::SendTo: Failed to alloc send context." << std::endl; return false; }

            memcpy(sendContext->buffer.data(), data, length);
            sendContext->wsaBuf.len = length;

            sendContext->remoteAddrNative.sin_family = AF_INET;
            sendContext->remoteAddrNative.sin_port = htons(recipient.port);
            if (inet_pton(AF_INET, recipient.ipAddress.c_str(), &(sendContext->remoteAddrNative.sin_addr)) != 1) {
                std::cerr << "UDPSocketAsync::SendTo: inet_pton failed for IP " << recipient.ipAddress << std::endl;
                delete sendContext; return false;
            }

            int result = WSASendTo(m_socket, &(sendContext->wsaBuf), 1, NULL, 0,
                (SOCKADDR*)&(sendContext->remoteAddrNative), sizeof(sendContext->remoteAddrNative),
                &(sendContext->overlapped), NULL);

            if (result == SOCKET_ERROR) {
                int errorCode = WSAGetLastError();
                if (errorCode != WSA_IO_PENDING) {
                    std::cerr << "UDPSocketAsync::SendTo: WSASendTo failed immediately with error: " << errorCode << std::endl;
                    delete sendContext; return false;
                }
            }
            return true; // Pending or completed immediately
        }

    } // namespace Networking
} // namespace RiftForged