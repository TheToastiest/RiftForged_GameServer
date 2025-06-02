#include "../../include/os_api/OS.h"

#ifdef RF_CORE_PLATFORM_WINDOWS

#include "../network/win/WinSockInitializer.h" // Our internal RAII helper
#include <ws2tcpip.h> // For IPPROTO_TCP, IPPROTO_UDP
#include <process.h>  // For _beginthreadex, _endthreadex
#include <cstdio>     // For error printf/perror (temporary)

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

namespace RiftForged
{
    // Anonymous namespace for internal linkage static variables/helpers
    namespace
    {
        static uint64_t g_performanceFrequency = 0;
        static bool g_frequencyInitialized = false;

        void InitializePerformanceFrequencyOnce()
        {
            if (!g_frequencyInitialized)
            {
                LARGE_INTEGER frequency;
                if (::QueryPerformanceFrequency(&frequency))
                {
                    g_performanceFrequency = static_cast<uint64_t>(frequency.QuadPart);
                }
                // No else needed, g_performanceFrequency remains 0 if QPF fails
                g_frequencyInitialized = true;
            }
        }

        // Wrapper for _beginthreadex
        unsigned int __stdcall ThreadStartRoutine_Win(void* arg)
        {
            auto* threadInfo = static_cast<std::pair<ThreadFunction, void*>*>(arg);
            ThreadFunction func = threadInfo->first;
            void* userData = threadInfo->second;
            delete threadInfo; // Clean up the allocated pair

            func(userData);    // Call the user's thread function
            _endthreadex(0);   // Explicitly end the thread
            return 0;
        }
    } // anonymous namespace


    // --- Networking Initialization ---
    bool OS::InitNetworking()
    {
        InitializePerformanceFrequencyOnce(); // Good place to init this global helper
        CoreOS::Detail::Win::WinSockInitializer& initializer = CoreOS::Detail::Win::WinSockInitializer::GetInstance();
        return initializer.IsInitialized();
    }

    void OS::ShutdownNetworking()
    {
        // Relies on the static WinSockInitializer instance's destructor for WSACleanup at program exit.
        // Call GetInstance() to ensure it's created if it wasn't already, though InitNetworking should precede typical use.
        CoreOS::Detail::Win::WinSockInitializer::GetInstance();
    }

    // --- Socket Operations ---
    RiftSocketHandle OS::CreateSocket(SocketProtocol protocol)
    {
        if (!CoreOS::Detail::Win::WinSockInitializer::GetInstance().IsInitialized()) {
            // Consider logging: Networking not initialized via OS::InitNetworking()
            return RF_INVALID_SOCKET;
        }

        int af = AF_INET; // Using AF_INET (IPv4) for now. Could be a parameter.
        int type = 0;
        int proto = 0;

        switch (protocol)
        {
        case SocketProtocol::TCP:
            type = SOCK_STREAM;
            proto = IPPROTO_TCP;
            break;
        case SocketProtocol::UDP:
            type = SOCK_DGRAM;
            proto = IPPROTO_UDP;
            break;
        default:
            // Consider logging: Invalid protocol specified
            return RF_INVALID_SOCKET;
        }

        SOCKET newSocket = ::WSASocket(af, type, proto, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (newSocket == INVALID_SOCKET) {
            // Consider logging: WSASocket failed, WSAGetLastError()
            return RF_INVALID_SOCKET;
        }
        return newSocket;
    }

    bool OS::CloseSocket(RiftSocketHandle socketHandle)
    {
        if (socketHandle == RF_INVALID_SOCKET) return false;
        if (::closesocket(socketHandle) == SOCKET_ERROR) {
            // Consider logging: closesocket failed, WSAGetLastError()
            return false;
        }
        return true;
    }

    // --- Basic Timing ---
    void OS::Sleep(unsigned int milliseconds)
    {
        ::Sleep(milliseconds);
    }

    uint64_t OS::GetPerformanceCounter()
    {
        LARGE_INTEGER counter;
        ::QueryPerformanceCounter(&counter);
        return static_cast<uint64_t>(counter.QuadPart);
    }

    uint64_t OS::GetPerformanceFrequency()
    {
        if (!g_frequencyInitialized) { // Should have been called by InitNetworking
            InitializePerformanceFrequencyOnce();
        }
        return g_performanceFrequency;
    }

    uint64_t OS::GetTimeNowNanoseconds()
    {
        if (!g_frequencyInitialized) {
            InitializePerformanceFrequencyOnce();
        }
        if (g_performanceFrequency == 0) {
            // Consider logging: Performance frequency is 0, cannot calculate nanoseconds accurately.
            return 0;
        }
        LARGE_INTEGER counter;
        ::QueryPerformanceCounter(&counter);
        uint64_t ticks = static_cast<uint64_t>(counter.QuadPart);

        // (ticks * 1,000,000,000) / frequency
        // To avoid overflow, split the calculation:
        uint64_t seconds_part = ticks / g_performanceFrequency;
        uint64_t remainder_ticks = ticks % g_performanceFrequency;
        uint64_t nanoseconds = seconds_part * 1000000000ULL;
        nanoseconds += (remainder_ticks * 1000000000ULL) / g_performanceFrequency;
        return nanoseconds;
    }

    // --- Thread Management ---
    RiftThreadHandle OS::CreateThread(ThreadFunction startAddress, void* userData)
    {
        if (!startAddress) return RF_INVALID_THREAD_HANDLE;

        auto* threadInfo = new (std::nothrow) std::pair<ThreadFunction, void*>(startAddress, userData);
        if (!threadInfo) {
            // Consider logging: Failed to allocate thread info
            return RF_INVALID_THREAD_HANDLE;
        }

        unsigned int threadID_win;
        HANDLE handle = reinterpret_cast<HANDLE>(
            ::_beginthreadex(nullptr, 0, ThreadStartRoutine_Win, threadInfo, 0, &threadID_win)
            );

        if (handle == 0) { // _beginthreadex returns 0 on error, check errno
            delete threadInfo;
            // Consider logging: _beginthreadex failed, check errno or GetLastError() for CreateThread part.
            // perror("OS::CreateThread _beginthreadex failed"); // Example, use proper logger
            return RF_INVALID_THREAD_HANDLE;
        }
        return handle;
    }

    bool OS::JoinThread(RiftThreadHandle threadHandle)
    {
        if (threadHandle == RF_INVALID_THREAD_HANDLE) return false;
        return ::WaitForSingleObject(threadHandle, INFINITE) == WAIT_OBJECT_0;
    }

    void OS::CloseThreadHandle(RiftThreadHandle threadHandle)
    {
        if (threadHandle != RF_INVALID_THREAD_HANDLE) {
            ::CloseHandle(threadHandle);
        }
    }

    // --- IOCP (I/O Completion Port) Operations ---
    RiftIOCPHandle OS::CreateIOCP(uint32_t numberOfConcurrentThreads)
    {
        // For a new IOCP, first three parameters are specific.
        // FileHandle = INVALID_HANDLE_VALUE
        // ExistingCompletionPort = NULL
        // CompletionKey = 0 (ignored)
        HANDLE iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, static_cast<DWORD>(numberOfConcurrentThreads));
        if (iocpHandle == NULL) { // CreateIoCompletionPort returns NULL on failure
            // Consider logging: CreateIoCompletionPort failed, GetLastError()
            return RF_INVALID_IOCP_HANDLE;
        }
        return iocpHandle;
    }

    bool OS::CloseIOCP(RiftIOCPHandle iocpHandle)
    {
        if (iocpHandle == RF_INVALID_IOCP_HANDLE) return false;
        if (::CloseHandle(iocpHandle) == 0) { // CloseHandle returns 0 on failure
            // Consider logging: CloseHandle for IOCP failed, GetLastError()
            return false;
        }
        return true;
    }

    bool OS::AssociateDeviceWithIOCP(RiftIOCPHandle iocpHandle, HANDLE deviceHandle, uintptr_t completionKey)
    {
        if (iocpHandle == RF_INVALID_IOCP_HANDLE || deviceHandle == INVALID_HANDLE_VALUE /* or other invalid device checks */) {
            return false;
        }
        // When associating an existing device, the NumberOfConcurrentThreads parameter is ignored.
        HANDLE resultHandle = ::CreateIoCompletionPort(deviceHandle, iocpHandle, static_cast<ULONG_PTR>(completionKey), 0);
        if (resultHandle != iocpHandle) { // On success, it returns the ExistingCompletionPort handle
            // Consider logging: CreateIoCompletionPort (for association) failed, GetLastError()
            return false;
        }
        return true;
    }

    bool OS::PostCustomCompletion(RiftIOCPHandle iocpHandle, uint32_t bytesTransferred, uintptr_t completionKey, OVERLAPPED* overlapped)
    {
        if (iocpHandle == RF_INVALID_IOCP_HANDLE) return false;
        if (::PostQueuedCompletionStatus(iocpHandle, static_cast<DWORD>(bytesTransferred), static_cast<ULONG_PTR>(completionKey), overlapped) == 0) { // Returns 0 on failure
            // Consider logging: PostQueuedCompletionStatus failed, GetLastError()
            return false;
        }
        return true;
    }

    bool OS::GetNextCompletion(RiftIOCPHandle iocpHandle, uint32_t& outBytesTransferred, uintptr_t& outCompletionKey, OVERLAPPED** outOverlapped, uint32_t timeoutMilliseconds)
    {
        if (iocpHandle == RF_INVALID_IOCP_HANDLE) return false;

        DWORD numBytes = 0;
        ULONG_PTR completionKeySys = 0; // ULONG_PTR is the type for completion key in WinAPI
        LPOVERLAPPED pOverlapped = nullptr;

        BOOL result = ::GetQueuedCompletionStatus(iocpHandle, &numBytes, &completionKeySys, &pOverlapped, static_cast<DWORD>(timeoutMilliseconds));

        if (result == 0) // Call failed or timed out
        {
            if (pOverlapped != nullptr) {
                // An error occurred on an operation associated with a completion packet.
                // The error code is GetLastError(). The completion packet was dequeued.
                outBytesTransferred = static_cast<uint32_t>(numBytes);
                outCompletionKey = static_cast<uintptr_t>(completionKeySys);
                *outOverlapped = pOverlapped;
                // The caller should check GetLastError() to understand the error.
                // For the purpose of this function, a packet was dequeued, even if it's an error packet.
                // Or, you could decide to return false and have the caller check GetLastError() if pOverlapped is NULL.
                // Let's be explicit: true means a packet was dequeued. False means timeout or fatal error.
                return true; // A completion (even an error completion) was dequeued.
            }
            else
            {
                // No packet dequeued. This could be a timeout (GetLastError() == WAIT_TIMEOUT)
                // or a more serious error with the IOCP handle itself.
                // Consider logging if GetLastError() is not WAIT_TIMEOUT.
                return false; // Timeout or fatal error.
            }
        }

        // Success: a completion packet was dequeued.
        outBytesTransferred = static_cast<uint32_t>(numBytes);
        outCompletionKey = static_cast<uintptr_t>(completionKeySys);
        *outOverlapped = pOverlapped;
        return true;
    }

} // namespace RiftForged

#endif // RF_CORE_PLATFORM_WINDOWS