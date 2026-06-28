#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace audio_core {

enum class LogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3
};

LogLevel log_level_from_string(const std::string& name);
const char* log_level_to_string(LogLevel level);

// Minimal thread-safe, dependency-free logger. Writes to stderr and,
// optionally, to a log file. Intended for the CLI but usable anywhere.
class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    LogLevel level() const;

    // Opens (append mode) an optional log file. Empty path disables file logging.
    bool set_file(const std::string& path);

    void log(LogLevel level, const std::string& message);

    void error(const std::string& message) { log(LogLevel::Error, message); }
    void warn(const std::string& message) { log(LogLevel::Warn, message); }
    void info(const std::string& message) { log(LogLevel::Info, message); }
    void debug(const std::string& message) { log(LogLevel::Debug, message); }

private:
    Logger() = default;

    mutable std::mutex mutex_;
    LogLevel level_ = LogLevel::Info;
    std::ofstream file_;
};

}  // namespace audio_core
