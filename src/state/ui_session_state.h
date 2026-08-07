#pragma once

#include <cstddef>
#include <cstdint>

// Compatibility forward declaration for existing UI call sites.
enum class WorkflowMode : uint8_t;

namespace GroovePuterState {

enum class SessionWorkflow : uint8_t {
    Perform = 0,
    Generate,
    Hub,
    Song,
    Settings,
};

constexpr int kWorkflowSessionCount = 5;
constexpr int kUiPageCount = 15;
constexpr uint16_t kDefaultMasterVolumePermille = 600;
constexpr uint16_t kMaxMasterVolumePermille = 1800;

namespace SessionPages {
constexpr int kGenre = 0;
constexpr int kSynthA = 1;
constexpr int kSynthB = 2;
constexpr int kSynthAParameters = 3;
constexpr int kSynthBParameters = 4;
constexpr int kDrums = 5;
constexpr int kArrange = 6;
constexpr int kPattern = 7;
constexpr int kTexture = 8;  // legacy persisted page id; resolves to FEEL
constexpr int kFeel = 9;
constexpr int kProject = 10;
constexpr int kGeneration = 11;
constexpr int kPerform = 12;
constexpr int kPlayer = 13;
constexpr int kPhrase = 14;
}  // namespace SessionPages

inline int normalizeLegacyUiPage(int page) {
    return page == SessionPages::kTexture ? SessionPages::kFeel : page;
}

struct UiSessionState {
    int8_t activePage{static_cast<int8_t>(SessionPages::kGenre)};
    int8_t lastPageByWorkflow[kWorkflowSessionCount]{
        static_cast<int8_t>(SessionPages::kPerform),
        static_cast<int8_t>(SessionPages::kGenre),
        static_cast<int8_t>(SessionPages::kPattern),
        static_cast<int8_t>(SessionPages::kArrange),
        static_cast<int8_t>(SessionPages::kProject),
    };
    uint8_t visualStyle{2};
    uint8_t waveformOverlayEnabled{1};
    uint16_t masterVolumePermille{kDefaultMasterVolumePermille};
};

static_assert(sizeof(UiSessionState) <= 12,
              "UI session state must remain a tiny fixed-size payload");

inline int workflowSessionIndex(SessionWorkflow workflow) {
    const int value = static_cast<int>(workflow);
    return value >= 0 && value < kWorkflowSessionCount ? value : 0;
}

inline bool validUiPage(int page) {
    return page >= 0 && page < kUiPageCount;
}

inline SessionWorkflow sessionWorkflowForPage(int page) {
    page = normalizeLegacyUiPage(page);
    if (page == SessionPages::kPerform || page == SessionPages::kPlayer) {
        return SessionWorkflow::Perform;
    }
    if (page == SessionPages::kGenre ||
        page == SessionPages::kFeel ||
        page == SessionPages::kGeneration) {
        return SessionWorkflow::Generate;
    }
    if (page == SessionPages::kPattern || page == SessionPages::kSynthA ||
        page == SessionPages::kSynthB || page == SessionPages::kDrums ||
        page == SessionPages::kSynthAParameters ||
        page == SessionPages::kSynthBParameters) {
        return SessionWorkflow::Hub;
    }
    if (page == SessionPages::kArrange ||
        page == SessionPages::kPhrase) {
        return SessionWorkflow::Song;
    }
    return SessionWorkflow::Settings;
}

inline int defaultPageForWorkflow(SessionWorkflow workflow) {
    switch (workflow) {
        case SessionWorkflow::Perform: return SessionPages::kPerform;
        case SessionWorkflow::Generate: return SessionPages::kGenre;
        case SessionWorkflow::Hub: return SessionPages::kPattern;
        case SessionWorkflow::Song: return SessionPages::kArrange;
        case SessionWorkflow::Settings: return SessionPages::kProject;
    }
    return SessionPages::kGenre;
}

inline bool pageBelongsToWorkflow(int page, SessionWorkflow workflow) {
    page = normalizeLegacyUiPage(page);
    return validUiPage(page) && sessionWorkflowForPage(page) == workflow;
}

inline uint8_t sanitizeVisualStyle(uint8_t value) {
    return value == 2 || value == 3 ? value : 0;
}

inline uint16_t clampMasterVolumePermille(uint16_t value) {
    return value > kMaxMasterVolumePermille
        ? kMaxMasterVolumePermille
        : value;
}

inline UiSessionState defaultUiSessionState() {
    return UiSessionState{};
}

inline void sanitizeUiSessionState(UiSessionState& state) {
    for (int i = 0; i < kWorkflowSessionCount; ++i) {
        const auto workflow = static_cast<SessionWorkflow>(i);
        int page = normalizeLegacyUiPage(state.lastPageByWorkflow[i]);
        if (!pageBelongsToWorkflow(page, workflow)) {
            page = defaultPageForWorkflow(workflow);
        }
        state.lastPageByWorkflow[i] = static_cast<int8_t>(page);
    }

    int activePage = normalizeLegacyUiPage(state.activePage);
    if (!validUiPage(activePage)) {
        activePage = SessionPages::kGenre;
    }
    state.activePage = static_cast<int8_t>(activePage);

    state.visualStyle = sanitizeVisualStyle(state.visualStyle);
    state.waveformOverlayEnabled = state.waveformOverlayEnabled ? 1 : 0;
    state.masterVolumePermille =
        clampMasterVolumePermille(state.masterVolumePermille);
}

inline void rememberWorkflowPage(UiSessionState& state, int page) {
    page = normalizeLegacyUiPage(page);
    if (!validUiPage(page)) return;
    const SessionWorkflow workflow = sessionWorkflowForPage(page);
    state.lastPageByWorkflow[workflowSessionIndex(workflow)] =
        static_cast<int8_t>(page);
    state.activePage = static_cast<int8_t>(page);
}

inline int rememberedWorkflowPage(const UiSessionState& state,
                                  SessionWorkflow workflow) {
    const int page = normalizeLegacyUiPage(
        state.lastPageByWorkflow[workflowSessionIndex(workflow)]);
    return pageBelongsToWorkflow(page, workflow)
        ? page
        : defaultPageForWorkflow(workflow);
}

inline int rememberedWorkflowPage(const UiSessionState& state,
                                  WorkflowMode workflow) {
    return rememberedWorkflowPage(
        state, static_cast<SessionWorkflow>(workflow));
}

inline int pageCountForWorkflow(SessionWorkflow workflow) {
    switch (workflow) {
        case SessionWorkflow::Perform: return 2;
        case SessionWorkflow::Generate: return 3;
        case SessionWorkflow::Hub: return 6;
        case SessionWorkflow::Song: return 2;
        case SessionWorkflow::Settings: return 1;
    }
    return 1;
}

inline int pageAt(SessionWorkflow workflow, int index) {
    static constexpr int kPerformPages[] = {
        SessionPages::kPerform, SessionPages::kPlayer,
    };
    static constexpr int kGeneratePages[] = {
        SessionPages::kGenre,
        SessionPages::kFeel,
        SessionPages::kGeneration,
    };
    static constexpr int kHubPages[] = {
        SessionPages::kPattern, SessionPages::kSynthA, SessionPages::kSynthB,
        SessionPages::kDrums, SessionPages::kSynthAParameters,
        SessionPages::kSynthBParameters,
    };
    static constexpr int kSongPages[] = {
        SessionPages::kArrange, SessionPages::kPhrase,
    };
    static constexpr int kSettingsPages[] = {
        SessionPages::kProject,
    };

    const int count = pageCountForWorkflow(workflow);
    while (index < 0) index += count;
    while (index >= count) index -= count;

    switch (workflow) {
        case SessionWorkflow::Perform: return kPerformPages[index];
        case SessionWorkflow::Generate: return kGeneratePages[index];
        case SessionWorkflow::Hub: return kHubPages[index];
        case SessionWorkflow::Song: return kSongPages[index];
        case SessionWorkflow::Settings: return kSettingsPages[index];
    }
    return SessionPages::kGenre;
}

inline int pageIndexInWorkflow(int page) {
    page = normalizeLegacyUiPage(page);
    const SessionWorkflow workflow = sessionWorkflowForPage(page);
    const int count = pageCountForWorkflow(workflow);
    for (int index = 0; index < count; ++index) {
        if (pageAt(workflow, index) == page) return index;
    }
    return 0;
}

inline SessionWorkflow nextWorkflow(SessionWorkflow workflow, int direction) {
    int value = static_cast<int>(workflow) + direction;
    while (value < 0) value += kWorkflowSessionCount;
    while (value >= kWorkflowSessionCount) value -= kWorkflowSessionCount;
    return static_cast<SessionWorkflow>(value);
}

inline int workflowNavigationTarget(const UiSessionState& state,
                                    int currentPage,
                                    int direction,
                                    bool workflowModifier) {
    currentPage = normalizeLegacyUiPage(currentPage);
    const SessionWorkflow workflow = sessionWorkflowForPage(currentPage);
    if (workflowModifier) {
        return rememberedWorkflowPage(
            state, nextWorkflow(workflow, direction));
    }
    return pageAt(workflow, pageIndexInWorkflow(currentPage) + direction);
}

inline int rememberedAdjacentWorkflowPage(const UiSessionState& state,
                                          int currentPage,
                                          int direction) {
    currentPage = normalizeLegacyUiPage(currentPage);
    return rememberedWorkflowPage(
        state, nextWorkflow(sessionWorkflowForPage(currentPage), direction));
}

inline uint16_t masterVolumeToPermille(float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.8f) value = 1.8f;
    return static_cast<uint16_t>(value * 1000.0f + 0.5f);
}

inline float masterVolumeFromPermille(uint16_t value) {
    return static_cast<float>(clampMasterVolumePermille(value)) / 1000.0f;
}

inline bool operator==(const UiSessionState& lhs,
                       const UiSessionState& rhs) {
    if (lhs.activePage != rhs.activePage ||
        lhs.visualStyle != rhs.visualStyle ||
        lhs.waveformOverlayEnabled != rhs.waveformOverlayEnabled ||
        lhs.masterVolumePermille != rhs.masterVolumePermille) {
        return false;
    }
    for (int i = 0; i < kWorkflowSessionCount; ++i) {
        if (lhs.lastPageByWorkflow[i] != rhs.lastPageByWorkflow[i]) return false;
    }
    return true;
}

inline bool operator!=(const UiSessionState& lhs,
                       const UiSessionState& rhs) {
    return !(lhs == rhs);
}

}  // namespace GroovePuterState
