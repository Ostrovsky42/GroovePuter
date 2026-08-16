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
#include "../../generation/migration/quantized_generation_commit.h"
#include "../../state/scene_revision.h"
#include "../../state/undo_owner.h"
#include "../../state/undo_receipts.h"
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

// 0.9.9-B2: Pattern Editor G prepares the legacy material into a
// scratch Pattern before the canonical UndoOwner publishes it. Keep
// this logic in lockstep with MiniAcid::randomize303Pattern(): same
// compiled genre parameters, Reggae voice split and mode generator.
void preparePatternEditorGeneration(MiniAcid& engine,
                                    int voiceIndex,
                                    SynthPattern& pattern) {
  const int idx = voiceIndex <= 0 ? 0 : 1;
  const GenerativeParams& genreParams =
      engine.genreManager().getCompiledGenerativeParams();
  auto behavior = engine.genreManager().getBehavior();
  if (engine.genreManager().generativeMode() == GenerativeMode::Reggae) {
    if (idx == 0) {
      behavior.stepMask = 0x1111;
      behavior.motifLength = 2;
      behavior.avoidClusters = true;
      behavior.forceOctaveJump = false;
    } else {
      behavior.stepMask = 0xAAAA;
      behavior.motifLength = 4;
      behavior.avoidClusters = false;
      behavior.forceOctaveJump = false;
    }
  }
  engine.modeManager().generatePattern(
      pattern, engine.bpm(), genreParams, behavior, idx);
}
}  // namespace

// Keep the established editor implementation intact under a private unowned
// entry point. R3 adds a narrow wrapper below that intercepts only persistent
// Pattern writes; navigation, R2 reset/Undo, copy and unrelated behavior stay in
// the retained implementation.
#define drawStandardFooter drawPatternInputLockedFooter
#define handleEvent handleEventLegacyUnowned
#include "pattern_edit_page_legacy.h"
#undef handleEvent
#undef drawStandardFooter

bool PatternEditPage::handleEventLegacy(UIEvent& ui_event) {
  // B2 generation receipts use the same bounded Synth Pattern before-
  // image as R3 manual edits, but a distinct Generation kind. Handle
  // only that exact payload here so a full B1 generation receipt is
  // not misread as a Pattern Editor receipt.
  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT &&
      ui_event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) {
    using GroovePuterUndo::SynthPatternUndoPayload;
    using GroovePuterUndo::UndoKind;
    using GroovePuterUndo::UndoResult;
    auto& owner = GroovePuterUndo::undoOwner();
    // Plain G uses the B1 quantized-generation receipt. Handle that exact
    // larger payload first; the legacy/fallback G path below uses the compact
    // SynthPattern receipt. Size discrimination prevents cross-decoding.
    if (owner.kind() == UndoKind::Generation &&
        owner.payloadSize() == GroovePuterRhythm::quantizedGenerationUndoPayloadSize()) {
      const UndoResult result =
          GroovePuterRhythm::undoLastQuantizedGeneration(mini_acid_);
      switch (result) {
        case UndoResult::Restored:
          UI::showToast("UNDO: GENERATION", 900);
          return true;
        case UndoResult::NothingToUndo:
          UI::showToast("NOTHING TO UNDO", 800);
          return true;
        case UndoResult::TargetUnavailable:
          UI::showToast("UNDO: RETURN PAGE", 1000);
          return true;
        case UndoResult::Expired:
          UI::showToast("UNDO EXPIRED", 900);
          return true;
        case UndoResult::KindMismatch:
        default:
          return false;
      }
    }
    if (owner.kind() == UndoKind::Generation &&
        owner.payloadSize() == sizeof(SynthPatternUndoPayload)) {
      const uint32_t committedRevision = owner.committedRevision();
      const UndoResult result =
          owner.undoPrepared<SynthPatternUndoPayload>(
              UndoKind::Generation,
              [&](const SynthPatternUndoPayload& receipt) {
                return GroovePuterUndo::synthPatternUndoTargetAvailable(
                    mini_acid_.sceneManager(), receipt);
              },
              [&](const SynthPatternUndoPayload& receipt) {
                const auto restore = [&]() {
                  GroovePuterUndo::restoreSynthPatternUndo(
                      mini_acid_.sceneManager(), receipt);
                };
                if (audio_guard_) audio_guard_(restore);
                else restore();
              });
      if (result == UndoResult::Restored) {
        GroovePuterRhythm::QuantizedGenerationDetail::
            cancelPendingGenerationActivationForRevision(
                mini_acid_, committedRevision);
      }
      switch (result) {
        case UndoResult::Restored:
          UI::showToast("UNDO: GENERATION", 900);
          return true;
        case UndoResult::NothingToUndo:
          UI::showToast("NOTHING TO UNDO", 800);
          return true;
        case UndoResult::TargetUnavailable:
          UI::showToast("UNDO: RETURN PAGE", 1000);
          return true;
        case UndoResult::Expired:
          UI::showToast("UNDO EXPIRED", 900);
          return true;
        case UndoResult::KindMismatch:
        default:
          return false;
      }
    }
  }
  using GroovePuterUndo::PatternEdit::adjustFxParam;
  using GroovePuterUndo::PatternEdit::adjustNote;
  using GroovePuterUndo::PatternEdit::adjustOctave;
  using GroovePuterUndo::PatternEdit::clearStep;
  using GroovePuterUndo::PatternEdit::cycleFx;
  using GroovePuterUndo::PatternEdit::rotate;
  using GroovePuterUndo::PatternEdit::setAccent;
  using GroovePuterUndo::PatternEdit::setSlide;

  // Paste is a persistent Pattern edit. Build the complete destination on the
  // prepared copy, then publish one receipt and perform one bounded COMMIT.
  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT &&
      ui_event.app_event_type == GROOVEPUTER_APP_EVENT_PASTE) {
    if (!g_pattern_step_clipboard.has_data && !g_pattern_clipboard.has_pattern) {
      return handleEventLegacyUnowned(ui_event);
    }

    commitPatternMutation([&](SynthPattern& dst) {
      if (g_pattern_step_clipboard.has_data) {
        int start_row = activePatternStep() / kPatternStepColumns;
        int start_col = activePatternStep() % kPatternStepColumns;
        if (has_selection_) {
          int min_row, max_row, min_col, max_col;
          getSelectionBounds(min_row, max_row, min_col, max_col);
          start_row = min_row;
          start_col = min_col;
        }

        int idx = 0;
        for (int r = 0; r < g_pattern_step_clipboard.rows; ++r) {
          for (int c = 0; c < g_pattern_step_clipboard.cols; ++c) {
            if (idx >= static_cast<int>(g_pattern_step_clipboard.steps.size())) break;
            const int tr = start_row + r;
            const int tc = start_col + c;
            if (tr < 0 || tr >= kPatternStepRows ||
                tc < 0 || tc >= kPatternStepColumns) {
              ++idx;
              continue;
            }
            dst.steps[tr * kPatternStepColumns + tc] =
                g_pattern_step_clipboard.steps[idx++];
          }
        }
      } else {
        const SynthPattern& src = g_pattern_clipboard.pattern;
        for (int i = 0; i < SEQ_STEPS; ++i) dst.steps[i] = src.steps[i];
      }
    });
    if (has_selection_) clearSelection();
    return true;
  }

  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) {
    return handleEventLegacyUnowned(ui_event);
  }

  const int nav = UIInput::navCode(ui_event);
  char key = ui_event.key;
  if (key == 0 && ui_event.scancode >= GROOVEPUTER_F1 &&
      ui_event.scancode <= GROOVEPUTER_F8) {
    key = static_cast<char>('1' + (ui_event.scancode - GROOVEPUTER_F1));
  }
  const char lowerKey = key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(key)))
      : 0;
  const bool isBackspace = key == '\b' || key == 0x7F;

  // R2 owns Reset Pattern and the existing application Undo implementation.
  // Keep that exact vertical slice in the retained handler.
  if (ui_event.alt && isBackspace) {
    return handleEventLegacyUnowned(ui_event);
  }

  auto prepareSelectionOrCursor = [&](auto&& edit) {
    if (patternRowFocused()) focusPatternSteps();
    else ensureStepFocus();

    if (has_selection_) {
      int min_row, max_row, min_col, max_col;
      getSelectionBounds(min_row, max_row, min_col, max_col);
      return commitPatternMutation([&](SynthPattern& pattern) {
        for (int r = min_row; r <= max_row; ++r) {
          for (int c = min_col; c <= max_col; ++c) {
            edit(pattern, r * kPatternStepColumns + c);
          }
        }
      });
    }

    const int step = activePatternStep();
    return commitPatternMutation(
        [&](SynthPattern& pattern) { edit(pattern, step); });
  };

  // Pattern rotation is one logical mutation regardless of 16 affected steps.
  if (ui_event.alt &&
      (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)) {
    const int dir = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    commitPatternMutation(
        [&](SynthPattern& pattern) { rotate(pattern, dir); });
    return true;
  }

  // Meta arrows preserve the legacy note/octave semantics, but prepare the
  // whole selected edit before publishing Undo/revision state.
  if (ui_event.meta) {
    switch (nav) {
      case GROOVEPUTER_UP:
        prepareSelectionOrCursor(
            [&](SynthPattern& pattern, int step) { adjustNote(pattern, step, 1); });
        return true;
      case GROOVEPUTER_DOWN:
        prepareSelectionOrCursor(
            [&](SynthPattern& pattern, int step) { adjustNote(pattern, step, -1); });
        return true;
      case GROOVEPUTER_LEFT:
        prepareSelectionOrCursor(
            [&](SynthPattern& pattern, int step) { adjustOctave(pattern, step, -1); });
        return true;
      case GROOVEPUTER_RIGHT:
        prepareSelectionOrCursor(
            [&](SynthPattern& pattern, int step) { adjustOctave(pattern, step, 1); });
        return true;
      default:
        break;
    }
  }

  if (ui_event.alt &&
      (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN)) {
    ensureStepFocus();
    const int step = activePatternStep();
    const int delta = nav == GROOVEPUTER_UP ? 1 : -1;
    commitPatternMutation([&](SynthPattern& pattern) {
      adjustFxParam(pattern, step, delta);
    });
    return true;
  }

  const bool keyA = lowerKey == 'a' || ui_event.scancode == GROOVEPUTER_A;
  const bool keyS = lowerKey == 's' || ui_event.scancode == GROOVEPUTER_S;
  const bool keyZ = lowerKey == 'z' || ui_event.scancode == GROOVEPUTER_Z;
  const bool keyX = lowerKey == 'x' || ui_event.scancode == GROOVEPUTER_X;
  const bool keyG = lowerKey == 'g' || ui_event.scancode == GROOVEPUTER_G;
  const bool keyF = lowerKey == 'f' || ui_event.scancode == GROOVEPUTER_F;
  const bool keyV = lowerKey == 'v' || ui_event.scancode == GROOVEPUTER_V;

  // The retained Ctrl+V handler recursively calls the retained handler after
  // macro-renaming. Intercept it here so Paste cannot bypass R3 ownership.
  if (keyV && ui_event.ctrl) {
    UIEvent appEvent = ui_event;
    appEvent.event_type = GROOVEPUTER_APPLICATION_EVENT;
    appEvent.app_event_type = GROOVEPUTER_APP_EVENT_PASTE;
    return handleEventLegacy(appEvent);
  }

  if (keyS) {
    if (ui_event.alt || ui_event.ctrl) {
      if (patternRowFocused()) focusPatternSteps();
      else ensureStepFocus();
      if (has_selection_) {
        int min_row, max_row, min_col, max_col;
        getSelectionBounds(min_row, max_row, min_col, max_col);
        commitPatternMutation([&](SynthPattern& pattern) {
          const bool target = !pattern.steps[min_row * kPatternStepColumns + min_col].slide;
          for (int r = min_row; r <= max_row; ++r) {
            for (int c = min_col; c <= max_col; ++c) {
              setSlide(pattern, r * kPatternStepColumns + c, target);
            }
          }
        });
      } else {
        const int step = activePatternStep();
        commitPatternMutation([&](SynthPattern& pattern) {
          GroovePuterUndo::PatternEdit::toggleSlide(pattern, step);
        });
      }
    } else {
      prepareSelectionOrCursor(
          [&](SynthPattern& pattern, int step) { adjustOctave(pattern, step, 1); });
    }
    return true;
  }

  if (keyA) {
    if (ui_event.alt || ui_event.ctrl) {
      if (patternRowFocused()) focusPatternSteps();
      else ensureStepFocus();
      if (has_selection_) {
        int min_row, max_row, min_col, max_col;
        getSelectionBounds(min_row, max_row, min_col, max_col);
        commitPatternMutation([&](SynthPattern& pattern) {
          const bool target = !pattern.steps[min_row * kPatternStepColumns + min_col].accent;
          for (int r = min_row; r <= max_row; ++r) {
            for (int c = min_col; c <= max_col; ++c) {
              setAccent(pattern, r * kPatternStepColumns + c, target);
            }
          }
        });
      } else {
        const int step = activePatternStep();
        commitPatternMutation([&](SynthPattern& pattern) {
          GroovePuterUndo::PatternEdit::toggleAccent(pattern, step);
        });
      }
    } else {
      prepareSelectionOrCursor(
          [&](SynthPattern& pattern, int step) { adjustNote(pattern, step, 1); });
    }
    return true;
  }

  if (keyZ) {
    prepareSelectionOrCursor(
        [&](SynthPattern& pattern, int step) { adjustNote(pattern, step, -1); });
    return true;
  }

  if (keyX) {
    prepareSelectionOrCursor(
        [&](SynthPattern& pattern, int step) { adjustOctave(pattern, step, -1); });
    return true;
  }

  // C keeps B2's legacy/fallback musical generator but joins the one
  // bounded activation contract. STOP commits and is audible immediately.
  // PLAY arms the old Pattern as audible truth, commits the new Pattern now,
  // then releases that overlay only at BAR_START.
  if (keyG) {
    using GroovePuterUndo::SynthPatternUndoPayload;
    using GroovePuterUndo::UndoKind;

    SceneManager& manager = mini_acid_.sceneManager();
    SynthPatternUndoPayload before{};
    if (!GroovePuterUndo::captureCurrentSynthPatternUndo(
            manager, voice_index_, before)) {
      return true;
    }

    SynthPattern after = before.before;
    preparePatternEditorGeneration(mini_acid_, voice_index_, after);
    if (GroovePuterUndo::PatternEdit::samePattern(before.before, after)) {
      return true;
    }
    if (!GroovePuterUndo::synthPatternUndoTargetAvailable(manager, before)) {
      return true;
    }

    SynthPatternUndoPayload prepared = before;
    prepared.before = after;
    int activationSlot = -1;
    if (mini_acid_.isPlaying()) {
      const auto target = GroovePuterRhythm::QuantizedGenerationDetail::
          captureGenerationActivationTarget(manager);
      activationSlot = GroovePuterRhythm::QuantizedGenerationDetail::
          armCompactSynthActivation(
              mini_acid_, target, voice_index_, before.before);
      if (activationSlot < 0) {
        UI::showToast("GEN BUSY", 800);
        return true;
      }
    }

    const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
        UndoKind::Generation, before, [&]() {
          GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);
        });
    if (!committed) {
      if (activationSlot >= 0) {
        GroovePuterRhythm::QuantizedGenerationDetail::abortArmedActivation(
            activationSlot,
            GroovePuterRhythm::QuantizedGenerationStatus::Busy);
      }
      return true;
    }

    if (activationSlot >= 0) {
      GroovePuterRhythm::QuantizedGenerationDetail::completeArmedActivation(
          activationSlot,
          GroovePuterUndo::undoOwner().committedRevision());
      UI::showToast("GEN -> NEXT BAR", 1000);
    }
    return true;
  }

  if (keyF) {
    ensureStepFocus();
    const int step = activePatternStep();
    commitPatternMutation(
        [&](SynthPattern& pattern) { cycleFx(pattern, step); });
    return true;
  }

  if (isBackspace && has_selection_) {
    int min_row, max_row, min_col, max_col;
    getSelectionBounds(min_row, max_row, min_col, max_col);
    commitPatternMutation([&](SynthPattern& pattern) {
      for (int r = min_row; r <= max_row; ++r) {
        for (int c = min_col; c <= max_col; ++c) {
          clearStep(pattern, r * kPatternStepColumns + c);
        }
      }
    });
    clearSelection();
    UI::showToast("Selection Cleared");
    return true;
  }

  if (isBackspace) {
    if (patternRowFocused()) focusPatternSteps();
    else ensureStepFocus();
    const int step = activePatternStep();
    commitPatternMutation(
        [&](SynthPattern& pattern) { clearStep(pattern, step); });
    return true;
  }

  return handleEventLegacyUnowned(ui_event);
}

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
  commitPatternMutation([&](SynthPattern& pattern) {
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

  const int cursorStep = activePatternStep();
  int writeStep = cursorStep;

  if (continuation) {
    if (last_entered_step_ < 0 || last_entered_step_ >= SEQ_STEPS - 1) {
      last_note_key_ms_ = now;
      return true;
    }
    writeStep = last_entered_step_ + 1;
  } else {
    note_hold_active_ = false;
  }

  writeNoteEntryStep(writeStep, note, continuation);
  last_note_key_ = lower;
  last_entered_note_ = note;
  last_entered_step_ = writeStep;
  last_note_key_ms_ = now;
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
  const int nav = UIInput::navCode(ui_event);
  const bool keyG =
      lowerKey == 'g' || ui_event.scancode == GROOVEPUTER_G;
  const bool gridArrow =
      nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT ||
      nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN;

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

  // Cardputer ADV emits the physical arrow legends through Fn-modified
  // punctuation HID codes, so those events carry meta=true. NOTE ENTRY owns
  // arrow scancodes explicitly before the legacy/meta router can reject them.
  if (note_entry_mode_ && gridArrow && !ui_event.alt && !ui_event.ctrl) {
    focus_ = Focus::Steps;
    if (has_selection_) clearSelection();
    resetNoteHoldTracking();

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

  if (note_entry_mode_ && !ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
    const bool isBackspace = key == '\b' || key == 0x7F;
    if (isBackspace) {
      const int step = activePatternStep();
      commitPatternMutation([&](SynthPattern& pattern) {
        GroovePuterUndo::PatternEdit::clearStep(pattern, step);
      });
      resetNoteHoldTracking();
      return true;
    }

    if (key == '\n' || key == '\r') {
      advanceNoteEntryCursor();
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
        return true;
      }
      UI::showToast("NO LAST NOTE", 700);
      return true;
    }

    if (handleNoteEntryKey(key)) return true;

    // Any other local command ends hold inference so a later press cannot be
    // mistaken for a held-key repeat. The last entered pitch is intentionally
    // retained so ';' can still recall it after navigation.
    resetNoteHoldTracking();
  }

  // Outside NOTE ENTRY, plain G rerolls only this physical synth voice through
  // the active Genre/recipe/P-level/harmony context. Drums and the other synth
  // remain owned by their current patterns. Modified editor commands remain in
  // the retained legacy handler.
  if (!note_entry_mode_ && keyG &&
      !ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
    using GroovePuterRhythm::QuantizedGenerationResult;
    QuantizedGenerationResult result = QuantizedGenerationResult::Failed;
    const auto generate = [&]() {
      result = GroovePuterRhythm::regenerateSynthWithQuantizedCommit(
          mini_acid_, voice_index_);
    };
    if (mini_acid_.isPlaying()) {
      generate();
    } else {
      withAudioGuard(generate);
    }
    // B1 commitPrepared() is the sole persistent revision owner. Do not
    // advance Scene revision again here for CommittedNow.

    const char* label = "GEN FAILED";
    if (result == QuantizedGenerationResult::CommittedNow)
      label = "GENERATED";
    else if (result == QuantizedGenerationResult::PendingNextBar)
      label = "GEN -> NEXT BAR";
    else if (result == QuantizedGenerationResult::AttemptUnavailable)
      label = "GEN ATTEMPT FULL";
    UI::showToast(label, 1200);
    return true;
  }

  // Global navigation, pattern rotation/FX editing and meta note editing keep
  // their existing behavior. Only unmodified/selection arrows are grid-owned.
  if (UIInput::isGlobalNav(ui_event)) {
    return handleEventLegacy(ui_event);
  }

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
      bool songMutated = false;
      withAudioGuard([&]() {
        mini_acid_.set303PatternIndex(voice_index_, patternIdx);
        if (chaining_mode_) {
          const SongTrack track = voice_index_ == 0
              ? SongTrack::SynthA
              : SongTrack::SynthB;
          for (int i = 0; i < Song::kMaxPositions; ++i) {
            if (mini_acid_.songPatternAt(i, track) == -1) {
              mini_acid_.setSongPattern(i, track, patternIdx);
              songMutated = true;
              break;
            }
          }
        }
      });
      if (songMutated) GroovePuterState::markSceneMutated();
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
