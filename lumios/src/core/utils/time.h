/**
 * @file time.h
 * @brief Time utilities for the Lumios engine
 * 
 * This module provides time-related functionality including:
 * - High-resolution timestamps
 * - Formatted time strings
 * - Cross-platform time operations
 * 
 * @author Lumios Engine Team
 * @version 1.0
 * @date 2025
 */

#pragma once

#include "defines.h"
#include <string>
#include <chrono>

namespace lumios::utils {

    /**
     * @brief Get current timestamp as formatted string
     * @return Timestamp string in format "HH:MM:SS.mmm"
     * 
     * Returns a high-precision timestamp with millisecond accuracy.
     * Uses the system's high-resolution clock for precise timing.
     * 
     * Example output: "14:32:05.123"
     */
    inline std::string get_timestamp() {
        using namespace std::chrono;
        
        // Get current time
        auto now = system_clock::now();
        auto now_time_t = system_clock::to_time_t(now);
        auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        
        // Convert to local time
        std::tm local_time;
#ifdef _WIN32
        localtime_s(&local_time, &now_time_t);
#else
        localtime_r(&now_time_t, &local_time);
#endif
        
        // Format the timestamp
        char buffer[32];
        char time_buffer[16];
        
        // Format HH:MM:SS
#ifdef _WIN32
        sprintf_s(time_buffer, sizeof(time_buffer), "%02d:%02d:%02d",
                  local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
        sprintf_s(buffer, sizeof(buffer), "%s.%03d",
                  time_buffer, static_cast<int>(now_ms.count()));
#else
        sprintf(time_buffer, "%02d:%02d:%02d",
                local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
        sprintf(buffer, "%s.%03d",
                time_buffer, static_cast<int>(now_ms.count()));
#endif
        
        return std::string(buffer);
    }

    /**
     * @brief Get high-resolution timestamp in milliseconds since epoch
     * @return Number of milliseconds since Unix epoch
     * 
     * Useful for performance measurements and timing operations.
     */
    inline int64_t get_time_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();
    }

    /**
     * @brief Get high-resolution timestamp in microseconds since epoch
     * @return Number of microseconds since Unix epoch
     * 
     * Useful for high-precision performance measurements.
     */
    inline int64_t get_time_us() {
        using namespace std::chrono;
        return duration_cast<microseconds>(
            system_clock::now().time_since_epoch()
        ).count();
    }

} // namespace lumios::utils

