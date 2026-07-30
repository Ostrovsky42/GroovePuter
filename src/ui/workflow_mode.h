#pragma once

#include <cstdint>

enum class WorkflowMode : uint8_t {
    Perform,
    Pattern,
    Arrange,
};

namespace WorkflowPages {
constexpr int kGenre = 0;
constexpr int kSynthAParameters = 3;
constexpr int kArrange = 6;
constexpr int kPattern = 7;
constexpr int kFeelTexture = 8;
constexpr int kPerform = 12;

inline WorkflowMode modeForPage(int page) {
    if (page == kPerform) return WorkflowMode::Perform;
    if (page == kArrange) return WorkflowMode::Arrange;
    return WorkflowMode::Pattern;
}

inline int pageForMode(WorkflowMode mode) {
    switch (mode) {
        case WorkflowMode::Perform: return kPerform;
        case WorkflowMode::Pattern: return kPattern;
        case WorkflowMode::Arrange: return kArrange;
    }
    return kPerform;
}

inline WorkflowMode nextMode(WorkflowMode mode, int direction) {
    int value = static_cast<int>(mode) + direction;
    constexpr int count = 3;
    while (value < 0) value += count;
    while (value >= count) value -= count;
    return static_cast<WorkflowMode>(value);
}

inline bool allowsPerformanceKeyboard(int page) {
    return page == kPerform ||
           page == kSynthAParameters ||
           page == kFeelTexture;
}
}  // namespace WorkflowPages
