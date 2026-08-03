#pragma once
#ifndef GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H
#define GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H

#include <atomic>
#include <cstdint>
#include <cstring>

namespace GroovePuterUi {

struct SmfPlayerSessionSnapshot {
    static constexpr std::size_t kPathCapacity = 128;

    char browserPath[kPathCapacity]{"/midi"};
    int16_t browserSelection{0};
    int16_t browserScroll{0};
    int16_t selectedTrack{0};
    int16_t inspectorScroll{0};
    bool browserVisible{true};
    bool performanceVisible{false};
    bool inspectorVisible{false};
};

class SmfPlayerSessionState {
public:
    SmfPlayerSessionSnapshot snapshot() const {
        SmfPlayerSessionSnapshot result{};
        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint32_t first = version_.load(std::memory_order_acquire);
            if ((first & 1u) != 0u) continue;
            result = state_;
            const uint32_t second = version_.load(std::memory_order_acquire);
            if (first == second && (second & 1u) == 0u) return result;
        }
        return SmfPlayerSessionSnapshot{};
    }

    void publish(const SmfPlayerSessionSnapshot& next) {
        const uint32_t version = version_.load(std::memory_order_relaxed);
        version_.store(version + 1u, std::memory_order_release);
        state_ = sanitize(next);
        version_.store(version + 2u, std::memory_order_release);
    }

private:
    static SmfPlayerSessionSnapshot sanitize(
            const SmfPlayerSessionSnapshot& input) {
        SmfPlayerSessionSnapshot result = input;
        result.browserPath[SmfPlayerSessionSnapshot::kPathCapacity - 1u] = '\0';
        if (result.browserPath[0] != '/') {
            std::strncpy(result.browserPath, "/midi",
                         SmfPlayerSessionSnapshot::kPathCapacity - 1u);
            result.browserPath[SmfPlayerSessionSnapshot::kPathCapacity - 1u] = '\0';
        }
        if (result.browserSelection < 0) result.browserSelection = 0;
        if (result.browserScroll < 0) result.browserScroll = 0;
        if (result.selectedTrack < 0) result.selectedTrack = 0;
        if (result.inspectorScroll < 0) result.inspectorScroll = 0;
        return result;
    }

    std::atomic<uint32_t> version_{0};
    SmfPlayerSessionSnapshot state_{};
};

inline SmfPlayerSessionState& smfPlayerSessionState() {
    static SmfPlayerSessionState state;
    return state;
}

}  // namespace GroovePuterUi

#endif  // GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H
