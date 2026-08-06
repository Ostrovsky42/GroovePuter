#pragma once

#include <cstdint>

#ifdef ARDUINO
#include <M5Cardputer.h>
#endif

enum class WorkflowMode : uint8_t {
    Perform = 0,
    Generate,
    Hub,
    Song,
    Settings,
};

// Existing enum values are preserved for persisted UI-session compatibility.
// Their page labels are now the fixed four-axis GENERATE addresses.
enum class Workspace : uint8_t {
    // PERFORM
    Perform = 0,
    Player,

    // GENERATE
    Groove,       // GENRE
    Mode,         // GENERATION
    FeelTexture,  // TEXTURE

    // HUB
    Pattern,
    SynthA,
    SynthB,
    Drums,
    SynthAParameters,
    SynthBParameters,

    // SONG
    Arrange,

    // SETTINGS / fourth GENERATE page
    Project,
    Generator,    // FEEL
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
constexpr int kFeelTexture = 8;  // TEXTURE
constexpr int kGenerator = 9;    // FEEL
constexpr int kProject = 10;
constexpr int kMode = 11;        // GENERATION
constexpr int kPerform = 12;
constexpr int kPlayer = 13;

inline bool isPerformWorkflowPage(int page) {
    return page == kPerform || page == kPlayer;
}

inline bool isGenerateWorkflowPage(int page) {
    return page == kGenre ||
           page == kGenerator ||
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
    return page == kProject;
}

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
        case kGenerator: return Workspace::Generator;
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
        default: return Workspace::Groove;
    }
}

inline int pageForWorkspace(Workspace workspace) {
    switch (workspace) {
        case Workspace::Perform: return kPerform;
        case Workspace::Player: return kPlayer;
        case Workspace::Groove: return kGenre;
        case Workspace::Generator: return kGenerator;
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
        case kGenerator: return "FEEL";
        case kMode: return "GENERATION";
        case kFeelTexture: return "TEXTURE";
        case kPattern: return "OVERVIEW";
        case kSynthA: return "SYNTH A";
        case kSynthB: return "SYNTH B";
        case kDrums: return "DRUMS";
        case kSynthAParameters: return "SYNTH A SOUND";
        case kSynthBParameters: return "SYNTH B SOUND";
        case kArrange: return "SONG";
        case kProject: return "PROJECT / SETUP";
        default: return "PAGE";
    }
}

inline int pageCountForMode(WorkflowMode mode) {
    switch (mode) {
        case WorkflowMode::Perform: return 2;
        case WorkflowMode::Generate: return 4;
        case WorkflowMode::Hub: return 6;
        case WorkflowMode::Song: return 1;
        case WorkflowMode::Settings: return 1;
    }
    return 1;
}

inline int pageAt(WorkflowMode mode, int index) {
    static constexpr int kPerformPages[] = {
        kPerform, kPlayer,
    };
    static constexpr int kGeneratePages[] = {
        kGenre, kGenerator, kMode, kFeelTexture,
    };
    static constexpr int kHubPages[] = {
        kPattern, kSynthA, kSynthB, kDrums,
        kSynthAParameters, kSynthBParameters,
    };
    static constexpr int kSettingsPages[] = {
        kProject,
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

inline bool hardwareWorkflowModifierHeld() {
#ifdef ARDUINO
    return M5Cardputer.Keyboard.keysState().fn;
#else
    return false;
#endif
}

inline Workspace nextWorkspace(Workspace workspace,
                               int direction,
                               bool workflowModifier) {
    const int page = pageForWorkspace(workspace);
    const WorkflowMode mode = modeForPage(page);

    if (workflowModifier) {
        return workspaceForPage(pageForMode(nextMode(mode, direction)));
    }

    const int nextIndex = pageIndexInMode(page) + direction;
    return workspaceForPage(pageAt(mode, nextIndex));
}

inline Workspace nextWorkspace(Workspace workspace, int direction) {
    return nextWorkspace(workspace, direction, hardwareWorkflowModifierHeld());
}

inline bool allowsPerformanceKeyboard(int page) {
    return page == kPerform ||
           page == kSynthAParameters ||
           page == kFeelTexture;
}
}  // namespace WorkflowPages
