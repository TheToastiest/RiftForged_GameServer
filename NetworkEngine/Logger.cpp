
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "Logger.h"

// -------------------- LOGGER INITIALIZATION --------------------

Logger::Logger() {
    try {
        // Create different types of log sinks (output locations)
        auto error_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/error_log.txt", true);
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/server_log.txt", 1024 * 1024 * 5, 3);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // Set log levels for each sink
        error_sink->set_level(spdlog::level::err);     // Log general info to file
        rotating_sink->set_level(spdlog::level::trace); // Debug logs in rotating file
        console_sink->set_level(spdlog::level::warn);   // Warnings displayed in console

        // Combine all sinks into one logger
        std::vector<spdlog::sink_ptr> sinks{ error_sink, rotating_sink, console_sink };
        logger = std::make_shared<spdlog::logger>("multi_sink_logger", sinks.begin(), sinks.end());

        // Register and set the default logger
        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
        logger->set_level(spdlog::level::trace); // Allow full trace-level logging
    }
    catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        throw;
    }
}

// -------------------- BASIC LOGGING FUNCTIONS --------------------

// Writes trace-level logs (used for detailed debugging)
void Logger::trace(const std::string& message) {
    logger->trace(message);
}

// Trace-level logs with additional data
void Logger::trace(const std::string& message, const std::string& data) {
    logger->trace("{} - {}", message, data);
}

// Debug-level logs (general debugging messages)
void Logger::debug(const std::string& message) {
    logger->debug(message);
}

// Debug logs with a specific port number
void Logger::debug(const std::string& message, int port) {
    logger->debug("{} | Port: {}", message, port);
}

// Info logs with additional data as required
void Logger::info(const std::string& message, int port) {
    logger->debug("{} {}", message, port);
}

void Logger::info(const std::string& message, std::string& value) {
    logger->info("{} {}", message, value);
}

// Debug logs with additional data
void Logger::debug(const std::string& message, const std::string& additionalData) {
    logger->debug("{} | Data: {}", message, additionalData);
}

// Info Overload with two integers
void Logger::info(const std::string& message, int value1, int value2) {
    logger->info("{} - Error Code: {}", message, value1, value2);
}

//  Error Overload with port and windows errors
void Logger::error(const std::string& message, int port, int errorCode) {
    logger->error("{} - Port: {} | Error Code: {}", message, port, errorCode);
}

// Info-level logs (used for standard operations)
void Logger::info(const std::string& message) {
    logger->info(message);
}

//// Info-level logs with error codes
//void Logger::info(const std::string& message, int port, int errorCode) {
//    logger->info("{} - Error Code: {}", message, errorCode);
//}

// Warning logs (used for potential issues)
void Logger::warn(const std::string& message) {
    logger->warn(message);
}

// Warning logs with error codes
void Logger::warn(const std::string& message, int errorCode) {
    logger->warn("{} - Error Code: {}", message, errorCode);
}

// Error logs (critical issues)
void Logger::error(const std::string& message) {
    logger->error(message);
}

// Error logs with Windows error codes
void Logger::error(const std::string& message, int errorCode) {
    logger->error("{} - Error Code: {}", message, errorCode);
}

// Error logs with additional details
void Logger::error(const std::string& message, int errorCode, const std::string& details) {
    logger->error("{} - Error Code: {} | Details: {}", message, errorCode, details);
}

// -------------------- LOGGING UTILS --------------------

// Flush logs to ensure all data is written
void Logger::flush() {
    logger->flush();
}

// Generates a timestamp for log entries
std::string Logger::getTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm localTime;

    // Convert to local time safely
    if (localtime_s(&localTime, &now) != 0) {
        return "Error retrieving time";
    }

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S"); // Format: YYYY-MM-DD HH:MM:SS
    return oss.str();
}

// Logs network packet events
void Logger::logPacketEvent(const std::string& eventType, const std::string& clientInfo, const std::string& message) {
    logger->info("[{}] [{}] [{}] {}", getTimestamp(), eventType, clientInfo, message);
}