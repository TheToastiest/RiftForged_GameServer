#include "net/win/WinSockInitializer.h" // Header for this implementation

#ifdef RF_CORE_PLATFORM_WINDOWS

#include <cstdio> // For printf, for basic error reporting initially
// Potentially include our own Logger class here later:
// #include "diagnostics/Logger.h" // e.g., RF_CORE_LOG_ERROR, RF_CORE_LOG_INFO

namespace RiftForged
{
    namespace Net
    {
        namespace Win
        {

            WinSockInitializer& WinSockInitializer::GetInstance()
            {
                // C++11 guarantees thread-safe initialization for static local variables.
                static WinSockInitializer instance;
                return instance;
            }

            WinSockInitializer::WinSockInitializer()
                // m_isInitialized is already false by in-class initialization
                // m_initializationErrorCode is already 0 by in-class initialization
            {
                int result = WSAStartup(MAKEWORD(2, 2), &m_wsaData);
                if (result != 0)
                {
                    m_initializationErrorCode = result;
                    // RF_CORE_LOG_ERROR("WSAStartup failed with error code: {}", m_initializationErrorCode);
                    printf("WinSockInitializer: WSAStartup failed with error: %d\n", m_initializationErrorCode);
                    m_isInitialized = false;
                }
                else
                {
                    if (LOBYTE(m_wsaData.wVersion) != 2 || HIBYTE(m_wsaData.wVersion) != 2)
                    {
                        // WSAStartup succeeded but didn't provide the version we asked for.
                        // This is unusual for WinSock 2.2, but good to check.
                        // We'll treat this as a failure for our purposes.
                        // WSASYSNOTREADY (10091) could be a more specific error if we want to invent one here,
                        // but WSAStartup itself returned 0, so we use a generic internal error approach.
                        m_initializationErrorCode = -1; // Custom error code indicating version mismatch
                        // RF_CORE_LOG_ERROR("Could not find a usable WinSock DLL. Requested 2.2, got {}.{}. Cleaning up.",
                        //                   LOBYTE(m_wsaData.wVersion), HIBYTE(m_wsaData.wVersion));
                        printf("WinSockInitializer: Could not find a usable WinSock DLL. Requested 2.2, got %d.%d. Cleaning up.\n",
                            LOBYTE(m_wsaData.wVersion), HIBYTE(m_wsaData.wVersion));
                        WSACleanup(); // Cleanup what was started
                        m_isInitialized = false;
                    }
                    else
                    {
                        // RF_CORE_LOG_INFO("WinSock 2.2 initialized successfully.");
                        // RF_CORE_LOG_INFO("Description: {}", m_wsaData.szDescription);
                        // RF_CORE_LOG_INFO("System Status: {}", m_wsaData.szSystemStatus);
                        printf("WinSockInitializer: WinSock 2.2 initialized successfully.\n");
                        // printf("Description: %s\n", m_wsaData.szDescription); // Optional: for verbose logging
                        // printf("System Status: %s\n", m_wsaData.szSystemStatus); // Optional: for verbose logging
                        m_isInitialized = true;
                        m_initializationErrorCode = 0; // Success
                    }
                }
            }

            WinSockInitializer::~WinSockInitializer()
            {
                if (m_isInitialized) // Only cleanup if we successfully initialized
                {
                    if (WSACleanup() != 0)
                    {
                        int cleanupError = WSAGetLastError();
                        // RF_CORE_LOG_ERROR("WSACleanup failed with error: {}", cleanupError);
                        printf("WinSockInitializer: WSACleanup failed with error: %d\n", cleanupError);
                    }
                    else
                    {
                        // RF_CORE_LOG_INFO("WinSock successfully cleaned up.");
                        printf("WinSockInitializer: WinSock successfully cleaned up.\n");
                    }
                }
            }

            bool WinSockInitializer::IsInitialized() const
            {
                return m_isInitialized;
            }

            std::optional<WSADATA> WinSockInitializer::GetWSAData() const
            {
                if (m_isInitialized)
                {
                    return m_wsaData;
                }
                return std::nullopt;
            }

            int WinSockInitializer::GetInitializationError() const
            {
                return m_initializationErrorCode;
            }

        } // namespace Win
    } // namespace Net
} // namespace RiftForged

#endif // RF_CORE_PLATFORM_WINDOWS