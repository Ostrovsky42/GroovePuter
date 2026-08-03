#include <cassert>

#include "src/state/ui_session_state.h"

using namespace GroovePuterState;

int main() {
    UiSessionState state = defaultUiSessionState();
    sanitizeUiSessionState(state);

    assert(rememberedWorkflowPage(state, SessionWorkflow::Perform) ==
           SessionPages::kPerform);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Generate) ==
           SessionPages::kGenre);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Hub) ==
           SessionPages::kPattern);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Song) ==
           SessionPages::kArrange);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Settings) ==
           SessionPages::kProject);

    rememberWorkflowPage(state, SessionPages::kPlayer);
    rememberWorkflowPage(state, SessionPages::kFeelTexture);
    rememberWorkflowPage(state, SessionPages::kSynthBParameters);
    rememberWorkflowPage(state, SessionPages::kGenerator);

    assert(rememberedWorkflowPage(state, SessionWorkflow::Perform) ==
           SessionPages::kPlayer);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Generate) ==
           SessionPages::kFeelTexture);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Hub) ==
           SessionPages::kSynthBParameters);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Settings) ==
           SessionPages::kGenerator);

    const int fromSettingsToPerform = workflowNavigationTarget(
        state, SessionPages::kGenerator, 1, true);
    assert(fromSettingsToPerform == SessionPages::kPlayer);

    const int fromPerformToGenerate = workflowNavigationTarget(
        state, SessionPages::kPlayer, 1, true);
    assert(fromPerformToGenerate == SessionPages::kFeelTexture);

    const int localPerformWrap = workflowNavigationTarget(
        state, SessionPages::kPlayer, 1, false);
    assert(localPerformWrap == SessionPages::kPerform);

    state.activePage = 99;
    state.lastPageByWorkflow[workflowSessionIndex(SessionWorkflow::Hub)] =
        static_cast<int8_t>(SessionPages::kPlayer);
    state.visualStyle = 1;
    state.waveformOverlayEnabled = 7;
    state.masterVolumePermille = 60000;
    sanitizeUiSessionState(state);

    assert(state.activePage == SessionPages::kGenre);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Hub) ==
           SessionPages::kPattern);
    assert(state.visualStyle == 0);
    assert(state.waveformOverlayEnabled == 1);
    assert(state.masterVolumePermille == kMaxMasterVolumePermille);

    assert(masterVolumeToPermille(0.6f) == 600);
    assert(masterVolumeToPermille(2.5f) == 1800);
    assert(masterVolumeFromPermille(600) > 0.599f &&
           masterVolumeFromPermille(600) < 0.601f);

    UiSessionState copy = state;
    assert(copy == state);
    copy.activePage = SessionPages::kPerform;
    assert(copy != state);

    return 0;
}
