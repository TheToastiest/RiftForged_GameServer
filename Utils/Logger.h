// Copyright (C) 2023 RiftForged

#pragma once

#include <spdlog/spdlog.h> // Main SpdLog include
#include <spdlog/fmt/ostr.h> // For custom ostream operator support if you log custom types
#include <memory>            // For std::shared_ptr
#include <string>            // For std::string
#include <vector>            // For sinks vector


// Forward declare to reduce include dependencies in other headers if they only need to log
// and don't call Init or specific sink setups.
namespace spdlog {
    class logger;
    namespace sinks { class sink; } // Forward declare base sink type
}

namespace RiftForged {
    namespace Utilities { // Using "Utilities" as you confirmed

        class Logger {
        public:
            // Call this ONCE at the very beginning of your server application (e.g., in main())
            // Default console level: info, Default file level: trace
            static void Init(
                spdlog::level::level_enum console_level = spdlog::level::info,
                spdlog::level::level_enum file_level = spdlog::level::trace,
                const std::string& log_file_name = "logs/riftforged_server.log",
                size_t max_file_size_mb = 10,
                size_t max_files = 5
            );

            // Access to specific, named loggers
            static std::shared_ptr<spdlog::logger>& GetCoreLogger();     // General server/app events
            static std::shared_ptr<spdlog::logger>& GetNetworkLogger();  // UDPSocketAsync, PacketProcessor, etc.
            static std::shared_ptr<spdlog::logger>& GetGameplayLogger(); // GameplayEngine, ability logic
            static std::shared_ptr<spdlog::logger>& GetPlayerManagerLogger(); // PlayerManager events
            static std::shared_ptr<spdlog::logger>& GetDataAccessLogger(); // For DataAccessEngine (SQL)
            static std::shared_ptr<spdlog::logger>& GetCacheLogger();      // For Redis/DragonflyDB interactions
            // Add more loggers as your modules grow

            // Ensure all buffered logs are written to sinks
            static void FlushAll();
            static void Shutdown(); // Properly shutdown spdlog

        private:
            // Static logger instances, one per major module/category
            static std::shared_ptr<spdlog::logger> s_coreLogger;
            static std::shared_ptr<spdlog::logger> s_networkLogger;
            static std::shared_ptr<spdlog::logger> s_gameplayLogger;
            static std::shared_ptr<spdlog::logger> s_playerManagerLogger;
            static std::shared_ptr<spdlog::logger> s_dataAccessLogger;
            static std::shared_ptr<spdlog::logger> s_cacheLogger;

            static bool s_isInitialized; // To prevent double initialization

            // Helper to create and register a logger with shared sinks
            static std::shared_ptr<spdlog::logger> CreateAndRegisterLogger(
                const std::string& logger_name,
                const std::vector<std::shared_ptr<spdlog::sinks::sink>>& sinks,
                spdlog::level::level_enum default_level = spdlog::level::trace
            );
        };

        // --- Convenience Logging Macros ---
        // These use the static getters from the Logger class.
        // (Ensure this header is included wherever you use these macros)

        // Core Logger Macros
#define RF_CORE_TRACE(...)    if (RiftForged::Utilities::Logger::GetCoreLogger()) { RiftForged::Utilities::Logger::GetCoreLogger()->trace(__VA_ARGS__); }
#define RF_CORE_DEBUG(...)    if (RiftForged::Utilities::Logger::GetCoreLogger()) { RiftForged::Utilities::Logger::GetCoreLogger()->debug(__VA_ARGS__); }
#define RF_CORE_INFO(...)     if (RiftForged::Utilities::Logger::GetCoreLogger()) { RiftForged::Utilities::Logger::GetCoreLogger()->info(__VA_ARGS__); }
#define RF_CORE_WARN(...)     if (RiftForged::Utilities::Logger::GetCoreLogger()) { RiftForged::Utilities::Logger::GetCoreLogger()->warn(__VA_ARGS__); }
#define RF_CORE_ERROR(...)    if (RiftForged::Utilities::Logger::GetCoreLogger()) { RiftForged::Utilities::Logger::GetCoreLogger()->error(__VA_ARGS__); }
#define RF_CORE_CRITICAL(...) if (RiftForged::Utilities::Logger::GetCoreLogger()) { RiftForged::Utilities::Logger::GetCoreLogger()->critical(__VA_ARGS__); }

// Network Logger Macros
#define RF_NETWORK_TRACE(...) if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->trace(__VA_ARGS__); }
#define RF_NETWORK_DEBUG(...) if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->debug(__VA_ARGS__); }
#define RF_NETWORK_INFO(...)  if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->info(__VA_ARGS__); }
#define RF_NETWORK_WARN(...)  if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->warn(__VA_ARGS__); }
#define RF_NETWORK_ERROR(...) if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->error(__VA_ARGS__); }
#define RF_NETWORK_CRITICAL(...) if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->critical(__VA_ARGS__); }

// Server Engine Logger Macros
#define RF_ENGINE_TRACE(...) if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->trace(__VA_ARGS__); }
#define RF_ENGINE_DEBUG(...) if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->debug(__VA_ARGS__); }
#define RF_ENGINE_INFO(...)  if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->info(__VA_ARGS__); }
#define RF_ENGINE_WARN(...)  if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->warn(__VA_ARGS__); }
#define RF_ENGINE_ERROR(...) if (RiftForged::Utilities::Logger::GetNetworkLogger()) { RiftForged::Utilities::Logger::GetNetworkLogger()->error(__VA_ARGS__); }

// Gameplay Logger Macros
#define RF_GAMEPLAY_TRACE(...) if (RiftForged::Utilities::Logger::GetGameplayLogger()) { RiftForged::Utilities::Logger::GetGameplayLogger()->trace(__VA_ARGS__); }
#define RF_GAMEPLAY_DEBUG(...) if (RiftForged::Utilities::Logger::GetGameplayLogger()) { RiftForged::Utilities::Logger::GetGameplayLogger()->debug(__VA_ARGS__); }
#define RF_GAMEPLAY_INFO(...)  if (RiftForged::Utilities::Logger::GetGameplayLogger()) { RiftForged::Utilities::Logger::GetGameplayLogger()->info(__VA_ARGS__); }
#define RF_GAMEPLAY_WARN(...)  if (RiftForged::Utilities::Logger::GetGameplayLogger()) { RiftForged::Utilities::Logger::GetGameplayLogger()->warn(__VA_ARGS__); }
#define RF_GAMEPLAY_ERROR(...) if (RiftForged::Utilities::Logger::GetGameplayLogger()) { RiftForged::Utilities::Logger::GetGameplayLogger()->error(__VA_ARGS__); }

// PlayerManager Logger Macros
#define RF_PLAYERMGR_TRACE(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->trace(__VA_ARGS__); }
#define RF_PLAYERMGR_DEBUG(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->debug(__VA_ARGS__); }
#define RF_PLAYERMGR_INFO(...)  if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->info(__VA_ARGS__); }
#define RF_PLAYERMGR_WARN(...)  if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->warn(__VA_ARGS__); }
#define RF_PLAYERMGR_ERROR(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->error(__VA_ARGS__); }
#define RF_PLAYERMGR_CRITICAL(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->critical(__VA_ARGS__); }

// DataAccess Logger Macros
#define RF_DATAACCESS_TRACE(...) if (RiftForged::Utilities::Logger::GetDataAccessLogger()) { RiftForged::Utilities::Logger::GetDataAccessLogger()->trace(__VA_ARGS__); }
#define RF_DATAACCESS_DEBUG(...) if (RiftForged::Utilities::Logger::GetDataAccessLogger()) { RiftForged::Utilities::Logger::GetDataAccessLogger()->debug(__VA_ARGS__); }
#define RF_DATAACCESS_INFO(...)  if (RiftForged::Utilities::Logger::GetDataAccessLogger()) { RiftForged::Utilities::Logger::GetDataAccessLogger()->info(__VA_ARGS__); }
#define RF_DATAACCESS_WARN(...)  if (RiftForged::Utilities::Logger::GetDataAccessLogger()) { RiftForged::Utilities::Logger::GetDataAccessLogger()->warn(__VA_ARGS__); }
#define RF_DATAACCESS_ERROR(...) if (RiftForged::Utilities::Logger::GetDataAccessLogger()) { RiftForged::Utilities::Logger::GetDataAccessLogger()->error(__VA_ARGS__); }

// Cache Logger Macros
#define RF_CACHE_TRACE(...) if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->trace(__VA_ARGS__); }
#define RF_CACHE_DEBUG(...) if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->debug(__VA_ARGS__); }
#define RF_CACHE_INFO(...)  if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->info(__VA_ARGS__); }
#define RF_CACHE_WARN(...)  if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->warn(__VA_ARGS__); }
#define RF_CACHE_ERROR(...) if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->error(__VA_ARGS__); }
        
        // Player Manager Logger Macros
#define RF_PLAYERMNGR_TRACE(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->trace(__VA_ARGS__); }
#define RF_PLAYERMNGR_DEBUG(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->debug(__VA_ARGS__); }
#define RF_PLAYERMNGR_INFO(...)  if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->info(__VA_ARGS__); }
#define RF_PLAYERMNGR_WARN(...)  if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->warn(__VA_ARGS__); }
#define RF_PLAYERMNGR_ERROR(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->error(__VA_ARGS__); }
#define RF_PLAYERMNGR_CRITICAL(...) if (RiftForged::Utilities::Logger::GetPlayerManagerLogger()) { RiftForged::Utilities::Logger::GetPlayerManagerLogger()->critical(__VA_ARGS__); }

        // Physics Logger Macros
#define RF_PHYSICS_TRACE(...) if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->trace(__VA_ARGS__); }
#define RF_PHYSICS_DEBUG(...) if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->debug(__VA_ARGS__); }
#define RF_PHYSICS_INFO(...)  if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->info(__VA_ARGS__); }
#define RF_PHYSICS_WARN(...)  if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->warn(__VA_ARGS__); }
#define RF_PHYSICS_ERROR(...) if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->error(__VA_ARGS__); }
#define RF_PHYSICS_CRITICAL(...) if (RiftForged::Utilities::Logger::GetCacheLogger()) { RiftForged::Utilities::Logger::GetCacheLogger()->critical(__VA_ARGS__); }


    } // namespace Utilities
} // namespace RiftForged