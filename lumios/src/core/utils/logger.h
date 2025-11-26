/**
 * @file logger.h
 * @brief Comprehensive logging system with colored output and multiple log levels
 * 
 * This logging system provides:
 * - Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
 * - Colored console output with ANSI codes
 * - Timestamp support with millisecond precision
 * - Easy-to-use macros for different log levels
 * - Cross-platform compatibility (Windows, Linux, macOS)
 * - Thread-safe logging operations
 * - Configurable output streams (stdout/stderr)
 * 
 * @author Lumios Engine Team
 * @version 1.0
 * @date 2025
 */

#pragma once

#include "defines.h"
#include "time.h"

#include <string>
#include <sstream>

// Forward declarations to avoid including problematic headers
extern "C" {
    int printf(const char* format, ...);
    int sprintf_s(char* buffer, size_t sizeOfBuffer, const char* format, ...);
}

namespace lumios::utils {
    
    /**
     * @brief Enumeration of available log levels
     * 
     * Log levels are ordered by severity, with TRACE being the least severe
     * and FATAL being the most severe. Setting a minimum log level will
     * filter out all messages below that level.
     * 
     * Usage example:
     * @code
     * set_log_level(LogLevel::WARN); // Only show WARN, ERROR, FATAL
     * LOG_INFO("This won't be shown");
     * LOG_ERROR("This will be shown");
     * @endcode
     */
    enum class LogLevel {
        LUM_TRACE = 0,  ///< Detailed trace information for debugging (white)
        LUM_DEBUG = 1,  ///< Debug information for development (cyan)
        LUM_INFO = 2,   ///< General information messages (green)
        LUM_WARN = 3,   ///< Warning messages for potential issues (yellow)
        LUM_ERROR = 4,  ///< Error messages for recoverable errors (red)
        LUM_FATAL = 5   ///< Fatal error messages for unrecoverable errors (magenta)
    };

    /**
     * @brief Log a message with the specified level
     * @param level The severity level of the message
     * @param message The message to log
     * 
     * This function outputs a formatted log message with timestamp, level indicator,
     * and color coding (if enabled). Messages below the current minimum log level
     * will be filtered out.
     * 
     * The output format is: [HH:MM:SS.mmm] [LEVEL] message
     * 
     * @note This function is thread-safe
     */
    LUMIOS_API void log_message(LogLevel level, const std::string& message);
    
    /**
     * @brief Log a message with the specified level (const char* overload)
     * @param level The severity level of the message
     * @param message The message to log (C-style string)
     * 
     * Convenience overload for logging C-style string literals directly
     * without requiring explicit string construction.
     * 
     * @note This function is thread-safe
     */
    LUMIOS_API void log_message(LogLevel level, const char* message);
    
    /**
     * @brief Set the minimum log level to display
     * @param level The minimum log level - messages below this level will be filtered
     * 
     * Use this to control the verbosity of logging output:
     * - LogLevel::TRACE: Show all messages (most verbose)
     * - LogLevel::DEBUG: Show DEBUG, INFO, WARN, ERROR, FATAL
     * - LogLevel::INFO: Show INFO, WARN, ERROR, FATAL (normal verbosity)
     * - LogLevel::WARN: Show WARN, ERROR, FATAL
     * - LogLevel::ERROR: Show ERROR, FATAL
     * - LogLevel::FATAL: Show only FATAL (least verbose)
     * 
     * @note Thread-safe operation
     */
    LUMIOS_API void set_log_level(LogLevel level);
    
    /**
     * @brief Get the current minimum log level
     * @return The current minimum log level
     * 
     * @note Thread-safe operation
     */
    LUMIOS_API LogLevel get_log_level();
    
    /**
     * @brief Enable or disable colored console output
     * @param enable True to enable ANSI color codes, false to disable
     * 
     * When enabled, log messages will be displayed with different colors
     * based on their severity level:
     * - TRACE: White (\033[37m)
     * - DEBUG: Cyan (\033[36m)
     * - INFO: Green (\033[32m)
     * - WARN: Yellow (\033[33m)
     * - ERROR: Red (\033[31m)
     * - FATAL: Magenta (\033[35m)
     * 
     * On Windows, this automatically enables ANSI color support in the console.
     * 
     * @note Thread-safe operation
     */
    LUMIOS_API void enable_colors(bool enable);

    /**
     * @brief Convert log level to human-readable string
     * @param level The log level to convert
     * @return String representation of the log level (5 characters, padded)
     * 
     * Returns a fixed-width string for consistent formatting:
     * - TRACE, DEBUG, INFO , WARN , ERROR, FATAL
     */
    LUMIOS_API const char* log_level_to_string(LogLevel level);
    
    /**
     * @brief Get ANSI color code for log level
     * @param level The log level
     * @return ANSI color escape sequence (empty string if colors disabled)
     * 
     * Returns the appropriate ANSI escape sequence for the given log level,
     * or an empty string if colors are disabled.
     */
    LUMIOS_API const char* log_level_to_color(LogLevel level);

    /**
     * @brief Setup console for colored output (Windows-specific)
     * 
     * On Windows, this enables ANSI color code processing in the console.
     * On other platforms, this function does nothing.
     * 
     * @note Called automatically when colors are first enabled
     */
    LUMIOS_API void setup_console_colors();

    // Helper function to convert value to string
    template<typename T>
    inline std::string to_string_helper(const T& value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    // Specialization for string types to avoid unnecessary conversions
    inline std::string to_string_helper(const std::string& value) {
        return value;
    }

    inline std::string to_string_helper(const char* value) {
        return std::string(value ? value : "");
    }

    /**
     * @brief Format a message with arguments (template function)
     * @tparam Args Variadic template arguments
     * @param level The log level
     * @param format Format string with {} placeholders
     * @param args Arguments to format
     * 
     * This template function provides formatted logging capabilities.
     * It replaces {} placeholders in the format string with the provided arguments.
     * 
     * Example usage:
     * @code
     * log_formatted(LogLevel::INFO, "Player {} has {} health", playerName, health);
     * @endcode
     */
    template<typename... Args>
    void log_formatted(LogLevel level, const std::string& format, Args&&... args) {
        if (level < get_log_level()) {
            return;
        }
        
        // Convert all arguments to strings
        std::string arg_strings[] = { to_string_helper(std::forward<Args>(args))... };
        
        // Replace {} placeholders with arguments
        std::string result = format;
        size_t arg_index = 0;
        size_t pos = 0;
        
        while ((pos = result.find("{}", pos)) != std::string::npos && arg_index < sizeof...(Args)) {
            result.replace(pos, 2, arg_strings[arg_index]);
            pos += arg_strings[arg_index].length();
            arg_index++;
        }
        
        log_message(level, result);
    }

    // Internal formatting functions are implemented in the .cpp file
}

// Convenient logging macros for easy use throughout the codebase

/**
 * @brief Log a trace message
 * @param msg The message to log (std::string or const char*)
 * 
 * Use for detailed debugging information that's typically only needed
 * during development or when diagnosing specific issues.
 * 
 * Example: LOG_TRACE("Entering function with parameter: " + paramValue);
 */
#define LOG_TRACE(msg) lumios::utils::log_message(lumios::utils::LogLevel::LUM_TRACE, msg)

/**
 * @brief Log a debug message
 * @param msg The message to log (std::string or const char*)
 * 
 * Use for debug information that helps understand program flow
 * and state during development.
 * 
 * Example: LOG_DEBUG("Processing " + std::to_string(count) + " items");
 */
#define LOG_DEBUG(msg) lumios::utils::log_message(lumios::utils::LogLevel::LUM_DEBUG, msg)
//#define LOG_DEBUG(msg)

/**
 * @brief Log an info message
 * @param msg The message to log (std::string or const char*)
 * 
 * Use for general information about program operation that's
 * useful for users and administrators.
 * 
 * Example: LOG_INFO("Server started on port " + std::to_string(port));
 */
#define LOG_INFO(msg) lumios::utils::log_message(lumios::utils::LogLevel::LUM_INFO, msg)

/**
 * @brief Log a warning message
 * @param msg The message to log (std::string or const char*)
 * 
 * Use for potentially problematic situations that don't prevent
 * the program from continuing but should be noted.
 * 
 * Example: LOG_WARN("Configuration file not found, using defaults");
 */
#define LOG_WARN(msg) lumios::utils::log_message(lumios::utils::LogLevel::LUM_WARN, msg)

/**
 * @brief Log an error message
 * @param msg The message to log (std::string or const char*)
 * 
 * Use for error conditions that are recoverable but indicate
 * something went wrong.
 * 
 * Example: LOG_ERROR("Failed to load texture: " + filename);
 */
#define LOG_ERROR(msg) lumios::utils::log_message(lumios::utils::LogLevel::LUM_ERROR, msg)

/**
 * @brief Log a fatal error message
 * @param msg The message to log (std::string or const char*)
 * 
 * Use for critical errors that may cause the program to terminate
 * or enter an unrecoverable state.
 * 
 * Example: LOG_FATAL("Out of memory - cannot continue");
 */
#define LOG_FATAL(msg) lumios::utils::log_message(lumios::utils::LogLevel::LUM_FATAL, msg)

// Formatted logging macros with argument support

/**
 * @brief Log a formatted trace message
 * @param fmt Format string with {} placeholders
 * @param ... Arguments to substitute into the format string
 * 
 * Example: LOG_TRACE_F("Function {} called with {} arguments", funcName, argCount);
 */
#define LOG_TRACE_F(fmt, ...) lumios::utils::log_formatted(lumios::utils::LogLevel::LUM_TRACE, fmt, __VA_ARGS__)

/**
 * @brief Log a formatted debug message
 * @param fmt Format string with {} placeholders
 * @param ... Arguments to substitute into the format string
 */
#define LOG_DEBUG_F(fmt, ...) lumios::utils::log_formatted(lumios::utils::LogLevel::LUM_DEBUG, fmt, __VA_ARGS__)

/**
 * @brief Log a formatted info message
 * @param fmt Format string with {} placeholders
 * @param ... Arguments to substitute into the format string
 */
#define LOG_INFO_F(fmt, ...) lumios::utils::log_formatted(lumios::utils::LogLevel::LUM_INFO, fmt, __VA_ARGS__)

/**
 * @brief Log a formatted warning message
 * @param fmt Format string with {} placeholders
 * @param ... Arguments to substitute into the format string
 */
#define LOG_WARN_F(fmt, ...) lumios::utils::log_formatted(lumios::utils::LogLevel::LUM_WARN, fmt, __VA_ARGS__)

/**
 * @brief Log a formatted error message
 * @param fmt Format string with {} placeholders
 * @param ... Arguments to substitute into the format string
 */
#define LOG_ERROR_F(fmt, ...) lumios::utils::log_formatted(lumios::utils::LogLevel::LUM_ERROR, fmt, __VA_ARGS__)

/**
 * @brief Log a formatted fatal message
 * @param fmt Format string with {} placeholders
 * @param ... Arguments to substitute into the format string
 */
#define LOG_FATAL_F(fmt, ...) lumios::utils::log_formatted(lumios::utils::LogLevel::LUM_FATAL, fmt, __VA_ARGS__)