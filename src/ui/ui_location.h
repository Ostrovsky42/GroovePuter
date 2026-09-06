#pragma once

#include <cstdint>

#include "workflow_mode.h"

namespace UI {

enum class UiTarget : uint8_t {
    Performance = 0,
    MidiPlayer,
    Generation,
    Overview,
    SynthA,
    SynthB,
    Drums,
    Song,
    Phrase,
    PhraseCore,
    Project,
};

enum class UiSurface : uint8_t {
    Performance = 0,
    Player,
    Genre,
    Feel,
    Overview,
    Local,
    Drums,
    Arrange,
    Phrase,
    PhraseCore,
    Project,
};

struct UiLocation {
    WorkflowMode workflow{WorkflowMode::Generate};
    UiTarget target{UiTarget::Generation};
    UiSurface surface{UiSurface::Genre};
};

// Projects an already-canonical runtime page identity into semantic UI
// coordinates. Legacy persisted aliases are intentionally NOT normalized here:
// migration belongs at the persistence/navigation boundary, not inside the new
// runtime semantic model.
inline bool tryUiLocationForPage(int canonicalPage, UiLocation& out) {
    switch (canonicalPage) {
        case WorkflowPages::kGenre:
            out = {WorkflowMode::Generate, UiTarget::Generation, UiSurface::Genre};
            return true;
        case WorkflowPages::kFeel:
            out = {WorkflowMode::Generate, UiTarget::Generation, UiSurface::Feel};
            return true;
        case WorkflowPages::kPerform:
            out = {WorkflowMode::Perform, UiTarget::Performance, UiSurface::Performance};
            return true;
        case WorkflowPages::kPlayer:
            out = {WorkflowMode::Perform, UiTarget::MidiPlayer, UiSurface::Player};
            return true;
        case WorkflowPages::kPattern:
            out = {WorkflowMode::Hub, UiTarget::Overview, UiSurface::Overview};
            return true;
        case WorkflowPages::kSynthA:
            out = {WorkflowMode::Hub, UiTarget::SynthA, UiSurface::Local};
            return true;
        case WorkflowPages::kSynthB:
            out = {WorkflowMode::Hub, UiTarget::SynthB, UiSurface::Local};
            return true;
        case WorkflowPages::kDrums:
            out = {WorkflowMode::Hub, UiTarget::Drums, UiSurface::Drums};
            return true;
        case WorkflowPages::kArrange:
            out = {WorkflowMode::Song, UiTarget::Song, UiSurface::Arrange};
            return true;
        case WorkflowPages::kPhrase:
            out = {WorkflowMode::Song, UiTarget::Phrase, UiSurface::Phrase};
            return true;
        case WorkflowPages::kPhraseCore:
            out = {WorkflowMode::Song, UiTarget::PhraseCore, UiSurface::PhraseCore};
            return true;
        case WorkflowPages::kProject:
            out = {WorkflowMode::Settings, UiTarget::Project, UiSurface::Project};
            return true;
        default:
            return false;
    }
}

}  // namespace UI
