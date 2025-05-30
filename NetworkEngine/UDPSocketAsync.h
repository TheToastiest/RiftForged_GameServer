// Description: Header file for the UDPSocketAsync class, which handles asynchronous UDP socket operations using IOCP.
// This file includes necessary headers, defines constants, and declares the UDPSocketAsync class and its methods.
// Copyright (C) 2023 RiftForged


#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <map>
#include <memory> // For std::unique_ptr
//#include <functional> // Only if you decide to use std::function for callbacks later

// Winsock specific includes
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

// Project-specific includes (adjust paths as necessary if your structure is different)
#include "NetworkEndpoint.h" 
#include "PacketProcessor.h" 
#include "NetworkCommon.h" // For S2C_Response (should include <optional>)
#include "GamePacketHeader.h" // For GamePacketHeader
#include "../Gameplay/PlayerManager.h" // For PlayerManager (if needed in this file)
#include "UDPReliabilityProtocol.h"

// Constants
const int DEFAULT_UDP_BUFFER_SIZE_IOCP = 4096;
const int MAX_PENDING_RECEIVES_IOCP = 200;
const int RELIABILITY_THREAD_SLEEP_MS = 20; // Check for retransmissions/pending ACKs every 20ms
const float DEFAULT_RTO_MS = 200.0f;        // Default Retransmission Timeout
const int DEFAULT_MAX_RETRIES = 5;          // Default max retries for a reliable packet
const int STALE_CONNECTION_TIMEOUT_SECONDS = 60; // Timeout for considering a connection stale (no packets received in this time)

namespace RiftForged {
    namespace Networking {

        enum class IOOperationType {
            None,
            Recv,
            Send
        };

        struct OverlappedIOContext {
            OVERLAPPED      overlapped;
            IOOperationType operationType;
            WSABUF          wsaBuf;
            std::vector<char> buffer;
            sockaddr_in     remoteAddrNative;
            int             remoteAddrNativeLen;

            OverlappedIOContext(IOOperationType opType, size_t bufferSize = DEFAULT_UDP_BUFFER_SIZE_IOCP)
                : operationType(opType), buffer(bufferSize), remoteAddrNativeLen(sizeof(sockaddr_in)) {
                ZeroMemory(&overlapped, sizeof(OVERLAPPED));
                ZeroMemory(&remoteAddrNative, sizeof(sockaddr_in));
                wsaBuf.buf = buffer.data();
                wsaBuf.len = static_cast<ULONG>(buffer.size());
            }

            void ResetForReceive() {
                // CRITICAL: Zero out the OVERLAPPED structure before re-posting a receive
                ZeroMemory(&overlapped, sizeof(OVERLAPPED));
                ZeroMemory(&remoteAddrNative, sizeof(sockaddr_in));
                operationType = IOOperationType::Recv; // Ensure type is set correctly
                remoteAddrNativeLen = sizeof(sockaddr_in); // Reset for WSARecvFrom

                // Ensure wsaBuf points to the buffer (especially if buffer could have reallocated, though unlikely for fixed pool)
                // and len is set to the full capacity for receiving.
                wsaBuf.buf = buffer.data();
                wsaBuf.len = static_cast<ULONG>(buffer.size());
            }
        };

        class UDPSocketAsync {
        public:
            // Constructor now takes PlayerManager
            UDPSocketAsync(RiftForged::GameLogic::PlayerManager& playerManager,
                PacketProcessor& packetProcessor,
                std::string listenIp, uint16_t listenPort);
            ~UDPSocketAsync();

            UDPSocketAsync(const UDPSocketAsync&) = delete;
            UDPSocketAsync& operator=(const UDPSocketAsync&) = delete;

            bool Init();
            bool Start();
            void Stop();
           
            bool SendRawTo(const NetworkEndpoint& recipient, const char* data, int length);

            // New method for sending data reliably
            bool SendReliableTo(const NetworkEndpoint& recipient,
                MessageType messageType,
                const uint8_t* payloadData,
                uint16_t payloadSize,
                uint8_t additionalFlags = 0); // e.g., IS_HEARTBEAT

            // Optional: A method for sending unreliable data directly if needed by application
            bool SendUnreliableTo(const NetworkEndpoint& recipient,
                MessageType messageType,
                const uint8_t* payloadData,
                uint16_t payloadSize,
                uint8_t additionalFlags = 0);


            bool IsRunning() const;

        private:
            void WorkerThread();
            bool PostReceive(OverlappedIOContext* pRecvContext);
            OverlappedIOContext* GetFreeReceiveContext();
            void ReturnReceiveContext(OverlappedIOContext* pContext);

            void ReliabilityManagementThread();
            std::shared_ptr<ReliableConnectionState> GetOrCreateReliabilityState(const NetworkEndpoint& endpoint);


            RiftForged::GameLogic::PlayerManager& m_playerManager; // Added PlayerManager reference
            PacketProcessor& m_packetProcessor;
            std::string m_listenIp;
            uint16_t m_listenPort;

            SOCKET m_socket;
            HANDLE m_iocpHandle;

            std::vector<std::thread> m_workerThreads;
            std::atomic<bool> m_isRunning;

            std::vector<std::unique_ptr<OverlappedIOContext>> m_receiveContextPool;
            std::deque<OverlappedIOContext*> m_freeReceiveContexts;
            std::mutex m_receiveContextMutex;

            std::map<NetworkEndpoint, std::shared_ptr<ReliableConnectionState>> m_reliabilityStates;
            std::mutex m_reliabilityStatesMutex; // To protect access to m_reliabilityStates
            std::thread m_reliabilityThread;    // Thread for managing retransmissions and pending ACKs

            std::map<NetworkEndpoint, std::chrono::steady_clock::time_point> m_endpointLastSeenTime;

        };

    } // namespace Networking
} // namespace RiftForged