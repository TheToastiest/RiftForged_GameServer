#ifndef RIFTFORGED_NET_WIN_WINSOCKINITIALIZER_H
#define RIFTFORGED_NET_WIN_WINSOCKINITIALIZER_H

#include "../../core_os/platform/PlatformDetection.h" // For RF_CORE_PLATFORM_WINDOWS

#ifdef RF_CORE_PLATFORM_WINDOWS

#include <winsock2.h> // Required for WSADATA, WSAStartup, WSACleanup
#include <optional>   // For std::optional

namespace RiftForged
{
    namespace Net
    {
        namespace Win
        {
            /**
             * @brief Manages the initialization and cleanup of WinSock2 using RAII.
             *
             * This class is a singleton, accessible via GetInstance(). It ensures
             * WSAStartup is called once and WSACleanup is called on program termination.
             */
            class WinSockInitializer
            {
            public:
                // Prevent copying and assignment
                WinSockInitializer(const WinSockInitializer&) = delete;
                WinSockInitializer& operator=(const WinSockInitializer&) = delete;
                WinSockInitializer(WinSockInitializer&&) = delete;
                WinSockInitializer& operator=(WinSockInitializer&&) = delete;

                /**
                 * @brief Gets the single instance of the WinSockInitializer.
                 * Initializes WinSock on the first call.
                 * @return Reference to the WinSockInitializer instance.
                 */
                static WinSockInitializer& GetInstance();

                /**
                 * @brief Checks if WinSock was successfully initialized.
                 * @return True if initialization was successful, false otherwise.
                 */
                bool IsInitialized() const;

                /**
                 * @brief Gets the WSADATA structure if WinSock was successfully initialized.
                 * @return An std::optional containing WSADATA if initialized, otherwise std::nullopt.
                 */
                std::optional<WSADATA> GetWSAData() const;

                /**
                 * @brief Gets the error code from WSAStartup if initialization failed.
                 * @return 0 if initialization was successful, otherwise the error code from WSAStartup.
                 */
                int GetInitializationError() const;

            private:
                // Private constructor and destructor for singleton pattern
                WinSockInitializer();
                ~WinSockInitializer();

                bool m_isInitialized = false;
                int m_initializationErrorCode = 0; // Stores error code from WSAStartup
                WSADATA m_wsaData;
            };

        } // namespace Win
    } // namespace Net
} // namespace RiftForged

#endif // RF_CORE_PLATFORM_WINDOWS
#endif // RIFTFORGED_NET_WIN_WINSOCKINITIALIZER_H