#pragma once

#include <cstddef>
#include <cstdint>

#include "src/ui/ui_config.h"
#include "src/ui/ui_core.h"
#include "src/ui/workflow_mode.h"

namespace GroovePuterState {

constexpr int kWorkflowSessionCount = 5;
constexpr uint16_t kDefaultMasterVolumePermille = 600;
constexpr uint16_t kMaxMasterVolumePermille = 1800;

struct UiSessionState {
    int8_t activePage{static_cast<int8_t>(WorkflowPages::kGenre)};
    int8_t lastPageByWorkflow[kWorkflowSessionCount]{
        static_cast<int8_t>(WorkflowPages::kPerform),
        static_cast<int8_t>(WorkflowPages::kGenre),
        static_cast<int8_t>(WorkflowPages::kPattern),
        static_cast<int8_t>(WorkflowPages::kArrange),
        static_cast<int8_t>(WorkflowPages::kProject),
    };
    uint8_t visualStyle{static_cast<uint8_t>(VisualStyle::RETRO_CLASSIC)};
    uint8_t waveformOverlayEnabled{1};
    uint16_t masterVolumePermille{kDefaultMasterVolumePermille};
};

static_assert(sizeof(UiSessionState) <= 12,
              "UI session state must remain a tiny fixed-size payload");

inline int workflowSessionIndex(WorkflowMode mode) {
    const int value = static_cast<int>(mode);
    return value >= 0 && value < kWorkflowSessionCount ? value : 0;
}

inline int defaultPageForWorkflow(WorkflowMode mode) {
    return WorkflowPages::pageForMode(mode);
}

inline bool validUiPage(int page) {
    return page >= 0 && page < UI::kPageCount;
}

inline bool pageBelongsToWorkflow(int page, WorkflowMode mode) {
    return validUiPage(page) && WorkflowPages::modeForPage(page) == mode;
}

inline VisualStyle sanitizeVisualStyle(uint8_t value) {
    const auto style = static_cast<VisualStyle>(value);
    switch (style) {
        case VisualStyle::MINIMAL:
        case VisualStyle::RETRO_CLASSIC:
        case VisualStyle::AMBER:
            return style;
        case VisualStyle::MINIMAL_DARK:
        default:
            return VisualStyle::MINIMAL;
    }
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
        const auto mode = static_cast<WorkflowMode>(i);
        const int page = state.lastPageByWorkflow[i];
        if (!pageBelongsToWorkflow(page, mode)) {
            state.lastPageByWorkflow[i] =
                static_cast<int8_t>(defaultPageForWorkflow(mode));
        }
    }

    if (!validUiPage(state.activePage)) {
        state.activePage = static_cast<int8_t>(WorkflowPages::kGenre);
    }
    const WorkflowMode activeMode = WorkflowPages::modeForPage(state.activePage);
    if (!pageBelongsToWorkflow(state.activePage, activeMode)) {
        state.activePage = static_cast<int8_t>(defaultPageForWorkflow(activeMode));
    }

    state.visualStyle = static_cast<uint8_t>(sanitizeVisualStyle(state.visualStyle));
    state.waveformOverlayEnabled = state.waveformOverlayEnabled ? 1 : 0;
    state.masterVolumePermille =
        clampMasterVolumePermille(state.masterVolumePermille);
}

inline void rememberWorkflowPage(UiSessionState& state, int page) {
    if (!validUiPage(page)) return;
    const WorkflowMode mode = WorkflowPages::modeForPage(page);
    state.lastPageByWorkflow[workflowSessionIndex(mode)] =
        static_cast<int8_t>(page);
    state.activePage = static_cast<int8_t>(page);
}

inline int rememberedWorkflowPage(const UiSessionState& state,
                                  WorkflowMode mode) {
    const int page = state.lastPageByWorkflow[workflowSessionIndex(mode)];
    return pageBelongsToWorkflow(page, mode)
        ? page
        : defaultPageForWorkflow(mode);
}

inline int workflowNavigationTarget(const UiSessionState& state,
                                    int currentPage,
                                    int direction,
                                    bool workflowModifier) {
    const WorkflowMode mode = WorkflowPages::modeForPage(currentPage);
    if (workflowModifier) {
        return rememberedWorkflowPage(
            state, WorkflowPages::nextMode(mode, direction));
    }

    const int count = WorkflowPages::pageCountForMode(mode);
    int index = WorkflowPages::pageIndexInMode(currentPage) + direction;
    while (index < 0) index += count;
    while (index >= count) index -= count;
    return WorkflowPages::pageAt(mode, index);
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
