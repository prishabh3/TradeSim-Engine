#pragma once

#include <string>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iostream>
#include "ThreadSafeQueue.hpp"

namespace trading {

enum class LogLevel { DEBUG = 0, INFO, WARN, ERROR, FATAL };

struct LogEntry {
    LogLevel    level;
    std::string message;
    std::chrono::system_clock::time_point time;
};

/**
 * Async logger: trading threads push LogEntry into a queue;
 * a dedicated background thread drains it to file + stdout.
 * This ensures zero-latency impact on the hot trading path.
 */
class Logger {
public:
    static Logger& instance();

    void init(const std::string& filepath, LogLevel min_level = LogLevel::INFO);

    void log(LogLevel level, const std::string& msg);

    void debug(const std::string& m) { log(LogLevel::DEBUG, m); }
    void info (const std::string& m) { log(LogLevel::INFO,  m); }
    void warn (const std::string& m) { log(LogLevel::WARN,  m); }
    void error(const std::string& m) { log(LogLevel::ERROR, m); }
    void fatal(const std::string& m) { log(LogLevel::FATAL, m); }

    void shutdown();

    ~Logger();

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::string level_str(LogLevel l);
    static std::string level_color(LogLevel l);
    void flush_loop();

    LogLevel                 min_level_  = LogLevel::INFO;
    std::ofstream            ofs_;
    ThreadSafeQueue<LogEntry> queue_;
    std::thread              worker_;
    std::atomic<bool>        initialized_{false};
};

// Convenience macro — injects source location automatically
#define LOG_DEBUG(msg) trading::Logger::instance().debug(std::string("[") + __FILE__ + ":" + std::to_string(__LINE__) + "] " + (msg))
#define LOG_INFO(msg)  trading::Logger::instance().info(msg)
#define LOG_WARN(msg)  trading::Logger::instance().warn(msg)
#define LOG_ERROR(msg) trading::Logger::instance().error(std::string("[") + __FILE__ + ":" + std::to_string(__LINE__) + "] " + (msg))
#define LOG_FATAL(msg) trading::Logger::instance().fatal(msg)

} // namespace trading
