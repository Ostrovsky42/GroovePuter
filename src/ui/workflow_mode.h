#pragma once

#include <cstdint>

enum class WorkflowMode : uint8_t {
    Perform,
    Pattern,
    Arrange,
};

enum class Workspace : uint8_t {
    Perform = 0,
    Pattern,
    Arrange,
    Player,
    Groove,
};

namespace WorkflowPages {
constexpr int kGenre = 0;
constexpr int kSynthA = 1;
constexpr int kSynthB = 2;
constexpr int kSynthAParameters = 3;
constexpr int kSynthBParameters = 4;
constexpr int kDrums = 5;
constexpr int kArrange = 6;
constexpr int kPattern = 7;
constexpr int kFeelTexture = 8;
constexpr int kGenerator = 9;
constexpr int kProject = 10;
constexpr int kMode = 11;
constexpr int kPerform = 12;
constexpr int kPlayer = 13;

inline bool isPatternWorkspacePage(int page) {
    return page == kSynthA ||
           page == kSynthB ||
           page == kSynthAParameters ||
           page == kSynthBParameters ||
           page == kDrums ||
           page == kPattern;
}

inline bool isGrooveWorkspacePage(int page) {
    return page == kGenre ||
           page == kFeelTexture ||
           page == kGenerator ||
           page == kMode;
}

inline bool isWorkspacePage(int page) {
    return page == kPerform ||
           page == kArrange ||
           page == kPlayer ||
           isPatternWorkspacePage(page) ||
           isGrooveWorkspacePage(page);
}

inline Workspace workspaceForPage(int page) {
    if (page == kPerform) return Workspace::Perform;
    if (page == kArrange) return Workspace::Arrange;
    if (page == kPlayer) return Workspace::Player;
    if (isGrooveWorkspacePage(page)) return Workspace::Groove;
    return Workspace::Pattern;
}

inline int pageForWorkspace(Workspace workspace) {
    switch (workspace) {
        case Workspace::Perform: return kPerform;
        case Workspace::Pattern: return kPattern;
        case Workspace::Arrange: return kArrange;
        case Workspace::Player: return kPlayer;
        case Workspace::Groove: return kGenre;
    }
    return kPerform;
}

inline const char* workspaceName(Workspace workspace) {
    switch (workspace) {
        case Workspace::Perform: return "PERFORM";
        case Workspace::Pattern: return "PATTERN";
        case Workspace::Arrange: return "ARRANGE";
        case Workspace::Player: return "PLAYER";
        case Workspace::Groove: return "GROOVE";
    }
    return "PERFORM";
}

inline Workspace nextWorkspace(Workspace workspace, int direction) {
    int value = static_cast<int>(workspace) + direction;
    constexpr int count = 5;
    while (value < 0) value += count;
    while (value >= count) value -= count;
    return static_cast<Workspace>(value);
}

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
