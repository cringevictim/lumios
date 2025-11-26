#include "logger.h"

#include <string>

#ifdef _WIN32
extern "C" {
    void* __stdcall GetStdHandle(unsigned long nStdHandle);
    int __stdcall GetConsoleMode(void* hConsoleHandle, unsigned long* lpMode);
    int __stdcall SetConsoleMode(void* hConsoleHandle, unsigned long dwMode);
}
#define STD_OUTPUT_HANDLE ((unsigned long)-11)
#define STD_ERROR_HANDLE ((unsigned long)-12)
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace lumios::utils {
    
    // Static variables (simplified without mutex for now)
    static LogLevel s_current_log_level = LogLevel::LUM_INFO;
    static bool s_colors_enabled = true;
    static bool s_colors_initialized = false;

    void set_log_level(LogLevel level) {
        s_current_log_level = level;
    }

    LogLevel get_log_level() {
        return s_current_log_level;
    }

    void enable_colors(bool enable) {
        s_colors_enabled = enable;
        if (enable && !s_colors_initialized) {
            setup_console_colors();
            s_colors_initialized = true;
        }
    }

    const char* log_level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::LUM_TRACE: return "TRACE";
            case LogLevel::LUM_DEBUG: return "DEBUG";
            case LogLevel::LUM_INFO:  return "INFO";
            case LogLevel::LUM_WARN:  return "WARN";
            case LogLevel::LUM_ERROR: return "ERROR";
            case LogLevel::LUM_FATAL: return "FATAL";
            default: return "UNKNOWN";
        }
    }

    const char* log_level_to_color(LogLevel level) {
        if (!s_colors_enabled) return "";
        
        switch (level) {
            case LogLevel::LUM_TRACE: return "\033[37m";      // White
            case LogLevel::LUM_DEBUG: return "\033[36m";      // Cyan
            case LogLevel::LUM_INFO:  return "\033[32m";      // Green
            case LogLevel::LUM_WARN:  return "\033[33m";      // Yellow
            case LogLevel::LUM_ERROR: return "\033[31m";      // Red
            case LogLevel::LUM_FATAL: return "\033[35m";      // Magenta
            default: return "\033[0m";                    // Reset
        }
    }


    void setup_console_colors() {
#ifdef _WIN32
        // Enable ANSI color codes on Windows
        void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != (void*)-1) {
            unsigned long dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
        
        void* hErr = GetStdHandle(STD_ERROR_HANDLE);
        if (hErr != (void*)-1) {
            unsigned long dwMode = 0;
            if (GetConsoleMode(hErr, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hErr, dwMode);
            }
        }
#endif
    }

    void log_message(LogLevel level, const std::string& message) {
        if (level < s_current_log_level) {
            return;
        }

        // Initialize colors if needed
        if (s_colors_enabled && !s_colors_initialized) {
            setup_console_colors();
            s_colors_initialized = true;
        }

        const char* color = log_level_to_color(level);
        const char* level_str = log_level_to_string(level);
        const char* reset_color = s_colors_enabled ? "\033[0m" : "";
        
        std::string timestamp = get_timestamp();
        
        // Simple printf-based output
        printf("%s[%s] [%s] %s%s\n", 
               color, timestamp.c_str(), level_str, message.c_str(), reset_color);
    }

    void log_message(LogLevel level, const char* message) {
        if (level < s_current_log_level) {
            return;
        }

        // Initialize colors if needed
        if (s_colors_enabled && !s_colors_initialized) {
            setup_console_colors();
            s_colors_initialized = true;
        }

        const char* color = log_level_to_color(level);
        const char* level_str = log_level_to_string(level);
        const char* reset_color = s_colors_enabled ? "\033[0m" : "";
        
        std::string timestamp = get_timestamp();
        
        // Simple printf-based output
        printf("%s[%s] [%s] %s%s\n", 
               color, timestamp.c_str(), level_str, message ? message : "", reset_color);
    }

    // End of logger implementation

} // namespace lumios::utils