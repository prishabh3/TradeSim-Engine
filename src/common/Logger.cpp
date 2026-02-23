#include "common/Logger.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace trading {

// ANSI color codes for terminal output
static const char* RESET  = "\033[0m";
static const char* CYAN   = "\033[36m";
static const char* GREEN  = "\033[32m";
static const char* YELLOW = "\033[33m";
static const char* RED    = "\033[31m";
static const char* BOLD_RED = "\033[1;31m";
static const char* GRAY   = "\033[90m";

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& filepath, LogLevel min_level) {
    min_level_ = min_level;
    ofs_.open(filepath, std::ios::app);
    initialized_ = true;
    worker_ = std::thread([this] { flush_loop(); });
}

void Logger::log(LogLevel level, const std::string& msg) {
    if (!initialized_ || level < min_level_) return;
    queue_.push(LogEntry{ level, msg, std::chrono::system_clock::now() });
}

void Logger::shutdown() {
    queue_.shutdown();
    if (worker_.joinable()) worker_.join();
    initialized_ = false;
}

Logger::~Logger() {
    if (initialized_) shutdown();
}

std::string Logger::level_str(LogLevel l) {
    switch (l) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "?????";
}

std::string Logger::level_color(LogLevel l) {
    switch (l) {
        case LogLevel::DEBUG: return GRAY;
        case LogLevel::INFO:  return GREEN;
        case LogLevel::WARN:  return YELLOW;
        case LogLevel::ERROR: return RED;
        case LogLevel::FATAL: return BOLD_RED;
    }
    return RESET;
}

void Logger::flush_loop() {
    while (true) {
        auto entry = queue_.pop(std::chrono::milliseconds(200));
        if (!entry) {
            if (queue_.is_shutdown()) break;
            continue;
        }

        // Format timestamp
        auto t      = std::chrono::system_clock::to_time_t(entry->time);
        auto ms     = std::chrono::duration_cast<std::chrono::milliseconds>(
                          entry->time.time_since_epoch()) % 1000;
        std::tm tm_buf{};
        localtime_r(&t, &tm_buf);

        std::ostringstream ts;
        ts << std::put_time(&tm_buf, "%H:%M:%S") << "."
           << std::setfill('0') << std::setw(3) << ms.count();

        // Console output with color
        std::string color = level_color(entry->level);
        std::cout << CYAN << "[" << ts.str() << "] "
                  << color << "[" << level_str(entry->level) << "] "
                  << RESET << entry->message << "\n";

        // File output (plain text)
        if (ofs_.is_open()) {
            ofs_ << "[" << ts.str() << "] "
                 << "[" << level_str(entry->level) << "] "
                 << entry->message << "\n";
            ofs_.flush();
        }
    }
}

} // namespace trading
