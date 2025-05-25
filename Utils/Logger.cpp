// Copyright (C) 2023 RiftForged

#include "Logger.h" // Should be first

#include <spdlog/sinks/stdout_color_sinks.h> // For console output
#include <spdlog/sinks/rotating_file_sink.h> // For rotating log files
#include <spdlog/async.h> // For asynchronous logging (optional but good for performance)
#include <iostream>       // For initial std::cerr if logger fails to init

namespace RiftForged {
    namespace Utilities {

        // Initialize static members
        std::shared_ptr<spdlog::logger> Logger::s_coreLogger;
        std::shared_ptr<spdlog::logger> Logger::s_networkLogger;
        std::shared_ptr<spdlog::logger> Logger::s_gameplayLogger;
        std::shared_ptr<spdlog::logger> Logger::s_playerManagerLogger;
        std::shared_ptr<spdlog::logger> Logger::s_dataAccessLogger;
        std::shared_ptr<spdlog::logger> Logger::s_cacheLogger;
        bool Logger::s_isInitialized = false;

        std::shared_ptr<spdlog::logger> Logger::CreateAndRegisterLogger(
            const std::string& logger_name,
            const std::vector<std::shared_ptr<spdlog::sinks::sink>>& sinks,
            spdlog::level::level_enum default_level)
        {
            auto logger = spdlog::get(logger_name);
            if (!logger) { // Only create if it doesn't exist
                // For asynchronous logging, create an async logger
                // logger = spdlog::async_factory::create<spdlog::sinks::rotating_file_sink_mt>( ... );
                // Or, more simply, create a standard logger and it can use async sinks if spdlog is initialized for async
                logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
                spdlog::register_logger(logger);
            }
            logger->set_level(default_level);
            logger->flush_on(spdlog::level::warn); // Example: flush on warning and above for this logger
            return logger;
        }


        void Logger::Init(spdlog::level::level_enum console_level,
            spdlog::level::level_enum file_level,
            const std::string& log_file_name,
            size_t max_file_size_mb,
            size_t max_files) {
            if (s_isInitialized) {
                if (s_coreLogger) {
                    s_coreLogger->warn("Logger::Init() called multiple times. Skipping re-initialization.");
                }
                else {
                    // This should not happen if s_isInitialized is true.
                    // Attempt to get a potentially existing default logger to log the warning.
                    auto default_logger = spdlog::default_logger_raw();
                    if (default_logger) default_logger->warn("Logger::Init() called multiple times when s_coreLogger is null.");
                }
                return;
            }

            try {
                // Optional: Initialize SpdLog's thread pool for asynchronous logging
                // spdlog::init_thread_pool(8192, 1); // queue size, 1 background thread

                // --- Create Sinks (shared by all loggers) ---
                std::vector<spdlog::sink_ptr> sinks;

                // Console Sink (thread-safe)
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(console_level);
                console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] %v"); // L for level initial
                sinks.push_back(console_sink);

                // Rotating File Sink for all general logs (thread-safe)
                // Creates files like "logs/server.log", "logs/server.1.log", etc.
                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    log_file_name,
                    1024 * 1024 * max_file_size_mb,
                    max_files,
                    false // false: don't rotate on open
                );
                file_sink->set_level(file_level);
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [PID:%P] [TID:%t] [%n] [%l] %v");
                sinks.push_back(file_sink);

                // --- Create and Register Loggers ---
                s_coreLogger = CreateAndRegisterLogger("Core", sinks, spdlog::level::trace);
                s_networkLogger = CreateAndRegisterLogger("Network", sinks, spdlog::level::trace);
                s_gameplayLogger = CreateAndRegisterLogger("Gameplay", sinks, spdlog::level::trace);
                s_playerManagerLogger = CreateAndRegisterLogger("PlayerMgr", sinks, spdlog::level::trace);
                s_dataAccessLogger = CreateAndRegisterLogger("DataAccess", sinks, spdlog::level::trace);
                s_cacheLogger = CreateAndRegisterLogger("CacheDB", sinks, spdlog::level::trace);

                spdlog::set_default_logger(s_coreLogger); // Set a default for any spdlog::info() calls

                s_isInitialized = true;

                // Use the new logger to confirm initialization
                RF_CORE_INFO("SpdLog Initialized. Console Level: {}, File Level: {}. LogFile: {}",
                    spdlog::level::to_string_view(console_level).data(),
                    spdlog::level::to_string_view(file_level).data(),
                    log_file_name);

            }
            catch (const spdlog::spdlog_ex& ex) {
                std::cerr << "Logger System Init FAILED: " << ex.what() << std::endl;
                // Depending on severity, you might want to exit or continue without file logging.
                // For now, we'll let it continue (console might still work if only file sink failed).
                // Or re-throw if logging is absolutely critical: throw;
            }
        }

        // Getter implementations for static loggers
        std::shared_ptr<spdlog::logger>& Logger::GetCoreLogger() {
            if (!s_isInitialized) {
                // Fallback basic init if Init() wasn't called. This is not ideal.
                // Better to ensure Init() is called once at application start.
                std::cerr << "Logger::GetCoreLogger() called before Logger::Init(). Performing emergency basic init." << std::endl;
                Init(spdlog::level::warn, spdlog::level::warn); // Basic defaults
            }
            return s_coreLogger;
        }
        std::shared_ptr<spdlog::logger>& Logger::GetNetworkLogger() {
            if (!s_isInitialized) Init(); return s_networkLogger;
        }
        std::shared_ptr<spdlog::logger>& Logger::GetGameplayLogger() {
            if (!s_isInitialized) Init(); return s_gameplayLogger;
        }
        std::shared_ptr<spdlog::logger>& Logger::GetPlayerManagerLogger() {
            if (!s_isInitialized) Init(); return s_playerManagerLogger;
        }
        std::shared_ptr<spdlog::logger>& Logger::GetDataAccessLogger() {
            if (!s_isInitialized) Init(); return s_dataAccessLogger;
        }
        std::shared_ptr<spdlog::logger>& Logger::GetCacheLogger() {
            if (!s_isInitialized) Init(); return s_cacheLogger;
        }

        void Logger::FlushAll() {
            if (!s_isInitialized) return;
            // spdlog::flush_every(std::chrono::seconds(3)); // Can be set globally
            // Or flush specific loggers if needed, but spdlog flushes on shutdown anyway.
            if (s_coreLogger) s_coreLogger->flush();
            if (s_networkLogger) s_networkLogger->flush();
            if (s_gameplayLogger) s_gameplayLogger->flush();
            if (s_playerManagerLogger) s_playerManagerLogger->flush();
            if (s_dataAccessLogger) s_dataAccessLogger->flush();
            if (s_cacheLogger) s_cacheLogger->flush();
            // RF_CORE_DEBUG("All loggers flushed.");
        }

        void Logger::Shutdown() {
            if (!s_isInitialized) return;
            RF_CORE_INFO("SpdLog Shutting down...");
            FlushAll();
            spdlog::shutdown(); // Important: Release all spdlog resources
            s_isInitialized = false; // Mark as not initialized
        }

    } // namespace Utilities
} // namespace RiftForged