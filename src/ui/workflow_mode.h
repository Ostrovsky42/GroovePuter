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
// Generation/Texture resolve to FEEL. Legacy standalone synth SOUND pages now
// resolve to their owning synth track; sound editing is a local Tab hierarchy.
enum class Workspace : uint8_t {
    // PERFORM
    Perform = 0,
    Player,

    // GENERATE
    Groove,      // GENRE
    Generation,  // legacy persisted value -> FEEL
    Texture,     // legacy persisted value -> FEEL

    // HUB
    Pattern,
    SynthA,
    SynthB,
    Drums,
    SynthAParameters, // legacy persisted value -> SYNTH A
    SynthBParameters, // legacy persisted value -> SYNTH B

    // SONG
    Arrange,
    Phrase,

    // SETTINGS / GENERATE
    Project,
    Feel,
};

namespace WorkflowPages {
constexpr int kGenre = 0;
constexpr int kSynthA = 1;
constexpr int kSynthB = 2;
constexpr int kSynthAParameters = 3; // legacy persisted page id -> SYNTH A
constexpr int kSynthBParameters = 4; // legacy persisted page id -> SYNTH B
constexpr int kDrums = 5;
constexpr int kArrange = 6;
constexpr int kPattern = 7;
constexpr int kTexture = 8;     // legacy persisted page id -> FEEL
constexpr int kFeel = 9;
constexpr int kProject = 10;
constexpr int kGeneration = 11; // legacy persisted page id -> FEEL
constexpr int kPerform = 12;
constexpr int kPlayer = 13;
constexpr int kPhrase = 14;

inline int normalizeLegacyPage(int page) {
    if (page == kTexture || page == kGeneration) return kFeel;
    if (page == kSynthAParameters) return kSynthA;
    if (page == kSynthBParameters) return kSynthB;
    return page;
}

inline bool isPerformWorkflowPage(int page) {
    page = normalizeLegacyPage(page);
    return page == kPerform || page == kPlayer;
}

inline bool isGenerateWorkflowPage(int page) {
    page = normalizeLegacyPage(page);
    return page == kGenre || page == kFeel;
}

inline bool isHubWorkflowPage(int page) {
    page = normalizeLegacyPage(page);
    return page == kPattern ||
           page == kSynthA ||
           page == kSynthB ||
           page == kDrums;
}

inline bool isSettingsWorkflowPage(int page) {
    return normalizeLegacyPage(page) == kProject;
}

inline bool isPatternWorkspacePage(int page) {
    return isHubWorkflowPage(page);
}

inline bool isGrooveWorkspacePage(int page) {
    return isGenerateWorkflowPage(page);
}

inline bool isWorkspacePage(int page) {
    page = normalizeLegacyPage(page);
    return isPerformWorkflowPage(page) ||
           isGenerateWorkflowPage(page) ||
           isHubWorkflowPage(page) ||
           page == kArrange ||
           page == kPhrase ||
           isSettingsWorkflowPage(page);
}

inline Workspace workspaceForPage(int page) {
    page = normalizeLegacyPage(page);
    switch (page) {
        case kPerform: return Workspace::Perform;
        case kPlayer: return Workspace::Player;
        case kGenre: return Workspace::Groove;
        case kFeel: return Workspace::Feel;
        case kPattern: return Workspace::Pattern;
        case kSynthA: return Workspace::SynthA;
        case kSynthB: return Workspace::SynthB;
        case kDrums: return Workspace::Drums;
        case kArrange: return Workspace::Arrange;
        case kPhrase: return Workspace::Phrase;
        case kProject: return Workspace::Project;
        default: return Workspace::Groove;
    }
}

inline int pageForWorkspace(Workspace workspace) {
    switch (workspace) {
        case Workspace::Perform: return kPerform;
        case Workspace::Player: return kPlayer;
        case Workspace::Groove: return kGenre;
        case Workspace::Feel:
        case Workspace::Texture:
        case Workspace::Generation:
            return kFeel;
        case Workspace::Pattern: return kPattern;
        case Workspace::SynthA:
        case Workspace::SynthAParameters:
            return kSynthA;
        case Workspace::SynthB:
        case Workspace::SynthBParameters:
            return kSynthB;
        case Workspace::Drums: return kDrums;
        case Workspace::Arrange: return kArrange;
        case Workspace::Phrase: return kPhrase;
        case Workspace::Project: return kProject;
    }
    return kGenre;
}

inline WorkflowMode modeForPage(int page) {
    page = normalizeLegacyPage(page);
    if (isPerformWorkflowPage(page)) return WorkflowMode::Perform;
    if (isGenerateWorkflowPage(page)) return WorkflowMode::Generate;
    if (isHubWorkflowPage(page)) return WorkflowMode::Hub;
    if (page == kArrange || page == kPhrase) return WorkflowMode::Song;
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
    page = normalizeLegacyPage(page);
    switch (page) {
        case kPerform: return "MIDI KEYBOARD";
        case kPlayer: return "MIDI PLAYER";
        case kGenre: return "GENRE";
        case kFeel: return "FEEL";
        case kPattern: return "OVERVIEW";
        case kSynthA: return "SYNTH A";
        case kSynthB: return "SYNTH B";
        case kDrums: return "DRUMS";
        case kArrange: return "SONG";
        case kPhrase: return "PHRASE CORE";
        case kProject: return "PROJECT / SETUP";
        default: return "PAGE";
    }
}

inline int pageCountForMode(WorkflowMode mode) {
    switch (mode) {
        case WorkflowMode::Perform: return 2;
        case WorkflowMode::Generate: return 2;
        case WorkflowMode::Hub: return 4;
        case WorkflowMode::Song: return 2;
        case WorkflowMode::Settings: return 1;
    }
    return 1;
}

inline int pageAt(WorkflowMode mode, int index) {
    static constexpr int kPerformPages[] = {
        kPerform, kPlayer,
    };
    static constexpr int kGeneratePages[] = {
        kGenre, kFeel,
    };
    static constexpr int kHubPages[] = {
        kPattern, kSynthA, kSynthB, kDrums,
    };
    static constexpr int kSongPages[] = {
        kArrange, kPhrase,
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
        case WorkflowMode::Song: return kSongPages[index];
        case WorkflowMode::Settings: return kSettingsPages[index];
    }
    return kGenre;
}

inline int pageIndexInMode(int page) {
    page = normalizeLegacyPage(page);
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
    page = normalizeLegacyPage(page);
    return page == kPerform || page == kSynthA || page == kSynthB;
}
}  // namespace WorkflowPages
