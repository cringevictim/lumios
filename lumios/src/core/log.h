#pragma once

#include "../defines.h"

namespace lumios {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };

using LogCallback = void(*)(LogLevel level, const char* message);

namespace log {
    LUMIOS_API void init();
    LUMIOS_API void set_level(LogLevel level);
    LUMIOS_API void set_callback(LogCallback cb);
    LUMIOS_API void message(LogLevel level, const char* fmt, ...);
}

} // namespace lumios

#define LOG_TRACE(fmt, ...) ::lumios::log::message(::lumios::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) ::lumios::log::message(::lumios::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::lumios::log::message(::lumios::LogLevel::Info,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::lumios::log::message(::lumios::LogLevel::Warn,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::lumios::log::message(::lumios::LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) ::lumios::log::message(::lumios::LogLevel::Fatal, fmt, ##__VA_ARGS__)
