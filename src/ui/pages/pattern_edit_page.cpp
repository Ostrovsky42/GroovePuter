#include "pattern_edit_page.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "../ui_common.h"
#include "../retro_ui_theme.h"
#include "../retro_widgets.h"
#include "../amber_ui_theme.h"
#include "../amber_widgets.h"
#include "../ui_clipboard.h"
#include "../ui_input.h"
#include "../layout_manager.h"
#include "../help_dialog_frames.h"
#include "../components/bank_selection_bar.h"
#include "../components/pattern_selection_bar.h"
#include "../../debug_log.h"
#include "../key_normalize.h"

namespace UI {
inline void drawPatternInputLockedFooter(IGfx& gfx,
                                         const char* left,
                                         const char* right) {
  const bool staleBinding =
      (left && std::strstr(left, "B:Bank")) ||
      (right && std::strstr(right, "B:Bank"));
  if (staleBinding) {
    drawStandardFooter(gfx, "ARROWS:GRID Q-I:PAT", "C1/2:BANK TAB:SUB");
    return;
  }
  drawStandardFooter(gfx, left, right);
}
}  // namespace UI

// Keep the established editor behavior intact under a private legacy entry
// point. The public handler below owns only navigation and explicit selectors.
#define drawStandardFooter drawPatternInputLockedFooter
#define handleEvent handleEventLegacy
#include "pattern_edit_page_legacy.inc"
#undef handleEvent
#undef drawStandardFooter

bool PatternEditPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) {
    return handleEventLegacy(ui_event);
  }

  // Global navigation, pattern rotation/FX editing and meta note editing keep
  // their existing behavior. Only unmodified/selection arrows are grid-owned.
  if (UIInput::isGlobalNav(ui_event)) {
    return handleEventLegacy(ui_event);
  }

  const int nav = UIInput::navCode(ui_event);
  const bool gridArrow =
      nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT ||
      nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN;
  if (gridArrow && !ui_event.alt && !ui_event.meta) {
    const bool extendSelection = ui_event.shift || ui_event.ctrl;
    if (extendSelection && selection_locked_) selection_locked_ = false;

    focus_ = Focus::Steps;
    if (selection_locked_ && has_selection_ && !extendSelection) {
      switch (nav) {
        case GROOVEPUTER_LEFT:  moveSelectionFrameBy(0, -1); break;
        case GROOVEPUTER_RIGHT: moveSelectionFrameBy(0, 1); break;
        case GROOVEPUTER_UP:    moveSelectionFrameBy(-1, 0); break;
        case GROOVEPUTER_DOWN:  moveSelectionFrameBy(1, 0); break;
        default: break;
      }
      return true;
    }

    if (extendSelection) updateSelection();

    const int step = activePatternStep();
    int row = step / kPatternStepColumns;
    int col = step % kPatternStepColumns;
    switch (nav) {
      case GROOVEPUTER_LEFT:
        col = (col + kPatternStepColumns - 1) % kPatternStepColumns;
        break;
      case GROOVEPUTER_RIGHT:
        col = (col + 1) % kPatternStepColumns;
        break;
      case GROOVEPUTER_UP:
        row = std::max(0, row - 1);
        break;
      case GROOVEPUTER_DOWN:
        row = std::min(kPatternStepRows - 1, row + 1);
        break;
      default:
        break;
    }
    pattern_edit_cursor_ = row * kPatternStepColumns + col;
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

  // Q-I is the only keyboard path for slots 1-8. Selection is immediate and
  // the next arrow continues moving inside the note grid.
  if (!ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
    int patternIdx = patternIndexFromKey(lowerKey);
    if (patternIdx < 0) {
      patternIdx = scancodeToPatternIndex(ui_event.scancode);
    }
    if (patternIdx >= 0) {
      if (mini_acid_.songModeEnabled()) return true;
      setPatternCursor(patternIdx);
      withAudioGuard([&]() {
        mini_acid_.set303PatternIndex(voice_index_, patternIdx);
        if (chaining_mode_) {
          const SongTrack track = voice_index_ == 0
              ? SongTrack::SynthA
              : SongTrack::SynthB;
          for (int i = 0; i < Song::kMaxPositions; ++i) {
            if (mini_acid_.songPatternAt(i, track) == -1) {
              mini_acid_.setSongPattern(i, track, patternIdx);
              break;
            }
          }
        }
      });
      focus_ = Focus::Steps;
      return true;
    }
  }

  // Bank selection has one unambiguous binding. Plain numbers remain global
  // track mutes; plain B no longer silently changes the current bank.
  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta &&
      (key == '1' || key == '2')) {
    const int bankIdx = bankIndexFromKey(key);
    bank_cursor_ = bankIdx;
    setBankIndex(bankIdx);
    focus_ = Focus::Steps;
    UI::showToast(bankIdx == 0 ? "Bank A (Ctrl+1)" : "Bank B (Ctrl+2)", 800);
    return true;
  }

  if (!ui_event.shift && !ui_event.ctrl && !ui_event.alt && !ui_event.meta &&
      (lowerKey == 'b' || ui_event.scancode == GROOVEPUTER_B)) {
    UI::showToast("Bank: Ctrl+1 / Ctrl+2", 800);
    return true;
  }

  return handleEventLegacy(ui_event);
}
