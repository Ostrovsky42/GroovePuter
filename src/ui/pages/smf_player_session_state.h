#pragma once
#ifndef GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H
#define GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace GroovePuterUi {

struct SmfPlayerSessionSnapshot {
    static constexpr std::size_t kPathCapacity = 128;

    char browserPath[kPathCapacity]{"/midi"};
    int16_t browserSelection{0};
    int16_t browserScroll{0};
    int16_t inspectorScroll{0};
    bool browserVisible{true};
    bool performanceVisible{false};
    bool inspectorVisible{false};
    bool valid{false};
};

enum class SmfPlayerSessionFlag : uint8_t {
    BrowserVisible = 0,
    PerformanceVisible,
    InspectorVisible,
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
        state_.valid = true;
        version_.store(version + 2u, std::memory_order_release);
    }

    void publishFlag(SmfPlayerSessionFlag flag, bool value) {
        SmfPlayerSessionSnapshot next = snapshot();
        switch (flag) {
            case SmfPlayerSessionFlag::BrowserVisible:
                next.browserVisible = value;
                break;
            case SmfPlayerSessionFlag::PerformanceVisible:
                next.performanceVisible = value;
                break;
            case SmfPlayerSessionFlag::InspectorVisible:
                next.inspectorVisible = value;
                break;
        }
        publish(next);
    }

    void setActive(bool active) {
        const bool previous = active_.exchange(active, std::memory_order_acq_rel);
        if (active && !previous) {
            activationEpoch_.fetch_add(1u, std::memory_order_acq_rel);
        }
    }

    bool active() const {
        return active_.load(std::memory_order_acquire);
    }

    uint32_t activationEpoch() const {
        return activationEpoch_.load(std::memory_order_acquire);
    }

private:
    static SmfPlayerSessionSnapshot sanitize(
            const SmfPlayerSessionSnapshot& input) {
        SmfPlayerSessionSnapshot result = input;
        result.browserPath[SmfPlayerSessionSnapshot::kPathCapacity - 1u] = '\0';
        if (result.browserPath[0] != '/') {
            std::snprintf(result.browserPath,
                          SmfPlayerSessionSnapshot::kPathCapacity,
                          "%s", "/midi");
        }
        if (result.browserSelection < 0) result.browserSelection = 0;
        if (result.browserScroll < 0) result.browserScroll = 0;
        if (result.inspectorScroll < 0) result.inspectorScroll = 0;
        return result;
    }

    std::atomic<uint32_t> version_{0};
    std::atomic<uint32_t> activationEpoch_{0};
    std::atomic<bool> active_{false};
    SmfPlayerSessionSnapshot state_{};
};

inline SmfPlayerSessionState& smfPlayerSessionState() {
    static SmfPlayerSessionState state;
    return state;
}

// Bool-compatible page field that mirrors visibility changes into the bounded
// session snapshot. The restored browser view ignores exactly one assignment:
// SmfPlayerPage::onEnter() derives a default from player state, but must not
// overwrite a valid user session after page eviction.
class SmfPlayerTrackedFlag {
public:
    SmfPlayerTrackedFlag(SmfPlayerSessionFlag flag, bool defaultValue)
        : flag_(flag), value_(defaultValue) {}

    operator bool() const { return value_; }
    bool operator!() const { return !value_; }

    SmfPlayerTrackedFlag& operator=(bool value) {
        smfPlayerSessionState().setActive(true);
        if (ignoreNextAssignment_) {
            ignoreNextAssignment_ = false;
            return *this;
        }
        value_ = value;
        smfPlayerSessionState().publishFlag(flag_, value_);
        return *this;
    }

    void restore(bool value, bool ignoreNextAssignment = false) {
        value_ = value;
        ignoreNextAssignment_ = ignoreNextAssignment;
    }

    bool value() const { return value_; }

private:
    SmfPlayerSessionFlag flag_;
    bool value_{false};
    bool ignoreNextAssignment_{false};
};

// RAII binding keeps session persistence out of draw/input code. Page creation
// restores bounded state; page eviction publishes the final state. onExit()
// separately marks the page inactive so cached previous pages cannot intercept
// another page's content clear.
class SmfPlayerSessionBinding {
public:
    SmfPlayerSessionBinding(std::string& path,
                            int& selection,
                            int& scroll,
                            int& inspectorScroll,
                            SmfPlayerTrackedFlag& browserVisible,
                            SmfPlayerTrackedFlag& performanceVisible,
                            SmfPlayerTrackedFlag& inspectorVisible)
        : path_(path),
          selection_(selection),
          scroll_(scroll),
          inspectorScroll_(inspectorScroll),
          browserVisible_(browserVisible),
          performanceVisible_(performanceVisible),
          inspectorVisible_(inspectorVisible) {
        const SmfPlayerSessionSnapshot saved =
            smfPlayerSessionState().snapshot();
        if (saved.valid) {
            path_ = saved.browserPath;
            selection_ = saved.browserSelection;
            scroll_ = saved.browserScroll;
            inspectorScroll_ = saved.inspectorScroll;
            browserVisible_.restore(saved.browserVisible, true);
            performanceVisible_.restore(saved.performanceVisible);
            inspectorVisible_.restore(saved.inspectorVisible);
        }
        smfPlayerSessionState().setActive(true);
    }

    ~SmfPlayerSessionBinding() {
        publish();
        smfPlayerSessionState().setActive(false);
    }

    void publish() const {
        SmfPlayerSessionSnapshot next{};
        std::snprintf(next.browserPath,
                      SmfPlayerSessionSnapshot::kPathCapacity,
                      "%s", path_.c_str());
        next.browserSelection = clampInt16(selection_);
        next.browserScroll = clampInt16(scroll_);
        next.inspectorScroll = clampInt16(inspectorScroll_);
        next.browserVisible = browserVisible_.value();
        next.performanceVisible = performanceVisible_.value();
        next.inspectorVisible = inspectorVisible_.value();
        next.valid = true;
        smfPlayerSessionState().publish(next);
    }

    void setActive(bool active) {
        if (!active) publish();
        smfPlayerSessionState().setActive(active);
    }

private:
    static int16_t clampInt16(int value) {
        if (value < 0) return 0;
        if (value > 32767) return 32767;
        return static_cast<int16_t>(value);
    }

    std::string& path_;
    int& selection_;
    int& scroll_;
    int& inspectorScroll_;
    SmfPlayerTrackedFlag& browserVisible_;
    SmfPlayerTrackedFlag& performanceVisible_;
    SmfPlayerTrackedFlag& inspectorVisible_;
};

}  // namespace GroovePuterUi

#endif  // GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H
