#include <cassert>

#include "../src/ui/ui_location.h"
#include "../src/ui/ui_status_chrome.h"

int main() {
  UI::UiLocation location{};

  assert(UI::tryUiLocationForPage(WorkflowPages::kGenre, location));
  assert(location.workflow == WorkflowMode::Generate);
  assert(location.target == UI::UiTarget::Generation);
  assert(location.surface == UI::UiSurface::Genre);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Genre);

  assert(UI::tryUiLocationForPage(WorkflowPages::kFeel, location));
  assert(location.workflow == WorkflowMode::Generate);
  assert(location.target == UI::UiTarget::Generation);
  assert(location.surface == UI::UiSurface::Feel);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Feel);

  assert(UI::tryUiLocationForPage(WorkflowPages::kPerform, location));
  assert(location.workflow == WorkflowMode::Perform);
  assert(location.target == UI::UiTarget::Performance);
  assert(location.surface == UI::UiSurface::Performance);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Perform);

  assert(UI::tryUiLocationForPage(WorkflowPages::kPlayer, location));
  assert(location.workflow == WorkflowMode::Perform);
  assert(location.target == UI::UiTarget::MidiPlayer);
  assert(location.surface == UI::UiSurface::Player);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Player);

  assert(UI::tryUiLocationForPage(WorkflowPages::kPattern, location));
  assert(location.workflow == WorkflowMode::Hub);
  assert(location.target == UI::UiTarget::Overview);
  assert(location.surface == UI::UiSurface::Overview);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Overview);

  assert(UI::tryUiLocationForPage(WorkflowPages::kSynthA, location));
  assert(location.workflow == WorkflowMode::Hub);
  assert(location.target == UI::UiTarget::SynthA);
  assert(location.surface == UI::UiSurface::Local);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::SynthA);

  assert(UI::tryUiLocationForPage(WorkflowPages::kSynthB, location));
  assert(location.workflow == WorkflowMode::Hub);
  assert(location.target == UI::UiTarget::SynthB);
  assert(location.surface == UI::UiSurface::Local);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::SynthB);

  assert(UI::tryUiLocationForPage(WorkflowPages::kDrums, location));
  assert(location.workflow == WorkflowMode::Hub);
  assert(location.target == UI::UiTarget::Drums);
  assert(location.surface == UI::UiSurface::Drums);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Drums);

  assert(UI::tryUiLocationForPage(WorkflowPages::kArrange, location));
  assert(location.workflow == WorkflowMode::Song);
  assert(location.target == UI::UiTarget::Song);
  assert(location.surface == UI::UiSurface::Arrange);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Song);

  assert(UI::tryUiLocationForPage(WorkflowPages::kPhrase, location));
  assert(location.workflow == WorkflowMode::Song);
  assert(location.target == UI::UiTarget::Phrase);
  assert(location.surface == UI::UiSurface::Phrase);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Phrase);

  assert(UI::tryUiLocationForPage(WorkflowPages::kPhraseCore, location));
  assert(location.workflow == WorkflowMode::Song);
  assert(location.target == UI::UiTarget::PhraseCore);
  assert(location.surface == UI::UiSurface::PhraseCore);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::PhraseCore);

  assert(UI::tryUiLocationForPage(WorkflowPages::kProject, location));
  assert(location.workflow == WorkflowMode::Settings);
  assert(location.target == UI::UiTarget::Project);
  assert(location.surface == UI::UiSurface::Project);
  assert(UI::uiStatusContextForLocation(location) == UI::UiStatusContext::Project);

  // Legacy aliases are not runtime semantic identities in the new model.
  assert(!UI::tryUiLocationForPage(WorkflowPages::kGeneration, location));
  assert(!UI::tryUiLocationForPage(WorkflowPages::kTexture, location));
  assert(!UI::tryUiLocationForPage(WorkflowPages::kSynthAParameters, location));
  assert(!UI::tryUiLocationForPage(WorkflowPages::kSynthBParameters, location));
  assert(!UI::tryUiLocationForPage(WorkflowPages::kSampler, location));
  assert(!UI::tryUiLocationForPage(999, location));

  return 0;
}
