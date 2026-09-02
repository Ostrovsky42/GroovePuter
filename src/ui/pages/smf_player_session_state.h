#pragma once
#ifndef GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H
#define GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace GroovePuterUi {

inline constexpr std::size_t kSmfPlayerSessionBrowserRows = 7;

struct SmfPlayerSessionBrowserRow {
    int16_t logicalIndex{-1};
    char displayName[40]{};
};

struct SmfPlayerSessionSnapshot {
    static constexpr std::size_t kPathCapacity = 128;

    char browserPath[kPathCapacity]{"/midi"};
    std::array<SmfPlayerSessionBrowserRow,
               kSmfPlayerSessionBrowserRows> browserRows{};
    int16_t browserSelection{0};
    int16_t browserScroll{0};
    int16_t inspectorScroll{0};
    int16_t directoryCount{0};
    int16_t fileCount{0};
    int16_t totalEntries{0};
    int16_t visibleWindowStart{-1};
    bool browserStorageReady{false};
    bool browserCacheValid{false};
    bool browserVisible{true};
    bool performanceVisible{false};
    bool inspectorVisible{false};
    bool valid{false};
};

static_assert(sizeof(SmfPlayerSessionSnapshot) <= 512,
              "SMF page session cache must remain a small bounded snapshot");

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
    static int16_t clampNonNegative(int16_t value) {
        return value < 0 ? 0 : value;
    }

    static SmfPlayerSessionSnapshot sanitize(
            const SmfPlayerSessionSnapshot& input) {
        SmfPlayerSessionSnapshot result = input;
        result.browserPath[SmfPlayerSessionSnapshot::kPathCapacity - 1u] = '\0';
        if (result.browserPath[0] != '/') {
            std::snprintf(result.browserPath,
                          SmfPlayerSessionSnapshot::kPathCapacity,
                          "%s", "/midi");
        }
        result.browserSelection = clampNonNegative(result.browserSelection);
        result.browserScroll = clampNonNegative(result.browserScroll);
        result.inspectorScroll = clampNonNegative(result.inspectorScroll);
        result.directoryCount = clampNonNegative(result.directoryCount);
        result.fileCount = clampNonNegative(result.fileCount);
        result.totalEntries = clampNonNegative(result.totalEntries);
        if (result.visibleWindowStart < -1) result.visibleWindowStart = -1;
        for (SmfPlayerSessionBrowserRow& row : result.browserRows) {
            if (row.logicalIndex < -1) row.logicalIndex = -1;
            row.displayName[sizeof(row.displayName) - 1u] = '\0';
        }
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
// overwrite a valid user session after page eviction or a cached-page return.
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

    void ignoreNextAssignment() {
        ignoreNextAssignment_ = true;
    }

    bool value() const { return value_; }

private:
    SmfPlayerSessionFlag flag_;
    bool value_{false};
    bool ignoreNextAssignment_{false};
};

// RAII binding keeps session persistence out of draw/input code. Page creation
// restores bounded state; page eviction publishes the final state. Only the
// current seven-row browser window is cached, avoiding a second full directory
// model and keeping static RAM below 512 bytes.
class SmfPlayerSessionBinding {
public:
    using BrowserRows = std::array<SmfPlayerSessionBrowserRow,
                                   kSmfPlayerSessionBrowserRows>;

    SmfPlayerSessionBinding(std::string& path,
                            BrowserRows& browserRows,
                            int& directoryCount,
                            int& fileCount,
                            int& totalEntries,
                            int& visibleWindowStart,
                            bool& browserStorageReady,
                            int& selection,
                            int& scroll,
                            int& inspectorScroll,
                            SmfPlayerTrackedFlag& browserVisible,
                            SmfPlayerTrackedFlag& performanceVisible,
                            SmfPlayerTrackedFlag& inspectorVisible)
        : path_(path),
          browserRows_(browserRows),
          directoryCount_(directoryCount),
          fileCount_(fileCount),
          totalEntries_(totalEntries),
          visibleWindowStart_(visibleWindowStart),
          browserStorageReady_(browserStorageReady),
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
            if (saved.browserCacheValid) {
                browserRows_ = saved.browserRows;
                directoryCount_ = saved.directoryCount;
                fileCount_ = saved.fileCount;
                totalEntries_ = saved.totalEntries;
                visibleWindowStart_ = saved.visibleWindowStart;
                browserStorageReady_ = saved.browserStorageReady;
            }
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
        next.browserRows = browserRows_;
        next.directoryCount = clampInt16(directoryCount_);
        next.fileCount = clampInt16(fileCount_);
        next.totalEntries = clampInt16(totalEntries_);
        next.visibleWindowStart = clampWindowStart(visibleWindowStart_);
        next.browserStorageReady = browserStorageReady_;
        next.browserCacheValid = browserStorageReady_;
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
        if (!active) {
            publish();
            // onEnter() always writes its default browser mode. Ignore that
            // first assignment when returning to a cached page so the user's
            // current browser/player choice survives ordinary navigation too.
            browserVisible_.ignoreNextAssignment();
        }
        smfPlayerSessionState().setActive(active);
    }

private:
    static int16_t clampInt16(int value) {
        if (value < 0) return 0;
        if (value > 32767) return 32767;
        return static_cast<int16_t>(value);
    }

    static int16_t clampWindowStart(int value) {
        if (value < -1) return -1;
        if (value > 32767) return 32767;
        return static_cast<int16_t>(value);
    }

    std::string& path_;
    BrowserRows& browserRows_;
    int& directoryCount_;
    int& fileCount_;
    int& totalEntries_;
    int& visibleWindowStart_;
    bool& browserStorageReady_;
    int& selection_;
    int& scroll_;
    int& inspectorScroll_;
    SmfPlayerTrackedFlag& browserVisible_;
    SmfPlayerTrackedFlag& performanceVisible_;
    SmfPlayerTrackedFlag& inspectorVisible_;
};

}  // namespace GroovePuterUi

#endif  // GROOVEPUTER_SMF_PLAYER_SESSION_STATE_H
