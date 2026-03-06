#include "log.h"
#include <cstdio>
#include <cstdarg>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace lumios::log {

static LogLevel s_min_level = LogLevel::Trace;
static LogCallback s_callback = nullptr;

static const char* level_strings[] = {
    "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"
};

static const char* level_colors[] = {
    "\033[90m", "\033[36m", "\033[32m", "\033[33m", "\033[31m", "\033[35;1m"
};

void init() {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        GetConsoleMode(console, &mode);
        SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
    s_min_level = LUMIOS_DEBUG ? LogLevel::Trace : LogLevel::Info;
}

void set_level(LogLevel level) {
    s_min_level = level;
}

void set_callback(LogCallback cb) {
    s_callback = cb;
}

void message(LogLevel level, const char* fmt, ...) {
    if (level < s_min_level) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif

    int idx = static_cast<int>(level);
    fprintf(stderr, "%s[%02d:%02d:%02d.%03d] [%s] ",
            level_colors[idx],
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
            static_cast<int>(ms.count()),
            level_strings[idx]);

    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    fprintf(stderr, "%s", buf);
    fprintf(stderr, "\033[0m\n");
    fflush(stderr);

    if (s_callback) s_callback(level, buf);
}

} // namespace lumios::log
