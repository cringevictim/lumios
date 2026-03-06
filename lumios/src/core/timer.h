#pragma once

#include <chrono>

namespace lumios {

class Timer {
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint start_;
    TimePoint last_tick_;
    float delta_time_ = 0.0f;
    float total_time_ = 0.0f;
    u64 frame_count_ = 0;

public:
    void reset() {
        start_ = Clock::now();
        last_tick_ = start_;
        delta_time_ = 0.0f;
        total_time_ = 0.0f;
        frame_count_ = 0;
    }

    void tick() {
        auto now = Clock::now();
        delta_time_ = std::chrono::duration<float>(now - last_tick_).count();
        total_time_ = std::chrono::duration<float>(now - start_).count();
        last_tick_ = now;
        frame_count_++;
    }

    float delta()       const { return delta_time_; }
    float total()       const { return total_time_; }
    u64   frame_count() const { return frame_count_; }
    float fps()         const { return delta_time_ > 0.0f ? 1.0f / delta_time_ : 0.0f; }

    using u64 = uint64_t;
};

} // namespace lumios
