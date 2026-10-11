#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_CONFIG_SAVE_TIMER_H
#define GROOVEPUTER_PERFORMANCE_CONFIG_SAVE_TIMER_H

#include <cstdint>

// Debounce for saving the PERFORM keyboard configuration. The first change starts the quiet
// window; a save is due once the record has been changed and left alone for kQuietMs.
// Owned by the loop, not by a page, so MIDI and page edits persist the same way.
class PerformanceConfigSaveTimer {
public:
    static constexpr uint32_t kQuietMs = 700;

    // Returns true when the caller should save now.
    bool due(bool changed, uint32_t nowMs) {
        if (!changed) {
            pending_ = false;
            return false;
        }
        if (!pending_) {
            pending_ = true;
            changedMs_ = nowMs;
            return false;
        }
        return static_cast<uint32_t>(nowMs - changedMs_) >= kQuietMs;
    }

    // Save succeeded: the caller has marked the keyboard persisted.
    void saved() { pending_ = false; }

    // Save failed: wait another quiet window before trying again.
    void retryAfter(uint32_t nowMs) { changedMs_ = nowMs; }

private:
    bool pending_{false};
    uint32_t changedMs_{0};
};

#endif  // GROOVEPUTER_PERFORMANCE_CONFIG_SAVE_TIMER_H
