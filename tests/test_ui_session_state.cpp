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

    assert(pageCountForWorkflow(SessionWorkflow::Generate) == 3);
    assert(pageAt(SessionWorkflow::Generate, 0) == SessionPages::kGenre);
    assert(pageAt(SessionWorkflow::Generate, 1) == SessionPages::kFeel);
    assert(pageAt(SessionWorkflow::Generate, 2) == SessionPages::kGeneration);
    assert(pageAt(SessionWorkflow::Generate, 3) == SessionPages::kGenre);
    assert(pageCountForWorkflow(SessionWorkflow::Song) == 2);
    assert(pageAt(SessionWorkflow::Song, 0) == SessionPages::kArrange);
    assert(pageAt(SessionWorkflow::Song, 1) == SessionPages::kPhrase);
    assert(pageCountForWorkflow(SessionWorkflow::Settings) == 1);
    assert(pageAt(SessionWorkflow::Settings, 0) == SessionPages::kProject);

    rememberWorkflowPage(state, SessionPages::kPlayer);
    rememberWorkflowPage(state, SessionPages::kFeel);
    rememberWorkflowPage(state, SessionPages::kSynthBParameters);
    rememberWorkflowPage(state, SessionPages::kPhrase);
    rememberWorkflowPage(state, SessionPages::kProject);

    assert(rememberedWorkflowPage(state, SessionWorkflow::Perform) ==
           SessionPages::kPlayer);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Generate) ==
           SessionPages::kFeel);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Hub) ==
           SessionPages::kSynthBParameters);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Song) ==
           SessionPages::kPhrase);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Settings) ==
           SessionPages::kProject);

    const int fromSettingsToPerform = workflowNavigationTarget(
        state, SessionPages::kProject, 1, true);
    assert(fromSettingsToPerform == SessionPages::kPlayer);

    const int fromPerformToGenerate = workflowNavigationTarget(
        state, SessionPages::kPlayer, 1, true);
    assert(fromPerformToGenerate == SessionPages::kFeel);

    const int genreToFeel = workflowNavigationTarget(
        state, SessionPages::kGenre, 1, false);
    assert(genreToFeel == SessionPages::kFeel);
    const int feelToGeneration = workflowNavigationTarget(
        state, SessionPages::kFeel, 1, false);
    assert(feelToGeneration == SessionPages::kGeneration);
    const int generationToGenre = workflowNavigationTarget(
        state, SessionPages::kGeneration, 1, false);
    assert(generationToGenre == SessionPages::kGenre);

    // Historical page id 8 is accepted only as a compatibility alias. It
    // resolves to FEEL before remembering, restoring or navigating.
    assert(normalizeLegacyUiPage(SessionPages::kTexture) == SessionPages::kFeel);
    rememberWorkflowPage(state, SessionPages::kTexture);
    assert(state.activePage == SessionPages::kFeel);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Generate) ==
           SessionPages::kFeel);
    const int textureToGeneration = workflowNavigationTarget(
        state, SessionPages::kTexture, 1, false);
    assert(textureToGeneration == SessionPages::kGeneration);

    const int phraseToSong = workflowNavigationTarget(
        state, SessionPages::kPhrase, 1, false);
    assert(phraseToSong == SessionPages::kArrange);
    const int songToPhrase = workflowNavigationTarget(
        state, SessionPages::kArrange, 1, false);
    assert(songToPhrase == SessionPages::kPhrase);

    state.activePage = SessionPages::kTexture;
    state.lastPageByWorkflow[workflowSessionIndex(SessionWorkflow::Generate)] =
        static_cast<int8_t>(SessionPages::kTexture);
    sanitizeUiSessionState(state);
    assert(state.activePage == SessionPages::kFeel);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Generate) ==
           SessionPages::kFeel);

    state.activePage = 99;
    state.lastPageByWorkflow[workflowSessionIndex(SessionWorkflow::Hub)] =
        static_cast<int8_t>(SessionPages::kPlayer);
    state.lastPageByWorkflow[workflowSessionIndex(SessionWorkflow::Settings)] =
        static_cast<int8_t>(SessionPages::kFeel);
    state.visualStyle = 1;
    state.waveformOverlayEnabled = 7;
    state.masterVolumePermille = 60000;
    sanitizeUiSessionState(state);

    assert(state.activePage == SessionPages::kGenre);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Hub) ==
           SessionPages::kPattern);
    assert(rememberedWorkflowPage(state, SessionWorkflow::Settings) ==
           SessionPages::kProject);
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
