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
    drawStandardFooter(gfx, "ARROWS:GRID Q-I:PAT", "C1/2:BANK Alt[]:PAGE");
    return;
  }
  drawStandardFooter(gfx, left, right);
}
}  // namespace UI

namespace {
constexpr unsigned long kFirstHoldRepeatMinMs = 250;
constexpr unsigned long kFirstHoldRepeatMaxMs = 500;
constexpr unsigned long kHoldRepeatMaxGapMs = 180;
constexpr int kEntryLowerBaseNote = 48;  // C3
constexpr int kEntryUpperBaseNote = 60;  // C4

int indexInKeyRow(char key, const char* row) {
  if (!row) return -1;
  const char* found = std::strchr(row, key);
  return found ? static_cast<int>(found - row) : -1;
}
}  // namespace

// Keep the established editor behavior intact under a private legacy entry
// point. The public handler below owns only navigation and explicit selectors.
#define drawStandardFooter drawPatternInputLockedFooter
#define handleEvent handleEventLegacy
#include "pattern_edit_page_legacy.h"
#undef handleEvent
#undef drawStandardFooter

int PatternEditPage::noteForEntryKey(char key) const {
  const char lower = static_cast<char>(
      std::tolower(static_cast<unsigned char>(key)));
  const int lowerIndex = indexInKeyRow(lower, "asdfghjkl");
  if (lowerIndex >= 0) return kEntryLowerBaseNote + lowerIndex;
  const int upperIndex = indexInKeyRow(lower, "qwertyuiop");
  if (upperIndex >= 0) return kEntryUpperBaseNote + upperIndex;
  return -1;
}

void PatternEditPage::resetNoteHoldTracking() {
  last_note_key_ = 0;
  last_entered_step_ = -1;
  last_note_key_ms_ = 0;
  note_hold_active_ = false;
}

void PatternEditPage::advanceNoteEntryCursor() {
  focus_ = Focus::Steps;
  pattern_edit_cursor_ = (activePatternStep() + 1) % SEQ_STEPS;
}

void PatternEditPage::writeNoteEntryStep(int step, int note, bool continuation) {
  if (step < 0 || step >= SEQ_STEPS || note < 0 || note > 127) return;
  withAudioGuard([&]() {
    const int vIdx = voice_index_ < 0 ? 0 : (voice_index_ >= 2 ? 1 : voice_index_);
    SynthPattern& pattern = mini_acid_.sceneManager().editCurrentSynthPattern(vIdx);
    if (continuation && last_entered_step_ >= 0 &&
        last_entered_step_ < SEQ_STEPS && last_entered_step_ != step) {
      pattern.steps[last_entered_step_].slide = true;
    }
    pattern.steps[step].note = static_cast<int8_t>(note);
    if (continuation) pattern.steps[step].slide = false;
  });
}

bool PatternEditPage::handleNoteEntryKey(char key) {
  const int note = noteForEntryKey(key);
  if (note < 0) return false;

  focus_ = Focus::Steps;
  if (has_selection_) clearSelection();

  const unsigned long now = millis();
  const char lower = static_cast<char>(
      std::tolower(static_cast<unsigned char>(key)));
  const unsigned long gap =
      last_note_key_ms_ == 0 ? 0 : now - last_note_key_ms_;

  bool continuation = false;
  if (lower == last_note_key_ && last_entered_note_ == note) {
    if (note_hold_active_ && gap <= kHoldRepeatMaxGapMs) {
      continuation = true;
    } else if (!note_hold_active_ &&
               gap >= kFirstHoldRepeatMinMs &&
               gap <= kFirstHoldRepeatMaxMs) {
      note_hold_active_ = true;
      continuation = true;
    }
  }

  if (!continuation) {
    note_hold_active_ = false;
  }

  const int step = activePatternStep();
  writeNoteEntryStep(step, note, continuation);
  last_note_key_ = lower;
  last_entered_note_ = note;
  last_entered_step_ = step;
  last_note_key_ms_ = now;
  advanceNoteEntryCursor();
  return true;
}

bool PatternEditPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) {
    return handleEventLegacy(ui_event);
  }

  char key = ui_event.key;
  if (key == 0 && ui_event.scancode >= GROOVEPUTER_F1 &&
      ui_event.scancode <= GROOVEPUTER_F8) {
    key = static_cast<char>('1' + (ui_event.scancode - GROOVEPUTER_F1));
  }
  const char lowerKey = key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(key)))
      : 0;

  // N toggles an optional direct-note layer. Disabled means the complete legacy
  // key map remains unchanged.
  if (!ui_event.ctrl && !ui_event.meta && !ui_event.alt && lowerKey == 'n') {
    note_entry_mode_ = !note_entry_mode_;
    focus_ = Focus::Steps;
    if (has_selection_) clearSelection();
    resetNoteHoldTracking();
    UI::showToast(note_entry_mode_ ? "NOTE ENTRY: ON" : "NOTE ENTRY: OFF", 900);
    return true;
  }

  if (note_entry_mode_ && !ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
    const bool isBackspace = key == '\b' || key == 0x7F;
    if (isBackspace) {
      const int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.clear303Step(step, voice_index_); });
      resetNoteHoldTracking();
      return true;
    }

    if (key == ';' || key == ':') {
      if (last_entered_note_ >= 0) {
        const int step = activePatternStep();
        writeNoteEntryStep(step, last_entered_note_, false);
        last_entered_step_ = step;
        last_note_key_ = 0;
        last_note_key_ms_ = millis();
        note_hold_active_ = false;
        advanceNoteEntryCursor();
        return true;
      }
      UI::showToast("NO LAST NOTE", 700);
      return true;
    }

    if (handleNoteEntryKey(key)) return true;

    // Any other local command ends hold inference so a later press cannot be
    // mistaken for a held-key repeat.
    resetNoteHoldTracking();
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

  // Q-I is the only keyboard path for slots 1-8 outside NOTE ENTRY. Selection
  // is immediate and the next arrow continues moving inside the note grid.
  if (!note_entry_mode_ && !ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
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
  // track mutes; plain B falls through to the legacy handler which toggles banks.
  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta &&
      (key == '1' || key == '2')) {
    const int bankIdx = bankIndexFromKey(key);
    bank_cursor_ = bankIdx;
    setBankIndex(bankIdx);
    focus_ = Focus::Steps;
    UI::showToast(bankIdx == 0 ? "Bank A (Ctrl+1)" : "Bank B (Ctrl+2)", 800);
    return true;
  }

  return handleEventLegacy(ui_event);
}
