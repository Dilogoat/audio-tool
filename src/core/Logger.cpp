#include "audio_core/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iostream>

namespace audio_core {

LogLevel log_level_from_string(const std::string& name) {
    if (name == "error") return LogLevel::Error;
    if (name == "warn" || name == "warning") return LogLevel::Warn;
    if (name == "debug") return LogLevel::Debug;
    return LogLevel::Info;
}

const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Debug:
            return "DEBUG";
    }
    return "INFO";
}

namespace {

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

}  // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}

bool Logger::set_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.close();
    }
    if (path.empty()) {
        return true;
    }
    file_.open(path, std::ios::app);
    return file_.is_open();
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) > static_cast<int>(level_)) {
        return;
    }
    const std::string line =
        timestamp() + " [" + log_level_to_string(level) + "] " + message;

    std::ostream& out = (level == LogLevel::Error || level == LogLevel::Warn) ? std::cerr : std::cout;
    out << line << '\n';
    out.flush();

    if (file_.is_open()) {
        file_ << line << '\n';
        file_.flush();
    }
}

}  // namespace audio_core
