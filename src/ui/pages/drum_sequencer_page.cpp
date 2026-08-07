#include "drum_sequencer_page.h"
#include "drum_automation_page.h"
#include "../ui_common.h"
#include "src/state/scene_revision.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
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
  drawStandardFooter(gfx, safeLeft, safeRight);
}
}  // namespace UI

class DrumSequencerMainPage;

class PatternLockedDrumContainer : public Container {
 public:
  bool handleEvent(UIEvent& ui_event) override {
    return handleEventLegacy(ui_event);
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

bool DrumSequencerPage::handleEvent(UIEvent& ui_event) {
  // Only the first tab is the DrumSequencerMainPage. All other drum tabs keep
  // their previous handlers and must not inherit the pattern-grid bindings.
  if (activePageIndex() != 0 ||
      ui_event.event_type != GROOVEPUTER_KEY_DOWN ||
      UIInput::isGlobalNav(ui_event)) {
    return handleEventLegacy(ui_event);
  }

  std::shared_ptr<Container> active = getPagePtr(0);
  if (!active) return handleEventLegacy(ui_event);
  auto* page = static_cast<DrumSequencerMainPage*>(active.get());

  const int nav = UIInput::navCode(ui_event);
  const bool gridArrow =
      nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT ||
      nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN;
  if (gridArrow && !ui_event.alt && !ui_event.meta) {
    const bool extendSelection = ui_event.shift || ui_event.ctrl;
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

  // Q-I changes the active slot but never hands keyboard focus to the selector.
  if (!ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
    int patternIdx = page->patternIndexFromKey(lowerKey);
    if (patternIdx < 0) {
      patternIdx = scancodeToPatternIndex(ui_event.scancode);
    }
    if (patternIdx >= 0) {
      if (page->mini_acid_.songModeEnabled()) return true;
      page->setDrumPatternCursor(patternIdx);
      page->focusGrid();
      page->withAudioGuard([&]() {
        page->mini_acid_.setDrumPatternIndex(patternIdx);
        if (page->chaining_mode_) {
          for (int i = 0; i < Song::kMaxPositions; ++i) {
            if (page->mini_acid_.songPatternAt(i, SongTrack::Drums) == -1) {
              page->mini_acid_.setSongPattern(i, SongTrack::Drums, patternIdx);
              break;
            }
          }
        }
      });
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
