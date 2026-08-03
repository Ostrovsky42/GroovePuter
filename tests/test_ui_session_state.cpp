#include <cassert>

#include "src/state/ui_session_state.h"

using namespace GroovePuterState;

int main() {
    UiSessionState state = defaultUiSessionState();
    sanitizeUiSessionState(state);

    assert(rememberedWorkflowPage(state, WorkflowMode::Perform) ==
           WorkflowPages::kPerform);
    assert(rememberedWorkflowPage(state, WorkflowMode::Generate) ==
           WorkflowPages::kGenre);
    assert(rememberedWorkflowPage(state, WorkflowMode::Hub) ==
           WorkflowPages::kPattern);
    assert(rememberedWorkflowPage(state, WorkflowMode::Song) ==
           WorkflowPages::kArrange);
    assert(rememberedWorkflowPage(state, WorkflowMode::Settings) ==
           WorkflowPages::kProject);

    rememberWorkflowPage(state, WorkflowPages::kPlayer);
    rememberWorkflowPage(state, WorkflowPages::kFeelTexture);
    rememberWorkflowPage(state, WorkflowPages::kSynthBParameters);
    rememberWorkflowPage(state, WorkflowPages::kGenerator);

    assert(rememberedWorkflowPage(state, WorkflowMode::Perform) ==
           WorkflowPages::kPlayer);
    assert(rememberedWorkflowPage(state, WorkflowMode::Generate) ==
           WorkflowPages::kFeelTexture);
    assert(rememberedWorkflowPage(state, WorkflowMode::Hub) ==
           WorkflowPages::kSynthBParameters);
    assert(rememberedWorkflowPage(state, WorkflowMode::Settings) ==
           WorkflowPages::kGenerator);

    const int fromSettingsToPerform = workflowNavigationTarget(
        state, WorkflowPages::kGenerator, 1, true);
    assert(fromSettingsToPerform == WorkflowPages::kPlayer);

    const int fromPerformToGenerate = workflowNavigationTarget(
        state, WorkflowPages::kPlayer, 1, true);
    assert(fromPerformToGenerate == WorkflowPages::kFeelTexture);

    const int localPerformWrap = workflowNavigationTarget(
        state, WorkflowPages::kPlayer, 1, false);
    assert(localPerformWrap == WorkflowPages::kPerform);

    state.activePage = 99;
    state.lastPageByWorkflow[workflowSessionIndex(WorkflowMode::Hub)] =
        static_cast<int8_t>(WorkflowPages::kPlayer);
    state.visualStyle = static_cast<uint8_t>(VisualStyle::MINIMAL_DARK);
    state.waveformOverlayEnabled = 7;
    state.masterVolumePermille = 60000;
    sanitizeUiSessionState(state);

    assert(state.activePage == WorkflowPages::kGenre);
    assert(rememberedWorkflowPage(state, WorkflowMode::Hub) ==
           WorkflowPages::kPattern);
    assert(static_cast<VisualStyle>(state.visualStyle) == VisualStyle::MINIMAL);
    assert(state.waveformOverlayEnabled == 1);
    assert(state.masterVolumePermille == kMaxMasterVolumePermille);

    assert(masterVolumeToPermille(0.6f) == 600);
    assert(masterVolumeToPermille(2.5f) == 1800);
    assert(masterVolumeFromPermille(600) > 0.599f &&
           masterVolumeFromPermille(600) < 0.601f);

    UiSessionState copy = state;
    assert(copy == state);
    copy.activePage = WorkflowPages::kPerform;
    assert(copy != state);

    return 0;
}
