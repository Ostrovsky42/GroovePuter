#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_CONFIG_SAVE_TIMER_H
#define GROOVEPUTER_PERFORMANCE_CONFIG_SAVE_TIMER_H

#include <cstdint>

class PerformanceConfigSaveTimer {
public:
    static constexpr uint32_t kQuietMs = 700;

    void changed(uint32_t nowMs) {
        pending_ = true;
        changedMs_ = nowMs;
    }

    bool due(uint32_t nowMs) const {
        return pending_ &&
            static_cast<uint32_t>(nowMs - changedMs_) >= kQuietMs;
    }

    void saved() { pending_ = false; }
    void retryAfter(uint32_t nowMs) { changedMs_ = nowMs; }

private:
    bool pending_{false};
    uint32_t changedMs_{0};
};

#endif  // GROOVEPUTER_PERFORMANCE_CONFIG_SAVE_TIMER_H
