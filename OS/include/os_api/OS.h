#ifndef RIFTFORGED_OS_API_RIFTFORGEDOS_H
#define RIFTFORGED_OS_API_RIFTFORGEDOS_H

#include "../core_os/platform/PlatformDetection.h" // Essential for platform-specific code


// Platform-specific types and constants
#ifdef RF_CORE_PLATFORM_WINDOWS
#include <winsock2.h> // For SOCKET type
#include <windows.h>  // For HANDLE (thread/IOCP handle), OVERLAPPED

// Socket Handle
using RiftSocketHandle = SOCKET;
constexpr RiftSocketHandle RF_INVALID_SOCKET = INVALID_SOCKET;

// Thread Handle
using RiftThreadHandle = HANDLE;
constexpr RiftThreadHandle RF_INVALID_THREAD_HANDLE = nullptr;

// IOCP Handle
using RiftIOCPHandle = HANDLE;
constexpr RiftIOCPHandle RF_INVALID_IOCP_HANDLE = nullptr; // CreateIoCompletionPort returns NULL on failure

#else
    // Define placeholders for non-Windows platforms
    // Socket Handle
using SOCKET_PLACEHOLDER_TYPE = int; // Common type for POSIX socket descriptors
using RiftSocketHandle = SOCKET_PLACEHOLDER_TYPE;
constexpr RiftSocketHandle RF_INVALID_SOCKET = -1; // Common invalid fd value

// Thread Handle (e.g., pthread_t on POSIX)
#include <pthread.h> // For pthread_t (example)
using RiftThreadHandle = pthread_t; // Example, actual type might vary or be opaque
// Validity of pthread_t is complex; often just compare with a value after creation fails.
// For an API, a wrapper struct or opaque pointer might be better cross-platform.
// For now, this is a placeholder for the concept.
constexpr RiftThreadHandle RF_INVALID_THREAD_HANDLE = 0; // Placeholder

// IOCP Handle (e.g., epoll fd on Linux, kqueue fd on macOS)
using RiftIOCPHandle = int; // Example, common for fd-based mechanisms
constexpr RiftIOCPHandle RF_INVALID_IOCP_HANDLE = -1; // Common invalid fd value

// OVERLAPPED is Windows-specific. For a cross-platform layer,
// a custom structure or void* would be needed for overlapped-like concepts.
// For now, we'll assume OVERLAPPED is only used in Windows-specific sections or
// a platform-agnostic equivalent would be defined.
struct OVERLAPPED; // Forward declare or provide an empty struct for API signature compatibility for now

#endif


namespace RiftForged
{
    // Forward declaration for thread function pointer type
    using ThreadFunction = void(*)(void* userData);

    class OS
    {
    public:
        // --- Networking Initialization ---
        static bool InitNetworking();
        static void ShutdownNetworking();

        // --- Socket Operations ---
        enum class SocketProtocol { TCP, UDP };
        static RiftSocketHandle CreateSocket(SocketProtocol protocol);
        static bool CloseSocket(RiftSocketHandle socketHandle);

        // --- Basic Timing ---
        static void Sleep(unsigned int milliseconds);
        static uint64_t GetTimeNowNanoseconds();
        static uint64_t GetPerformanceCounter();
        static uint64_t GetPerformanceFrequency();

        // --- Thread Management ---
        static RiftThreadHandle CreateThread(ThreadFunction startAddress, void* userData);
        static bool JoinThread(RiftThreadHandle threadHandle);
        static void CloseThreadHandle(RiftThreadHandle threadHandle);

        // --- IOCP (I/O Completion Port) Operations ---
        /**
         * @brief Value for GetQueuedCompletionStatus timeout indicating an infinite wait.
         */
        static constexpr uint32_t INFINITE_TIMEOUT = 0xFFFFFFFF; // Windows INFINITE

        static RiftIOCPHandle CreateIOCP(uint32_t numberOfConcurrentThreads = 0);
        static bool CloseIOCP(RiftIOCPHandle iocpHandle);
        static bool AssociateDeviceWithIOCP(RiftIOCPHandle iocpHandle, HANDLE deviceHandle, uintptr_t completionKey); // deviceHandle is HANDLE for Windows sockets/files
        static bool PostCustomCompletion(RiftIOCPHandle iocpHandle, uint32_t bytesTransferred, uintptr_t completionKey, OVERLAPPED* overlapped);
        static bool GetNextCompletion(RiftIOCPHandle iocpHandle, uint32_t& outBytesTransferred, uintptr_t& outCompletionKey, OVERLAPPED** outOverlapped, uint32_t timeoutMilliseconds);

        // --- Future additions ---
        // static GetLastErrorPlatformCode();
        // static GetLastErrorString();
    };

} // namespace RiftForged

#endif // RIFTFORGED_OS_API_RIFTFORGEDOS_H