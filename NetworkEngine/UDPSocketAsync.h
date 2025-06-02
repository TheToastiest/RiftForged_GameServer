// File: UDPSocketAsync.h (Refactored for INetworkIO role)
// Description: Header file for the UDPSocketAsync class, which handles asynchronous UDP socket operations using IOCP.
// This class will implement the INetworkIO interface.
// Copyright (C) 2023 RiftForged


#pragma once

#include <string>           // <<< KEEP
#include <vector>           // <<< KEEP
#include <thread>           // <<< KEEP
#include <atomic>           // <<< KEEP
#include <mutex>            // <<< KEEP
#include <deque>            // <<< KEEP
#include <memory>           // <<< KEEP (For std::unique_ptr)

// Winsock specific includes // <<< KEEP ALL WINSOCK RELATED
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

// Project-specific includes
#include "INetworkIO.h"         // <<< ADD: Definition of the interface we are implementing
#include "NetworkEndpoint.h"    // <<< KEEP
#include "NetworkCommon.h"      // <<< KEEP (If OverlappedIOContext or other low-level structs need it, otherwise can be removed if only S2C_Response was used from here)

#include "OverlappedIOContext.h"

// Constants
const int DEFAULT_UDP_BUFFER_SIZE_IOCP = 4096;  // <<< KEEP (or make it a non-const member if configurable per instance)
const int MAX_PENDING_RECEIVES_IOCP = 200;    // <<< KEEP (or make it a non-const member)

namespace RiftForged {
    namespace Networking {        
      
        class UDPSocketAsync : public INetworkIO { // <<< MODIFY: Implement INetworkIO
        public:
            // <<< MODIFY: Constructor signature - no longer takes PlayerManager or PacketProcessor directly.
            // It will be initialized via the Init method from the INetworkIO interface.
            UDPSocketAsync(); // Default constructor, or one that just takes listenIp/Port if Init takes the handler
            ~UDPSocketAsync() override; // <<< MODIFY: Add override

            UDPSocketAsync(const UDPSocketAsync&) = delete;             // <<< KEEP
            UDPSocketAsync& operator=(const UDPSocketAsync&) = delete;  // <<< KEEP

            // --- INetworkIO Interface Implementation ---
            bool Init(const std::string& listenIp, uint16_t listenPort, INetworkIOEvents* eventHandler) override; // <<< MODIFY/ADD
            bool Start() override;                                      // <<< MODIFY: Add override
            void Stop() override;                                       // <<< MODIFY: Add override
            bool SendData(const NetworkEndpoint& recipient, const uint8_t* data, uint32_t size) override; // <<< MODIFY/REPLACE SendRawTo
            bool IsRunning() const override;                            // <<< MODIFY: Add override

        private:

            void WorkerThread();
            // <<< MODIFY: Rename to avoid potential clash if INetworkIO exposes similar methods, and to denote internal use.
            bool PostReceiveInternal(OverlappedIOContext* pRecvContext);
            OverlappedIOContext* GetFreeReceiveContextInternal();
            void ReturnReceiveContextInternal(OverlappedIOContext* pContext);

            // --- Member Variables ---
            std::string m_listenIp;                 // <<< KEEP (Set by Init)
            uint16_t m_listenPort;                  // <<< KEEP (Set by Init)
            INetworkIOEvents* m_eventHandler;       // <<< ADD: Pointer to the event sink (PacketHandler)

            SOCKET m_socket;                        // <<< KEEP
            HANDLE m_iocpHandle;                    // <<< KEEP

            std::vector<std::thread> m_workerThreads; // <<< KEEP
            std::atomic<bool> m_isRunning;            // <<< KEEP

            // Receive context pooling
            std::vector<std::unique_ptr<OverlappedIOContext>> m_receiveContextPool; // <<< KEEP
            std::deque<OverlappedIOContext*> m_freeReceiveContexts;                 // <<< KEEP
            std::mutex m_receiveContextMutex;                                       // <<< KEEP

        };

    } // namespace Networking
} // namespace RiftForged