#pragma once
#include <iostream>
#include <string>

// Minimal logging facility for Phase 0.
// Deliberately simple: a hackathon MVP does not need a logging framework,
// just consistent, greppable output with severity tags.
namespace twin {

enum class LogLevel { Info, Warn, Error };

inline void Log(LogLevel level, const std::string& message) {
    switch (level) {
        case LogLevel::Info:
            std::cout << "[INFO]  " << message << std::endl;
            break;
        case LogLevel::Warn:
            std::cout << "[WARN]  " << message << std::endl;
            break;
        case LogLevel::Error:
            std::cerr << "[ERROR] " << message << std::endl;
            break;
    }
}

inline void LogInfo(const std::string& msg) { Log(LogLevel::Info, msg); }
inline void LogWarn(const std::string& msg) { Log(LogLevel::Warn, msg); }
inline void LogError(const std::string& msg) { Log(LogLevel::Error, msg); }

}  // namespace twin
