#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <vector>

class Logger {
public:
    Logger();
    void flush();
    // New method for logging packets
    void logPacketEvent(const std::string& eventType, const std::string& clientInfo, const std::string& message);

    void info(const std::string& message);
    void info(const std::string& message, std::string& value);
    void info(const std::string& message, int value);
    void info(const std::string& message, int port, int errorCode);
    void debug(const std::string& message);
    void debug(const std::string& message, int port);  //  Overload for logging ports  
    void debug(const std::string& message, const std::string& additionalData);  //  Overload for extra data
    void trace(const std::string& message);
    void trace(const std::string& message, const std::string& data);  // New overload
    void warn(const std::string& message);
    void warn(const std::string& message, int errorCode);
    void error(const std::string& message);  // Standard error logging
    void error(const std::string& message, int errorCode);  // Error logging with Windows error codes
    void error(const std::string& message, int port, int errorCode); // Error logging with port and windows codes.
    void error(const std::string& message, int errorCode, const std::string& details);
    static std::string getTimestamp();

private:
    std::shared_ptr<spdlog::logger> logger;
};
