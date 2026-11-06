#include "drum_sequencer_page.h"
#include "drum_automation_page.h"
#include "sampler_page.h"
#include "../ui_common.h"
#include "src/state/generation_request_state.h"
#include "src/state/scene_revision.h"
#include "src/state/song_edit.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"
#include "src/generation/migration/strong_rhythm_live_bridge.h"
#include "src/output/output_mode_runtime.h"
#include "src/midi/transport_clock_runtime.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

#include "../ui_input.h"
#include "../help_dialog_frames.h"
#include "../components/bank_selection_bar.h"
#include "../components/label_option.h"
#include "../components/pattern_selection_bar.h"
#include "../components/drum_sequencer_grid.h"
#include "../retro_widgets.h"
#include "../amber_widgets.h"
#include "../retro_ui_theme.h"
#include "../amber_ui_theme.h"
#include "../ui_widgets.h"
#include "../ui_clipboard.h"
#include "../../debug_log.h"
#include "../key_normalize.h"

namespace UI {
inline void drawDrumInputLockedFooter(IGfx& gfx,
                                      const char* left,
                                      const char* right) {
  const char* fixedLeft = "ARROWS:GRID Q-I:PAT";
  const char* fixedRight = "C1/2:BANK Alt[]:PAGE";
  const char* safeLeft = left;
  const char* safeRight = right;
  if (left && (std::strstr(left, "B:Bank") ||
               std::strstr(left, "1..8:Edit"))) {
    safeLeft = fixedLeft;
  }
  if (right && (std::strstr(right, "B:Bank") ||
                std::strstr(right, "1..8:Edit"))) {
    safeRight = fixedRight;
  }
  if (right && std::strstr(right, "DRUM Alt[]:PG")) safeRight = "Alt+R RHYTHMS";
  drawStandardFooter(gfx, safeLeft, safeRight);
}
}  // namespace UI

namespace {
bool g_suppressPatternLockedChildDraw = false;

bool isDrumOutputCycleKey(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN ||
      !event.alt || event.ctrl || event.meta) {
    return false;
  }
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'o' || event.scancode == GROOVEPUTER_O;
}

// Alt+E (no Ctrl/Meta) opens DRUMS STEP SETTINGS on the grid tab.
bool isDrumStepSettingsChord(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  if (!event.alt || event.ctrl || event.meta) return false;
  return event.key == 'e' || event.key == 'E' ||
         event.scancode == GROOVEPUTER_E;
}
}

class DrumSequencerMainPage;

class PatternLockedDrumContainer : public Container {
 public:
  bool handleEvent(UIEvent& ui_event) override {
    return handleEventLegacy(ui_event);
  }

  void draw(IGfx& gfx) override {
    if (g_suppressPatternLockedChildDraw) return;
    Container::draw(gfx);
  }

  virtual bool handleEventLegacy(UIEvent& ui_event) {
    return Container::handleEvent(ui_event);
  }
};

// Rename only the implementations in the retained source. Local drum pages
// keep their established handlers; the public DrumSequencerPage owns the
// main-grid input lock because it already knows which tab is active.
#define drawStandardFooter drawDrumInputLockedFooter
#define Container PatternLockedDrumContainer
#define MultiPage DrumSequencerLegacyMultiPage
#define handleEvent handleEventLegacy
#define private private: friend class DrumSequencerPage; private
#include "drum_sequencer_page_legacy.h"
#undef private
#undef handleEvent
#undef MultiPage
#undef Container
#undef drawStandardFooter

void DrumSequencerPage::stopBrowserPreview_() {
  if (auto main = getPagePtr(0)) {
    auto* page = static_cast<DrumSequencerMainPage*>(main.get());
    page->audio_guard_([&]() {
      const bool valid = browserSession_ &&
          page->mini_acid_.rhythmAuditionContextValid(browserSession_);
      if (browserSession_) (void)page->mini_acid_.endRhythmAudition(browserSession_);
      if (browserStartedTransport_ && valid && page->mini_acid_.isPlaying())
        page->mini_acid_.stop();
    });
  }
  browserSession_ = 0;
  browserStartedTransport_ = browserPending_ = browserSolo_ = false;
}

void DrumSequencerPage::closeBrowser_() {
  stopBrowserPreview_();
  browserOpen_ = false;
  browserBuffers_.reset();
}

bool DrumSequencerPage::stageBrowserCandidate_() {
  auto main = getPagePtr(0);
  if (!main || !browserBuffers_ || !browserSession_) return false;
  auto* page = static_cast<DrumSequencerMainPage*>(main.get());
  RhythmBrowser::Descriptor descriptor{};
  if (!RhythmBrowser::descriptorAt(browserIndex_, descriptor)) return false;
  const RhythmBrowser::Identity identity{
      descriptor.source, descriptor.stableId, browserVariant_, 0x52485954u,
      GroovePuterState::sceneRevisionSnapshot().currentRevision};
  if (RhythmBrowser::materialize(identity, browserBuffers_->workspace,
                                  browserBuffers_->candidate) != RhythmBrowser::Status::Ok) {
    UI::showToast("RHYTHM UNAVAILABLE", 1000);
    stopBrowserPreview_();
    return false;
  }
  bool staged = false;
  page->audio_guard_([&]() {
    staged = page->mini_acid_.stageRhythmAudition(browserSession_,
                                                  browserBuffers_->candidate.drums);
  });
  browserPending_ = !staged;
  return staged;
}

void DrumSequencerPage::selectBrowserStep_(int direction) {
  const int count = RhythmBrowser::count();
  for (int n = 0; n < count; ++n) {
    browserIndex_ = static_cast<uint8_t>((browserIndex_ + count + direction) % count);
    RhythmBrowser::Descriptor descriptor{};
    if (RhythmBrowser::descriptorAt(browserIndex_, descriptor) &&
        RhythmBrowser::matchesGroup(descriptor, static_cast<RhythmBrowser::Group>(browserGroup_))) {
      browserVariant_ = 0;
      if (browserSession_) (void)stageBrowserCandidate_();
      return;
    }
  }
}

void DrumSequencerPage::tick() {
  if (!browserOpen_ || !browserSession_) return;
  auto main = getPagePtr(0);
  if (!main) return;
  auto* page = static_cast<DrumSequencerMainPage*>(main.get());
  if (!page->mini_acid_.rhythmAuditionSessionValid(browserSession_)) {
    page->audio_guard_([&]() { (void)page->mini_acid_.endRhythmAudition(browserSession_); });
    browserSession_ = 0;
    browserStartedTransport_ = browserPending_ = false;
    UI::showToast("RHYTHM STOPPED", 900);
  } else if (browserPending_ && page->mini_acid_.rhythmAuditionCanStage(browserSession_)) {
    bool staged = false;
    page->audio_guard_([&]() {
      staged = page->mini_acid_.stageRhythmAudition(browserSession_,
                                                    browserBuffers_->candidate.drums);
    });
    browserPending_ = !staged;
  }
}

bool DrumSequencerPage::handleBrowserEvent_(UIEvent& event) {
  if (event.meta || event.alt || event.ctrl) return false;
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return true;
  if (UIInput::isBack(event) || event.scancode == GROOVEPUTER_ESCAPE) {
    closeBrowser_();
    return true;
  }
  auto main = getPagePtr(0);
  if (!main) return true;
  auto* page = static_cast<DrumSequencerMainPage*>(main.get());
  if (UIInput::isTab(event) || event.key == ' ') {
    if (browserSession_) {
      stopBrowserPreview_();
    } else {
      if (!page->mini_acid_.isPlaying() && GroovePuterMidi::externalClockOwnsTransport()) {
        UI::showToast("START EXTERNAL CLOCK", 1000);
        return true;
      }
      page->audio_guard_([&]() {
        browserStartedTransport_ = !page->mini_acid_.isPlaying();
        if (browserStartedTransport_) page->mini_acid_.start();
        browserSession_ = page->mini_acid_.beginRhythmAudition();
        if (!browserSession_ && browserStartedTransport_) page->mini_acid_.stop();
      });
      if (!browserSession_) {
        browserStartedTransport_ = false;
        UI::showToast("RHYTHM BUSY / NO TARGET", 1200);
      } else {
        (void)stageBrowserCandidate_();
      }
    }
    return true;
  }
  if (UIInput::isUp(event) || UIInput::isDown(event)) {
    selectBrowserStep_(UIInput::isUp(event) ? -1 : 1);
    return true;
  }
  if (event.key == 'g' || event.key == 'G') {
    browserGroup_ = static_cast<uint8_t>((browserGroup_ + 1) %
        static_cast<uint8_t>(RhythmBrowser::Group::Count));
    selectBrowserStep_(1);
    return true;
  }
  if (UIInput::isLeft(event) || UIInput::isRight(event)) {
    RhythmBrowser::Descriptor descriptor{};
    if (RhythmBrowser::descriptorAt(browserIndex_, descriptor) && descriptor.variantCount > 1) {
      const int delta = UIInput::isLeft(event) ? -1 : 1;
      browserVariant_ = static_cast<uint8_t>((browserVariant_ + descriptor.variantCount + delta) % descriptor.variantCount);
      if (browserSession_) (void)stageBrowserCandidate_();
    }
    return true;
  }
  if ((event.key == 'm' || event.key == 'M') && browserSession_) {
    browserSolo_ = !browserSolo_;
    page->audio_guard_([&]() { (void)page->mini_acid_.setRhythmAuditionSolo(browserSession_, browserSolo_); });
    return true;
  }
  return true;
}

void DrumSequencerPage::drawBrowser_(IGfx& gfx) {
  static const char* groups[] = {"ALL", "HOUSE", "TECHNO", "UKG", "BREAKS", "HIP-HOP", "ACID", "DUB", "ELECTRO"};
  const Rect& b = getBoundaries();
  gfx.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  gfx.setTextColor(COLOR_ACCENT);
  gfx.drawText(b.x + 3, b.y + 2, "RHYTHM BROWSER");
  gfx.setTextColor(COLOR_TEXT);
  gfx.drawText(b.x + 3, b.y + 12, groups[browserGroup_]);
  const auto neighbor = [&](int direction) {
    const int count = RhythmBrowser::count();
    int index = browserIndex_;
    for (int n = 0; n < count; ++n) {
      index = (index + count + direction) % count;
      RhythmBrowser::Descriptor item{};
      if (RhythmBrowser::descriptorAt(static_cast<uint8_t>(index), item) &&
          RhythmBrowser::matchesGroup(item, static_cast<RhythmBrowser::Group>(browserGroup_))) return index;
    }
    return static_cast<int>(browserIndex_);
  };
  RhythmBrowser::Descriptor adjacent{};
  gfx.setTextColor(COLOR_MUTED);
  if (RhythmBrowser::descriptorAt(static_cast<uint8_t>(neighbor(-1)), adjacent))
    gfx.drawText(b.x + 12, b.y + 23, adjacent.name);
  RhythmBrowser::Descriptor descriptor{};
  if (RhythmBrowser::descriptorAt(browserIndex_, descriptor)) {
    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(b.x + 3, b.y + 34, ">");
    gfx.drawText(b.x + 12, b.y + 34, descriptor.name);
    gfx.setTextColor(COLOR_MUTED);
    if (RhythmBrowser::descriptorAt(static_cast<uint8_t>(neighbor(1)), adjacent))
      gfx.drawText(b.x + 12, b.y + 45, adjacent.name);
    char info[40];
    std::snprintf(info, sizeof(info), "%s  P%u/%u",
                  descriptor.source == RhythmBrowser::Source::Atlas ? "ATLAS EXACT" : "VOCABULARY",
                  unsigned(browserVariant_ + 1), unsigned(descriptor.variantCount));
    gfx.setTextColor(COLOR_TEXT);
    gfx.drawText(b.x + 3, b.y + 56, info);
  }
  bool actualSolo = false;
  if (auto main = getPagePtr(0)) {
    auto* page = static_cast<DrumSequencerMainPage*>(main.get());
    actualSolo = page->mini_acid_.rhythmAuditionSolo(browserSession_);
  }
  const char* hear = !browserSession_ ? "BROWSING" :
      (browserSolo_ != actualSolo ? "MODE / NEXT BAR" :
       (actualSolo ? "SOLO" : "WITH SONG"));
  gfx.drawText(b.x + 3, b.y + 67, hear);
  gfx.setTextColor(COLOR_MUTED);
  gfx.drawText(b.x + 3, b.y + 78, "G GROUP  UP/DN RHYTHM");
  gfx.drawText(b.x + 3, b.y + 89, "LEFT/RIGHT VARIANT");
  gfx.drawText(b.x + 3, b.y + 100, "TAB HEAR M SOLO ESC CLOSE");
  gfx.drawText(b.x + 3, b.y + 111, "APPLY: UNAVAILABLE");
}

void DrumSequencerPage::onExit() {
  closeBrowser_();
  // STEP SETTINGS is an input mode of the grid; leaving DRUMS ends it.
  std::shared_ptr<Container> main = getPagePtr(0);
  if (main) static_cast<DrumSequencerMainPage*>(main.get())->step_settings_open_ = false;
}

void DrumSequencerPage::draw(IGfx& gfx) {
  if (browserOpen_) {
    drawBrowser_(gfx);
    return;
  }
  // The retained Minimal draw path renders grid_component_ explicitly and then
  // calls Container::draw(), which would render the same child a second time.
  // Suppress only that inherited child pass while the main grid tab is active.
  const bool previous = g_suppressPatternLockedChildDraw;
  g_suppressPatternLockedChildDraw = (activePageIndex() == 0);
  DrumSequencerLegacyMultiPage::draw(gfx);
  g_suppressPatternLockedChildDraw = previous;
}

bool DrumSequencerPage::handleEvent(UIEvent& ui_event) {
  if (browserOpen_) return handleBrowserEvent_(ui_event);
#if GROOVEPUTER_ENABLE_SAMPLER
  // Product sampler controls belong to the DRUMS workflow. Keep the retained
  // three-tab legacy constructor untouched and attach SAMPLES lazily before the
  // first Drum event; components inside SamplerPage remain lazy until displayed.
  if (pageCount() == 3) {
    std::shared_ptr<Container> main = getPagePtr(0);
    if (main) {
      auto* mainPage = static_cast<DrumSequencerMainPage*>(main.get());
      addPage(std::make_shared<SamplerPage>(
          mainPage->mini_acid_, mainPage->audio_guard_));
    }
  }

#endif
  // STEP SETTINGS is modal for keys and owns them before every other DRUMS
  // binding (Alt+O, G, Q-I, arrows). Undo/Paste are application events, not
  // keys, and still reach their handlers below.
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN && activePageIndex() == 0) {
    std::shared_ptr<Container> main = getPagePtr(0);
    auto* mainPage =
        main ? static_cast<DrumSequencerMainPage*>(main.get()) : nullptr;
    if (mainPage && mainPage->step_settings_open_) {
      return mainPage->handleStepSettingsKey(ui_event);
    }
    if (mainPage && ui_event.alt && !ui_event.ctrl && !ui_event.meta &&
        (ui_event.key == 'r' || ui_event.key == 'R' ||
         ui_event.scancode == GROOVEPUTER_R)) {
      browserBuffers_.reset(new (std::nothrow) BrowserBuffers{});
      if (!browserBuffers_) { UI::showToast("RHYTHM: NO MEMORY", 1200); return true; }
      browserOpen_ = true;
      browserIndex_ = browserVariant_ = browserGroup_ = 0;
      browserPending_ = browserSolo_ = false;
      return true;
    }
    if (mainPage && isDrumStepSettingsChord(ui_event)) {
      mainPage->openStepSettings();
      return true;
    }
  }

  // Output ownership belongs to the logical DRUMS track, not to one sub-page.
  // Alt+O therefore works from GRID/FEEL/AUTO/SAMPLES. A legacy project has no
  // fourth visible mode: the first explicit press canonicalizes it to LAYER.
  if (isDrumOutputCycleKey(ui_event)) {
    std::shared_ptr<Container> main = getPagePtr(0);
    if (!main) return true;
    auto* mainPage = static_cast<DrumSequencerMainPage*>(main.get());
    constexpr GroovePuterOutput::Track track = GroovePuterOutput::Track::Drums;
    const GroovePuterOutput::Mode next =
        GroovePuterOutput::hasExplicitMode(track)
            ? GroovePuterOutput::cycleMode(GroovePuterOutput::mode(track))
            : GroovePuterOutput::Mode::Layer;
    bool changed = false;
    mainPage->withAudioGuard([&]() {
      changed = GroovePuterOutput::applyModeWithLocalCleanup(
          mainPage->mini_acid_, track, next);
    });
    if (changed) GroovePuterState::markSceneMutated();
    char toast[40];
    std::snprintf(toast, sizeof(toast), "DRUMS OUT:%s",
                  GroovePuterOutput::modeName(next));
    UI::showToast(toast, 1200);
    return true;
  }

  // Global Drum Feel replaces the owning DrumSynthVoice when Character changes.
  // That object is read by AudioTask for every rendered sample, so the existing
  // page AudioGuard must stop the renderer at a block boundary before the legacy
  // handler performs setDrumEngine(). Other Feel rows keep their old realtime
  // path and global navigation must not acquire the guard.
  if (activePageIndex() == 1 &&
      ui_event.event_type == GROOVEPUTER_KEY_DOWN &&
      !UIInput::isGlobalNav(ui_event)) {
    std::shared_ptr<Container> feel = getPagePtr(1);
    std::shared_ptr<Container> main = getPagePtr(0);
    if (feel && main) {
      auto* feelPage = static_cast<GlobalDrumFeelPage*>(feel.get());
      auto* mainPage = static_cast<DrumSequencerMainPage*>(main.get());
      if (feelPage->selected_row_ == 0 && mainPage->audio_guard_) {
        bool handled = false;
        mainPage->audio_guard_([&]() {
          handled = handleEventLegacy(ui_event);
        });
        return handled;
      }
    }
  }

  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT &&
      ui_event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) {
    std::shared_ptr<Container> main = getPagePtr(0);
    if (!main) return false;
    auto* page = static_cast<DrumSequencerMainPage*>(main.get());
    auto& owner = GroovePuterUndo::undoOwner();
    if (!owner.hasUndo()) return false;

    if (owner.kind() == GroovePuterUndo::UndoKind::Pattern) {
      GroovePuterUndo::DrumPatternUndoPayload retained{};
      if (!owner.read(GroovePuterUndo::UndoKind::Pattern, retained)) return false;
      const bool redo = owner.nextIsRedo();
      const GroovePuterUndo::UndoResult result =
owner.togglePrepared<GroovePuterUndo::DrumPatternUndoPayload>(
    GroovePuterUndo::UndoKind::Pattern,
    [&](const GroovePuterUndo::DrumPatternUndoPayload& receipt) {
      return GroovePuterUndo::drumPatternUndoTargetAvailable(
          page->mini_acid_.sceneManager(), receipt);
    },
    [&](GroovePuterUndo::DrumPatternUndoPayload& receipt) {
      const auto exchange = [&]() {
        GroovePuterUndo::exchangeDrumPatternUndo(
            page->mini_acid_.sceneManager(), receipt);
      };
      if (page->audio_guard_) page->audio_guard_(exchange); else exchange();
    });
      if (result == GroovePuterUndo::UndoResult::Restored) {
        UI::showToast(redo ? "REDO: DRUMS" : "UNDO: DRUMS", 900);
        return true;
      }
      if (result == GroovePuterUndo::UndoResult::TargetUnavailable) {
        UI::showToast("UNDO: RETURN PAGE", 1100);
        return true;
      }
      return result == GroovePuterUndo::UndoResult::Expired;
    }
    return false;
  }

  // Only the first tab is the DrumSequencerMainPage. All other drum tabs keep
  // their previous handlers and must not inherit the pattern-grid bindings.
  const bool phraseAuditionChord =
      ui_event.ctrl && ui_event.alt && !ui_event.meta &&
      (ui_event.key == 'g' || ui_event.key == 'G' ||
       ui_event.scancode == GROOVEPUTER_G);
  if (activePageIndex() != 0 ||
      ui_event.event_type != GROOVEPUTER_KEY_DOWN ||
      (UIInput::isGlobalNav(ui_event) && !phraseAuditionChord)) {
    return handleEventLegacy(ui_event);
  }

  std::shared_ptr<Container> active = getPagePtr(0);
  if (!active) return handleEventLegacy(ui_event);
  auto* page = static_cast<DrumSequencerMainPage*>(active.get());

  // Single-cell Backspace is a manual persistent Drum edit. Intercept it before
  // the retained legacy handler so deletion uses the same bounded before-state
  // receipt as tap/Enter toggles. Selection delete and Alt+Backspace keep their
  // existing paths; this hardware follow-up is intentionally one-cell only.
  const bool singleCellBackspace = ui_event.key == '\b' || ui_event.key == 0x7F;
  if (singleCellBackspace && !ui_event.alt && !page->has_selection_ &&
      !page->patternRowFocused() && !page->bankRowFocused()) {
    const int voice = page->activeDrumVoice();
    const int step = page->activeDrumStep();
    page->commitDrumPatternMutation([&](DrumPatternSet& pattern) {
      pattern.voices[voice].steps[step].hit = false;
      pattern.voices[voice].steps[step].accent = false;
    });
    return true;
  }

  const int nav = UIInput::navCode(ui_event);
  const bool gridArrow =
      nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT ||
      nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN;
  if (gridArrow && !ui_event.alt && !ui_event.meta) {
    const bool extendSelection = ui_event.ctrl;
    if (extendSelection && page->selection_locked_) {
      page->selection_locked_ = false;
    }

    page->focusGrid();
    if (page->selection_locked_ && page->has_selection_ && !extendSelection) {
      switch (nav) {
        case GROOVEPUTER_LEFT:  page->moveSelectionFrameBy(0, -1); break;
        case GROOVEPUTER_RIGHT: page->moveSelectionFrameBy(0, 1); break;
        case GROOVEPUTER_UP:    page->moveSelectionFrameBy(-1, 0); break;
        case GROOVEPUTER_DOWN:  page->moveSelectionFrameBy(1, 0); break;
        default: break;
      }
      return true;
    }

    if (extendSelection) page->updateSelection();

    if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
      int step = page->activeDrumStep();
      step += nav == GROOVEPUTER_RIGHT ? 1 : -1;
      step %= SEQ_STEPS;
      if (step < 0) step += SEQ_STEPS;
      page->drum_step_cursor_ = step;
    } else {
      const int delta = nav == GROOVEPUTER_DOWN ? 1 : -1;
      page->drum_voice_cursor_ = std::clamp(
          page->activeDrumVoice() + delta, 0, NUM_DRUM_VOICES - 1);
    }
    return true;
  }

  char key = ui_event.key;
  if (key == 0 && ui_event.scancode >= GROOVEPUTER_F1 &&
      ui_event.scancode <= GROOVEPUTER_F8) {
    key = static_cast<char>('1' + (ui_event.scancode - GROOVEPUTER_F1));
  }
  const char lowerKey = key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(key)))
      : 0;
  const bool keyG =
      lowerKey == 'g' || ui_event.scancode == GROOVEPUTER_G;
  const bool keyL =
      lowerKey == 'l' || ui_event.scancode == GROOVEPUTER_L;
  const bool keyP =
      lowerKey == 'p' || ui_event.scancode == GROOVEPUTER_P;

  static GroovePuterRhythm::PhraseAuditionListeningCase m1ListeningCase =
      GroovePuterRhythm::PhraseAuditionListeningCase::CurrentWired;

  // Ctrl+Alt+P is a global MIDI Player shortcut, so M1L owns the local L
  // chord instead. It reaches this Drum Sequencer handler before legacy input.
  if (keyL && ui_event.ctrl && ui_event.alt && !ui_event.meta) {
    using Case = GroovePuterRhythm::PhraseAuditionListeningCase;
    m1ListeningCase = m1ListeningCase == Case::CurrentWired
        ? Case::M1SparseControl
        : m1ListeningCase == Case::M1SparseControl
        ? Case::M1SparseWired
        : m1ListeningCase == Case::M1SparseWired
            ? Case::M1CallWired
            : Case::M1SparseControl;
    const char* label = m1ListeningCase == Case::M1SparseControl
        ? "M1L C SPARSE 1,1,1,1"
        : m1ListeningCase == Case::M1SparseWired
            ? "M1L W SPARSE 1,0,1,0"
            : "M1L W CALL 2,2,2,2";
    UI::showToast(label, 1800);
    return true;
  }

  // Cardputer ADV has no dedicated Shift key in the physical workflow. Use the
  // existing Ctrl+Alt modifier pair for the explicit Stage 12 audition/probe.
  // Plain G, Ctrl+G, Alt+G and Ctrl+Alt+G stay four separate contracts.
  if (keyG && ui_event.ctrl && ui_event.alt && !ui_event.meta) {
    if (page->mini_acid_.songModeEnabled()) {
      UI::showToast("AUD: EXIT SONG", 1400);
      return true;
    }

    const int previousDrumBank = page->mini_acid_.currentDrumBankIndex();
    const int previousDrumPattern = page->mini_acid_.currentDrumPatternIndex();
    const int previousSynthBankA = page->mini_acid_.current303BankIndex(0);
    const int previousSynthBankB = page->mini_acid_.current303BankIndex(1);
    const int previousSynthPatternA = page->mini_acid_.current303PatternIndex(0);
    const int previousSynthPatternB = page->mini_acid_.current303PatternIndex(1);

    GroovePuterRhythm::PhraseAuditionResult audition{};
    page->withAudioGuard([&]() {
      audition = GroovePuterRhythm::regeneratePhraseAuditionWithProbe(
          page->mini_acid_, m1ListeningCase);

      // The bridge writes Bank B by temporarily selecting every reserved slot.
      // Rebase MiniAcid's pattern-mode return state to the exact pre-audition
      // selection before re-entering Song B.
      if (page->mini_acid_.songModeEnabled() &&
          (audition.status ==
               GroovePuterRhythm::PhraseAuditionStatus::AppliedEvolved ||
           audition.status == GroovePuterRhythm::PhraseAuditionStatus::
                                  AppliedVariationFallback)) {
        page->mini_acid_.setSongMode(false);
        page->mini_acid_.setDrumBankIndex(previousDrumBank);
        page->mini_acid_.setDrumPatternIndex(previousDrumPattern);
        page->mini_acid_.set303BankIndex(0, previousSynthBankA);
        page->mini_acid_.set303BankIndex(1, previousSynthBankB);
        page->mini_acid_.set303PatternIndex(0, previousSynthPatternA);
        page->mini_acid_.set303PatternIndex(1, previousSynthPatternB);
        page->mini_acid_.setSongMode(true);
      }
    });
    char toast[72];
    const char* listeningLabel = audition.listeningCase ==
            GroovePuterRhythm::PhraseAuditionListeningCase::M1SparseControl
        ? "M1L C SPARSE 1,1,1,1"
        : audition.listeningCase ==
                GroovePuterRhythm::PhraseAuditionListeningCase::M1SparseWired
            ? "M1L W SPARSE 1,0,1,0"
            : audition.listeningCase ==
                GroovePuterRhythm::PhraseAuditionListeningCase::M1CallWired
                ? "M1L W CALL 2,2,2,2"
                : "AUD";
    std::snprintf(
        toast,
        sizeof(toast),
        "%s %s",
        listeningLabel,
        GroovePuterRhythm::phraseAuditionStatusName(audition.status));
    UI::showToast(toast, 1800);
    return true;
  }

  // Whole-pattern plain G is a generation command, not a local edit. Preserve
  // the legacy pattern as fallback, then apply selected Stage7/14 RHYTHM + FEEL
  // to drums only. Cardputer may report G by scancode with key == 0.
  if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    SceneManager& manager = page->mini_acid_.sceneManager();
    GroovePuterUndo::DrumPatternUndoPayload before{};
    if (!GroovePuterUndo::captureCurrentDrumPatternUndo(manager, before)) {
      return true;
    }

    // Preserve the exact Stage12/strong-rhythm generator. PREPARE may touch the
    // live fixed pattern, so run it under the raw AudioGuard, capture the result,
    // then restore the before-state. Do NOT use DrumSequencerMainPage::
    // withAudioGuard() here: that legacy wrapper also calls markSceneMutated()
    // and would immediately expire the canonical receipt published below.
    DrumPatternSet after{};
    bool prepared = false;
    const auto prepareGeneration = [&]() {
      if (!GroovePuterUndo::drumPatternUndoTargetAvailable(manager, before)) {
        return;
      }

      GroovePuterRhythm::regenerateDrumsWithStrongRhythmMigration(
          page->mini_acid_);

      after = manager.currentScene().drumBanks[before.bankIndex]
                  .patterns[before.patternIndex];
      manager.currentScene().drumBanks[before.bankIndex]
          .patterns[before.patternIndex] = before.before;

      if (GroovePuterUndo::sameDrumPattern(before.before, after)) {
        return;
      }
      if (!GroovePuterUndo::drumPatternUndoTargetAvailable(manager, before)) {
        return;
      }
      prepared = true;
    };
    if (page->audio_guard_) page->audio_guard_(prepareGeneration);
    else prepareGeneration();
    if (!prepared) return true;

    GroovePuterUndo::undoOwner().commitPrepared(
        GroovePuterUndo::UndoKind::Pattern, before, [&]() {
          const auto apply = [&]() {
            manager.currentScene().drumBanks[before.bankIndex]
                .patterns[before.patternIndex] = after;
          };
          if (page->audio_guard_) page->audio_guard_(apply);
          else apply();
        });
    return true;
  }

  // P cycles the shared internal realization selector. The runtime keeps its
  // stable P1/P2/P3 contract, while the musician-facing projection is STYLE.
  // O remains blocked from the old sketch-level Synth B generator; I remains a
  // valid Q-I pattern-slot key.
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta && keyP) {
    const auto level = GroovePuterState::cycleGenerationLevel();
    UI::showToast(GroovePuterState::generationStyleName(level), 1200);
    return true;
  }
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lowerKey == 'o') {
    UI::showToast("LEGACY O GEN OFF", 1200);
    return true;
  }

  // Q-I changes the active Drum slot as runtime state. Optional chaining is a
  // separate persistent Song mutation owned by the canonical UndoOwner.
  if (!ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
    int patternIdx = page->patternIndexFromKey(lowerKey);
    if (patternIdx < 0) {
      patternIdx = scancodeToPatternIndex(ui_event.scancode);
    }
    if (patternIdx >= 0) {
      if (page->mini_acid_.songModeEnabled()) return true;
      page->setDrumPatternCursor(patternIdx);
      page->focusGrid();
      const auto selectPattern = [&]() {
        page->mini_acid_.setDrumPatternIndex(patternIdx);
      };
      if (page->audio_guard_) page->audio_guard_(selectPattern);
      else selectPattern();

      if (page->chaining_mode_) {
        SceneManager& manager = page->mini_acid_.sceneManager();
        GroovePuterUndo::SongUndoPayload before{};
        if (GroovePuterUndo::captureCurrentSongUndo(manager, before)) {
          Song after = before.before;
          for (int row = 0; row < Song::kMaxPositions; ++row) {
            if (GroovePuterUndo::SongEdit::patternAt(
                    after, row, SongTrack::Drums) == -1) {
              GroovePuterUndo::SongEdit::setPattern(
                  after, row, SongTrack::Drums, patternIdx);
              break;
            }
          }
          if (!GroovePuterUndo::sameSong(before.before, after) &&
              GroovePuterUndo::songUndoTargetAvailable(manager, before)) {
            GroovePuterUndo::undoOwner().commitPrepared(
                GroovePuterUndo::UndoKind::Song, before, [&]() {
                  const auto apply = [&]() {
                    manager.currentScene().songs[before.songSlot] = after;
                  };
                  if (page->audio_guard_) page->audio_guard_(apply);
                  else apply();
                });
          }
        }
      }
      return true;
    }
  }

  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta &&
      (key == '1' || key == '2')) {
    const int bankIdx = page->bankIndexFromKey(key);
    page->bank_cursor_ = bankIdx;
    page->setBankIndex(bankIdx);
    page->focusGrid();
    UI::showToast(bankIdx == 0 ? "Bank A (Ctrl+1)" : "Bank B (Ctrl+2)", 800);
    return true;
  }

  return page->handleEventLegacy(ui_event);
}
