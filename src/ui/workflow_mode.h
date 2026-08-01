#pragma once

#include <cstdint>

enum class WorkflowMode : uint8_t {
    Perform = 0,
    Generate,
    Hub,
    Song,
    Settings,
};

// Workspace stores the active page inside a workflow. Keeping this state
// page-aware lets the existing MiniAcidDisplay nextPage()/previousPage()
// methods restore [ / ] navigation without adding another navigation owner.
enum class Workspace : uint8_t {
    // PERFORM
    Perform = 0,
    Player,

    // GENERATE
    Groove,
    Mode,
    FeelTexture,

    // HUB
    Pattern,
    SynthA,
    SynthB,
    Drums,
    SynthAParameters,
    SynthBParameters,

    // SONG
    Arrange,

    // SETTINGS
    Project,
    Generator,
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

inline bool isPerformWorkflowPage(int page) {
    return page == kPerform || page == kPlayer;
}

inline bool isGenerateWorkflowPage(int page) {
    return page == kGenre ||
           page == kMode ||
           page == kFeelTexture;
}

inline bool isHubWorkflowPage(int page) {
    return page == kPattern ||
           page == kSynthA ||
           page == kSynthB ||
           page == kDrums ||
           page == kSynthAParameters ||
           page == kSynthBParameters;
}

inline bool isSettingsWorkflowPage(int page) {
    return page == kProject || page == kGenerator;
}

// Compatibility names used by older source-level checks and call sites.
inline bool isPatternWorkspacePage(int page) {
    return isHubWorkflowPage(page);
}

inline bool isGrooveWorkspacePage(int page) {
    return isGenerateWorkflowPage(page);
}

inline bool isWorkspacePage(int page) {
    return isPerformWorkflowPage(page) ||
           isGenerateWorkflowPage(page) ||
           isHubWorkflowPage(page) ||
           page == kArrange ||
           isSettingsWorkflowPage(page);
}

inline Workspace workspaceForPage(int page) {
    switch (page) {
        case kPerform: return Workspace::Perform;
        case kPlayer: return Workspace::Player;
        case kGenre: return Workspace::Groove;
        case kMode: return Workspace::Mode;
        case kFeelTexture: return Workspace::FeelTexture;
        case kPattern: return Workspace::Pattern;
        case kSynthA: return Workspace::SynthA;
        case kSynthB: return Workspace::SynthB;
        case kDrums: return Workspace::Drums;
        case kSynthAParameters: return Workspace::SynthAParameters;
        case kSynthBParameters: return Workspace::SynthBParameters;
        case kArrange: return Workspace::Arrange;
        case kProject: return Workspace::Project;
        case kGenerator: return Workspace::Generator;
        default: return Workspace::Groove;
    }
}

inline int pageForWorkspace(Workspace workspace) {
    switch (workspace) {
        case Workspace::Perform: return kPerform;
        case Workspace::Player: return kPlayer;
        case Workspace::Groove: return kGenre;
        case Workspace::Mode: return kMode;
        case Workspace::FeelTexture: return kFeelTexture;
        case Workspace::Pattern: return kPattern;
        case Workspace::SynthA: return kSynthA;
        case Workspace::SynthB: return kSynthB;
        case Workspace::Drums: return kDrums;
        case Workspace::SynthAParameters: return kSynthAParameters;
        case Workspace::SynthBParameters: return kSynthBParameters;
        case Workspace::Arrange: return kArrange;
        case Workspace::Project: return kProject;
        case Workspace::Generator: return kGenerator;
    }
    return kGenre;
}

inline WorkflowMode modeForPage(int page) {
    if (isPerformWorkflowPage(page)) return WorkflowMode::Perform;
    if (isGenerateWorkflowPage(page)) return WorkflowMode::Generate;
    if (isHubWorkflowPage(page)) return WorkflowMode::Hub;
    if (page == kArrange) return WorkflowMode::Song;
    return WorkflowMode::Settings;
}

inline WorkflowMode modeForWorkspace(Workspace workspace) {
    return modeForPage(pageForWorkspace(workspace));
}

inline const char* workflowName(WorkflowMode mode) {
    switch (mode) {
        case WorkflowMode::Perform: return "PERFORM";
        case WorkflowMode::Generate: return "GENERATE";
        case WorkflowMode::Hub: return "HUB";
        case WorkflowMode::Song: return "SONG";
        case WorkflowMode::Settings: return "SETTINGS";
    }
    return "GENERATE";
}

inline const char* workspaceName(Workspace workspace) {
    return workflowName(modeForWorkspace(workspace));
}

inline const char* pageName(int page) {
    switch (page) {
        case kPerform: return "MIDI KEYBOARD";
        case kPlayer: return "MIDI PLAYER";
        case kGenre: return "GENRE";
        case kMode: return "MODE / FLAVOR";
        case kFeelTexture: return "FEEL / TEXTURE";
        case kPattern: return "OVERVIEW";
        case kSynthA: return "SYNTH A";
        case kSynthB: return "SYNTH B";
        case kDrums: return "DRUMS";
        case kSynthAParameters: return "SYNTH A SOUND";
        case kSynthBParameters: return "SYNTH B SOUND";
        case kArrange: return "SONG";
        case kProject: return "PROJECT / SETUP";
        case kGenerator: return "ADV GENERATOR";
        default: return "PAGE";
    }
}

inline int pageCountForMode(WorkflowMode mode) {
    switch (mode) {
        case WorkflowMode::Perform: return 2;
        case WorkflowMode::Generate: return 3;
        case WorkflowMode::Hub: return 6;
        case WorkflowMode::Song: return 1;
        case WorkflowMode::Settings: return 2;
    }
    return 1;
}

inline int pageAt(WorkflowMode mode, int index) {
    static constexpr int kPerformPages[] = {
        kPerform, kPlayer,
    };
    static constexpr int kGeneratePages[] = {
        kGenre, kMode, kFeelTexture,
    };
    static constexpr int kHubPages[] = {
        kPattern, kSynthA, kSynthB, kDrums,
        kSynthAParameters, kSynthBParameters,
    };
    static constexpr int kSettingsPages[] = {
        kProject, kGenerator,
    };

    const int count = pageCountForMode(mode);
    while (index < 0) index += count;
    while (index >= count) index -= count;

    switch (mode) {
        case WorkflowMode::Perform: return kPerformPages[index];
        case WorkflowMode::Generate: return kGeneratePages[index];
        case WorkflowMode::Hub: return kHubPages[index];
        case WorkflowMode::Song: return kArrange;
        case WorkflowMode::Settings: return kSettingsPages[index];
    }
    return kGenre;
}

inline int pageIndexInMode(int page) {
    const WorkflowMode mode = modeForPage(page);
    const int count = pageCountForMode(mode);
    for (int index = 0; index < count; ++index) {
        if (pageAt(mode, index) == page) return index;
    }
    return 0;
}

inline WorkflowMode nextMode(WorkflowMode mode, int direction) {
    int value = static_cast<int>(mode) + direction;
    constexpr int count = 5;
    while (value < 0) value += count;
    while (value >= count) value -= count;
    return static_cast<WorkflowMode>(value);
}

inline int pageForMode(WorkflowMode mode) {
    return pageAt(mode, 0);
}

inline Workspace nextWorkspace(Workspace workspace, int direction) {
    const int page = pageForWorkspace(workspace);
    const WorkflowMode mode = modeForPage(page);
    const int index = pageIndexInMode(page);
    const int count = pageCountForMode(mode);

    if (direction < 0 && index == 0) {
        return workspaceForPage(pageForMode(nextMode(mode, -1)));
    }
    if (direction > 0 && index == count - 1) {
        return workspaceForPage(pageForMode(nextMode(mode, 1)));
    }

    return workspaceForPage(pageAt(mode, index + direction));
}

inline bool allowsPerformanceKeyboard(int page) {
    return page == kPerform ||
           page == kSynthAParameters ||
           page == kFeelTexture;
}
}  // namespace WorkflowPages
