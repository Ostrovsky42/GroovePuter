#include "pattern_edit_page.h"

#include <cctype>
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

#ifdef USE_RETRO_THEME
using namespace RetroTheme;
using namespace RetroWidgets;
#endif

namespace {
inline IGfxColor voiceColor(int voiceIndex) {
  return (voiceIndex == 0) ? IGfxColor(0x33C8FF) : IGfxColor(0xFF4FCB);
}

inline IGfxColor retroVoiceColor(int voiceIndex) {
  return (voiceIndex == 0) ? IGfxColor(NEON_CYAN) : IGfxColor(NEON_MAGENTA);
}

inline IGfxColor amberVoiceColor(int voiceIndex) {
  return (voiceIndex == 0) ? IGfxColor(AmberTheme::NEON_CYAN) : IGfxColor(AmberTheme::NEON_MAGENTA);
}

struct PatternStepAreaClipboard {
  bool has_data = false;
  bool full_row = false;
  int rows = 0;
  int cols = 0;
  std::vector<SynthStep> steps;
};

PatternStepAreaClipboard g_pattern_step_clipboard;
} // namespace

PatternEditPage::PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voice_index)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    audio_guard_(audio_guard),
    voice_index_(voice_index),
    pattern_edit_cursor_(0),
    pattern_row_cursor_(0),
    bank_index_(0),
    bank_cursor_(0),
    focus_(Focus::Steps) {
  int idx = mini_acid_.current303PatternIndex(voice_index_);
  if (idx < 0 || idx >= Bank<SynthPattern>::kPatterns) idx = 0;
  pattern_row_cursor_ = idx;
  bank_index_ = mini_acid_.current303BankIndex(voice_index_);
  bank_cursor_ = bank_index_;
  title_ = voice_index_ == 0 ? "303A PATTERNS" : "303B PATTERNS";
  pattern_bar_ = std::make_shared<PatternSelectionBarComponent>("PATTERNS");
  bank_bar_ = std::make_shared<BankSelectionBarComponent>("BANK", "AB");
  PatternSelectionBarComponent::Callbacks pattern_callbacks;
  pattern_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    focusPatternRow();
    setPatternCursor(index);
    withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, index); });
  };
  pattern_bar_->setCallbacks(std::move(pattern_callbacks));
  BankSelectionBarComponent::Callbacks bank_callbacks;
  bank_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    focus_ = Focus::BankRow;
    bank_cursor_ = index;
    setBankIndex(index);
  };
  bank_bar_->setCallbacks(std::move(bank_callbacks));
}

int PatternEditPage::clampCursor(int cursorIndex) const {
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<SynthPattern>::kPatterns)
    cursor = Bank<SynthPattern>::kPatterns - 1;
  return cursor;
}

int PatternEditPage::activeBankCursor() const {
  int cursor = bank_cursor_;
  if (cursor < 0) cursor = 0;
  if (cursor >= kBankCount) cursor = kBankCount - 1;
  return cursor;
}

int PatternEditPage::patternIndexFromKey(char key) const {
  return qwertyToPatternIndex(key);
}

int PatternEditPage::bankIndexFromKey(char key) const {
  switch (key) {
    case '1': return 0;
    case '2': return 1;
    default: return -1;
  }
}

void PatternEditPage::setBankIndex(int bankIndex) {
  if (bankIndex < 0) bankIndex = 0;
  if (bankIndex >= kBankCount) bankIndex = kBankCount - 1;
  if (bank_index_ == bankIndex) return;
  bank_index_ = bankIndex;
  withAudioGuard([&]() { mini_acid_.set303BankIndex(voice_index_, bank_index_); });
}

void PatternEditPage::ensureStepFocus() {
  if (patternRowFocused() || focus_ == Focus::BankRow) focus_ = Focus::Steps;
}

int PatternEditPage::activePatternCursor() const {
  return clampCursor(pattern_row_cursor_);
}

int PatternEditPage::activePatternStep() const {
  int idx = pattern_edit_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}

void PatternEditPage::setPatternCursor(int cursorIndex) {
  pattern_row_cursor_ = clampCursor(cursorIndex);
}

void PatternEditPage::focusPatternRow() {
  if (mini_acid_.songModeEnabled()) return;
  setPatternCursor(pattern_row_cursor_);
  focus_ = Focus::PatternRow;
}

void PatternEditPage::focusPatternSteps() {
  int row = pattern_edit_cursor_ / 8;
  if (row < 0 || row > 1) row = 0;
  pattern_edit_cursor_ = row * 8 + activePatternCursor();
  focus_ = Focus::Steps;
}

bool PatternEditPage::patternRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return focus_ == Focus::PatternRow;
}

void PatternEditPage::movePatternCursor(int delta) {
  if (mini_acid_.songModeEnabled() && focus_ == Focus::PatternRow) {
    focus_ = Focus::Steps;
  }
  if (focus_ == Focus::BankRow) {
    int cursor = activeBankCursor();
    cursor = (cursor + delta) % kBankCount;
    if (cursor < 0) cursor += kBankCount;
    bank_cursor_ = cursor;
    return;
  }
  if (focus_ == Focus::PatternRow) {
    int cursor = activePatternCursor();
    cursor = (cursor + delta) % Bank<SynthPattern>::kPatterns;
    if (cursor < 0) cursor += Bank<SynthPattern>::kPatterns;
    pattern_row_cursor_ = cursor;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  col = (col + delta) % 8;
  if (col < 0) col += 8;
  pattern_edit_cursor_ = row * 8 + col;
}

void PatternEditPage::movePatternCursorVertical(int delta) {
  if (delta == 0) return;
  if (mini_acid_.songModeEnabled() && focus_ == Focus::PatternRow) {
    focus_ = Focus::Steps;
  }
  if (focus_ == Focus::BankRow) {
    if (delta > 0) {
      focus_ = mini_acid_.songModeEnabled() ? Focus::Steps : Focus::PatternRow;
    }
    return;
  }
  if (focus_ == Focus::PatternRow) {
    if (delta < 0 && !mini_acid_.songModeEnabled()) {
      bank_cursor_ = bank_index_;
      focus_ = Focus::BankRow;
      return;
    }
    int col = activePatternCursor();
    int targetRow = delta > 0 ? 0 : 1;
    pattern_edit_cursor_ = targetRow * 8 + col;
    focus_ = Focus::Steps;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  int newRow = row + delta;
  if (newRow < 0) {
    if (mini_acid_.songModeEnabled()) newRow = 0;
    else {
      focus_ = Focus::PatternRow;
      setPatternCursor(col);
      return;
    }
  }
  if (newRow > 1) {
    if (mini_acid_.songModeEnabled()) newRow = 1;
    else {
      focus_ = Focus::PatternRow;
      setPatternCursor(col);
      return;
    }
  }
  pattern_edit_cursor_ = newRow * 8 + col;
}

void PatternEditPage::startSelection() {
  has_selection_ = true;
  selection_locked_ = false;
  selection_start_step_ = activePatternStep();
}

void PatternEditPage::updateSelection() {
  if (!has_selection_) startSelection();
}

void PatternEditPage::clearSelection() {
  has_selection_ = false;
  selection_locked_ = false;
}

bool PatternEditPage::hasSelection() const {
  return has_selection_;
}

void PatternEditPage::getSelectionBounds(int& min_row, int& max_row, int& min_col, int& max_col) const {
  int a = selection_start_step_;
  if (a < 0) a = 0;
  if (a >= SEQ_STEPS) a = SEQ_STEPS - 1;
  int b = pattern_edit_cursor_;
  if (b < 0) b = 0;
  if (b >= SEQ_STEPS) b = SEQ_STEPS - 1;
  int ar = a / 8, ac = a % 8;
  int br = b / 8, bc = b % 8;
  min_row = std::min(ar, br);
  max_row = std::max(ar, br);
  min_col = std::min(ac, bc);
  max_col = std::max(ac, bc);
}

bool PatternEditPage::isStepSelected(int stepIndex) const {
  if (!has_selection_) return false;
  int row = stepIndex / 8;
  int col = stepIndex % 8;
  int min_row, max_row, min_col, max_col;
  getSelectionBounds(min_row, max_row, min_col, max_col);
  return row >= min_row && row <= max_row && col >= min_col && col <= max_col;
}

bool PatternEditPage::moveSelectionFrameBy(int deltaRow, int deltaCol) {
  if (!has_selection_) return false;
  int min_row, max_row, min_col, max_col;
  getSelectionBounds(min_row, max_row, min_col, max_col);
  int dst_min_row = min_row + deltaRow;
  int dst_max_row = max_row + deltaRow;
  int dst_min_col = min_col + deltaCol;
  int dst_max_col = max_col + deltaCol;
  if (dst_min_row < 0 || dst_max_row > 1 || dst_min_col < 0 || dst_max_col > 7) return false;
  selection_start_step_ += deltaRow * 8 + deltaCol;
  pattern_edit_cursor_ += deltaRow * 8 + deltaCol;
  return true;
}

const std::string & PatternEditPage::getTitle() const {
  return title_;
}

void PatternEditPage::setContext(int context) {
    // Decode step index (0-15)
    int step = context;
    if (step < 0) step = 0;
    if (step >= SEQ_STEPS) step = SEQ_STEPS - 1;
    
    // Set focus to the specific step
    pattern_edit_cursor_ = step;
    focus_ = Focus::Steps;
    
    // Sync UI selection bars with current engine state
    pattern_row_cursor_ = mini_acid_.current303PatternIndex(voice_index_);
    bank_index_ = mini_acid_.current303BankIndex(voice_index_);
    bank_cursor_ = bank_index_;
}

bool PatternEditPage::handleEvent(UIEvent& ui_event) {
  // Let global numeric mutes (1..0) pass through to MiniAcidDisplay.
  // We intentionally skip local numeric quick-select on this page.
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN) {
    if (!ui_event.shift && !ui_event.ctrl && !ui_event.meta &&
        ui_event.key >= '0' && ui_event.key <= '9') {
      return false;
    }
  }

  if (pattern_bar_ && pattern_bar_->handleEvent(ui_event)) return true;
  if (bank_bar_ && bank_bar_->handleEvent(ui_event)) return true;
  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT) {
    switch (ui_event.app_event_type) {
      case GROOVEPUTER_APP_EVENT_COPY: {
        int patIdx = activePatternCursor();
        const SynthPattern& source = mini_acid_.sceneManager().getSynthPattern(voice_index_, patIdx);
        g_pattern_step_clipboard.has_data = true;
        g_pattern_step_clipboard.steps.clear();
        if (has_selection_) {
          int min_row, max_row, min_col, max_col;
          getSelectionBounds(min_row, max_row, min_col, max_col);
          const bool single_cell = (min_row == max_row && min_col == max_col);
          if (single_cell) {
            // Single-cell selection: copy whole row (8 steps) as requested.
            int row = min_row;
            g_pattern_step_clipboard.full_row = true;
            g_pattern_step_clipboard.rows = 1;
            g_pattern_step_clipboard.cols = 8;
            g_pattern_step_clipboard.steps.reserve(8);
            for (int c = 0; c < 8; ++c) {
              int step = row * 8 + c;
              g_pattern_step_clipboard.steps.push_back(source.steps[step]);
            }
          } else {
            g_pattern_step_clipboard.full_row = false;
            g_pattern_step_clipboard.rows = max_row - min_row + 1;
            g_pattern_step_clipboard.cols = max_col - min_col + 1;
            g_pattern_step_clipboard.steps.reserve(g_pattern_step_clipboard.rows * g_pattern_step_clipboard.cols);
            for (int r = min_row; r <= max_row; ++r) {
              for (int c = min_col; c <= max_col; ++c) {
                int step = r * 8 + c;
                g_pattern_step_clipboard.steps.push_back(source.steps[step]);
              }
            }
          }
        } else {
          // No selection: keep legacy full-pattern copy.
          g_pattern_step_clipboard.full_row = false;
          g_pattern_step_clipboard.rows = 2;
          g_pattern_step_clipboard.cols = 8;
          g_pattern_step_clipboard.steps.reserve(SEQ_STEPS);
          for (int i = 0; i < SEQ_STEPS; ++i) {
            g_pattern_step_clipboard.steps.push_back(source.steps[i]);
          }
        }
        if (has_selection_) {
          // Area copy is authoritative; avoid stale full-pattern clipboard consumers.
          g_pattern_clipboard.has_pattern = false;
        } else {
          g_pattern_clipboard.has_pattern = true;
          for (int i = 0; i < SEQ_STEPS; ++i) g_pattern_clipboard.pattern.steps[i] = source.steps[i];
        }
        if (has_selection_) selection_locked_ = true;
        return true;
      }
      case GROOVEPUTER_APP_EVENT_PASTE: {
        if (!g_pattern_step_clipboard.has_data && !g_pattern_clipboard.has_pattern) return false;
        withAudioGuard([&]() {
          int vIdx = (voice_index_ < 0) ? 0 : (voice_index_ >= 2 ? 1 : voice_index_);
          SynthPattern& dst = mini_acid_.sceneManager().editCurrentSynthPattern(vIdx);
          if (g_pattern_step_clipboard.has_data) {
            int start_row = activePatternStep() / 8;
            int start_col = activePatternStep() % 8;
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
                int tr = start_row + r;
                int tc = start_col + c;
                if (tr < 0 || tr > 1 || tc < 0 || tc > 7) {
                  ++idx;
                  continue;
                }
                int step = tr * 8 + tc;
                dst.steps[step] = g_pattern_step_clipboard.steps[idx++];
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
      default:
        return false;
    }
  }
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  // Alt+Esc must be handled before global Esc navigation.
  if (((ui_event.scancode == GROOVEPUTER_ESCAPE) || (ui_event.key == 0x1B)) && ui_event.alt) {
    chaining_mode_ = !chaining_mode_;
    return true;
  }

  // Handle local ESC/backtick selection clear before global nav steals the key.
  const bool early_is_escape = (ui_event.scancode == GROOVEPUTER_ESCAPE) || (ui_event.key == 0x1B);
  const bool early_is_backtick = (ui_event.key == '`' || ui_event.key == '~');
  if ((early_is_escape || early_is_backtick) && has_selection_) {
    clearSelection();
    return true;
  }

  // Let parent handle global navigation keys; do not steal them here.
  if (UIInput::isGlobalNav(ui_event)) return false;

  bool handled = false;
  
  // Arrow-first: Cardputer may deliver arrows in scancode OR key.
  // Keep vim-keys only as silent fallback (not in footer hints).
  int nav = UIInput::navCode(ui_event);
  bool extend_selection = (ui_event.shift || ui_event.ctrl) && !ui_event.alt;
  if (ui_event.meta) {
    auto applyMetaStep = [&](auto&& fn) {
      if (patternRowFocused()) {
        focusPatternSteps();
      } else {
        ensureStepFocus();
      }
      if (has_selection_) {
        int min_row, max_row, min_col, max_col;
        getSelectionBounds(min_row, max_row, min_col, max_col);
        withAudioGuard([&]() {
          for (int r = min_row; r <= max_row; ++r) {
            for (int c = min_col; c <= max_col; ++c) {
              fn(r * 8 + c);
            }
          }
        });
      } else {
        int step = activePatternStep();
        withAudioGuard([&]() { fn(step); });
      }
    };
    switch (nav) {
      case GROOVEPUTER_UP:
        applyMetaStep([&](int step) { mini_acid_.adjust303StepNote(voice_index_, step, 1); });
        return true;
      case GROOVEPUTER_DOWN:
        applyMetaStep([&](int step) { mini_acid_.adjust303StepNote(voice_index_, step, -1); });
        return true;
      case GROOVEPUTER_LEFT:
        applyMetaStep([&](int step) { mini_acid_.adjust303StepOctave(voice_index_, step, -1); });
        return true;
      case GROOVEPUTER_RIGHT:
        applyMetaStep([&](int step) { mini_acid_.adjust303StepOctave(voice_index_, step, 1); });
        return true;
      default:
        break;
    }
  }
  if (extend_selection && selection_locked_) selection_locked_ = false;
  if (selection_locked_ && has_selection_ && !extend_selection && focus_ == Focus::Steps) {
    switch (nav) {
      case GROOVEPUTER_LEFT: return moveSelectionFrameBy(0, -1);
      case GROOVEPUTER_RIGHT: return moveSelectionFrameBy(0, 1);
      case GROOVEPUTER_UP: return moveSelectionFrameBy(-1, 0);
      case GROOVEPUTER_DOWN: return moveSelectionFrameBy(1, 0);
      default: break;
    }
  }
  switch (nav) {
    case GROOVEPUTER_LEFT:
      if (extend_selection && focus_ == Focus::Steps) updateSelection();
      movePatternCursor(-1);
      handled = true;
      break;
    case GROOVEPUTER_RIGHT:
      if (extend_selection && focus_ == Focus::Steps) updateSelection();
      movePatternCursor(1);
      handled = true;
      break;
    case GROOVEPUTER_UP:
      if (ui_event.alt) {
           ensureStepFocus();
           int step = activePatternStep();
           withAudioGuard([&]() { mini_acid_.adjust303StepFxParam(voice_index_, step, 1); });
           handled = true;
      } else {
          if (extend_selection && focus_ == Focus::Steps) updateSelection();
          movePatternCursorVertical(-1);
          handled = true;
      }
      break;
    case GROOVEPUTER_DOWN:
      if (ui_event.alt) {
           ensureStepFocus();
           int step = activePatternStep();
           withAudioGuard([&]() { mini_acid_.adjust303StepFxParam(voice_index_, step, -1); });
           handled = true;
      } else {
          if (extend_selection && focus_ == Focus::Steps) updateSelection();
          movePatternCursorVertical(1);
          handled = true;
      }
      break;
    default:
      break;
  }
  if (handled) return true;

  // Handle TAB for voice toggle
  if (UIInput::isTab(ui_event)) {
      voice_index_ = (voice_index_ + 1) % 2; // Toggle 0 <-> 1
      title_ = voice_index_ == 0 ? "303A PATTERNS" : "303B PATTERNS";
      
      // Refresh state for new voice
      bank_index_ = mini_acid_.current303BankIndex(voice_index_);
      bank_cursor_ = bank_index_;
      pattern_row_cursor_ = mini_acid_.current303PatternIndex(voice_index_);
      if (pattern_row_cursor_ < 0) pattern_row_cursor_ = 0;
      
      return true;
  }

  char key = ui_event.key;
  char lowerKey = key ? static_cast<char>(std::tolower(static_cast<unsigned char>(key))) : 0;
  const bool is_escape = (ui_event.scancode == GROOVEPUTER_ESCAPE) || (key == 0x1B);
  const bool is_backspace = (key == '\b' || key == 0x7F);

  // Let app-level back navigation handle ESC when nothing local to clear.
  if (is_escape) return false;

  // Q-I Pattern Selection (Standardized) - PRIORITIZED
  if (!ui_event.shift && !ui_event.ctrl && !ui_event.meta && !ui_event.alt) {
    int patternIdx = patternIndexFromKey(lowerKey);
    if (patternIdx < 0) {
        patternIdx = scancodeToPatternIndex(ui_event.scancode);
    }
    
    if (patternIdx >= 0) {
      if (mini_acid_.songModeEnabled()) return true;
      focusPatternRow();
      setPatternCursor(patternIdx);
      withAudioGuard([&]() { 
          mini_acid_.set303PatternIndex(voice_index_, patternIdx);
          
          if (chaining_mode_) {
              // Find next empty position in song and append
              SongTrack track = (voice_index_ == 0) ? SongTrack::SynthA : SongTrack::SynthB;
              int songLen = mini_acid_.songLength();
              int nextPos = -1;
              
              // Search for the first empty slot (-1) or the first slot after the last used one
              for (int i = 0; i < Song::kMaxPositions; ++i) {
                  if (mini_acid_.songPatternAt(i, track) == -1) {
                      nextPos = i;
                      break;
                  }
              }
              
              if (nextPos != -1) {
                  // If we found an empty slot, put it there. 
                  // If it's the first empty slot, it effectively appends if previous were full.
                  mini_acid_.setSongPattern(nextPos, track, patternIdx);
              }
          }
      });
      return true;
    }
  }

  /*
  int bankIdx = bankIndexFromKey(key);
  if (bankIdx >= 0) {
    setBankIndex(bankIdx);
    if (!mini_acid_.songModeEnabled()) focus_ = Focus::BankRow;
    return true;
  }
    */

  if (key == '\n' || key == '\r') {
    if (has_selection_) {
      int min_row, max_row, min_col, max_col;
      getSelectionBounds(min_row, max_row, min_col, max_col);
      if (min_row == max_row && min_col == max_col) {
        clearSelection();
        return true;
      }
    }
    if (focus_ == Focus::BankRow) {
      if (mini_acid_.songModeEnabled()) return true;
      setBankIndex(activeBankCursor());
      return true;
    }
    if (patternRowFocused()) {
      if (mini_acid_.songModeEnabled()) return true;
      int cursor = activePatternCursor();
      setPatternCursor(cursor);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, cursor); });
      return true;
    }
  }


  auto ensureStepFocusAndCursor = [&]() {
    if (patternRowFocused()) {
      focusPatternSteps();
    } else {
      ensureStepFocus();
    }
  };
  auto applyToSelectionOrCursor = [&](auto&& fn) {
    ensureStepFocusAndCursor();
    if (has_selection_) {
      int min_row, max_row, min_col, max_col;
      getSelectionBounds(min_row, max_row, min_col, max_col);
      withAudioGuard([&]() {
        for (int r = min_row; r <= max_row; ++r) {
          for (int c = min_col; c <= max_col; ++c) {
            fn(r * 8 + c);
          }
        }
      });
    } else {
      int step = activePatternStep();
      withAudioGuard([&]() { fn(step); });
    }
  };

  bool key_a = (lowerKey == 'a') || (ui_event.scancode == GROOVEPUTER_A);
  bool key_b = (lowerKey == 'b') || (ui_event.scancode == GROOVEPUTER_B);
  bool key_s = (lowerKey == 's') || (ui_event.scancode == GROOVEPUTER_S);
  bool key_z = (lowerKey == 'z') || (ui_event.scancode == GROOVEPUTER_Z);
  bool key_x = (lowerKey == 'x') || (ui_event.scancode == GROOVEPUTER_X);
  bool key_g = (lowerKey == 'g') || (ui_event.scancode == GROOVEPUTER_G);
  bool key_f = (lowerKey == 'f') || (ui_event.scancode == GROOVEPUTER_F);
  bool key_c = (lowerKey == 'c') || (ui_event.scancode == GROOVEPUTER_C);
  bool key_v = (lowerKey == 'v') || (ui_event.scancode == GROOVEPUTER_V);
  bool key_r = (lowerKey == 'r') || (ui_event.scancode == GROOVEPUTER_R);

  if (key_s) {
    if (ui_event.alt || ui_event.ctrl) {
      ensureStepFocusAndCursor();
      if (has_selection_) {
        int min_row, max_row, min_col, max_col;
        getSelectionBounds(min_row, max_row, min_col, max_col);
        withAudioGuard([&]() {
          int vIdx = (voice_index_ < 0) ? 0 : (voice_index_ >= 2 ? 1 : voice_index_);
          SynthPattern& pattern = mini_acid_.sceneManager().editCurrentSynthPattern(vIdx);
          bool target = !pattern.steps[min_row * 8 + min_col].slide;
          for (int r = min_row; r <= max_row; ++r) {
            for (int c = min_col; c <= max_col; ++c) {
              pattern.steps[r * 8 + c].slide = target;
            }
          }
        });
      } else {
        int step = activePatternStep();
        withAudioGuard([&]() { mini_acid_.toggle303SlideStep(voice_index_, step); });
      }
    } else {
      applyToSelectionOrCursor([&](int step) {
        mini_acid_.adjust303StepOctave(voice_index_, step, 1);
      });
    }
    return true;
  }
  if (key_a) {
    if (ui_event.alt || ui_event.ctrl) {
      ensureStepFocusAndCursor();
      if (has_selection_) {
        int min_row, max_row, min_col, max_col;
        getSelectionBounds(min_row, max_row, min_col, max_col);
        withAudioGuard([&]() {
          int vIdx = (voice_index_ < 0) ? 0 : (voice_index_ >= 2 ? 1 : voice_index_);
          SynthPattern& pattern = mini_acid_.sceneManager().editCurrentSynthPattern(vIdx);
          bool target = !pattern.steps[min_row * 8 + min_col].accent;
          for (int r = min_row; r <= max_row; ++r) {
            for (int c = min_col; c <= max_col; ++c) {
              pattern.steps[r * 8 + c].accent = target;
            }
          }
        });
      } else {
        int step = activePatternStep();
        withAudioGuard([&]() { mini_acid_.toggle303AccentStep(voice_index_, step); });
      }
    } else {
      applyToSelectionOrCursor([&](int step) {
        mini_acid_.adjust303StepNote(voice_index_, step, 1);
      });
    }
    return true;
  }
  if (key_z) {
    applyToSelectionOrCursor([&](int step) {
      mini_acid_.adjust303StepNote(voice_index_, step, -1);
    });
    return true;
  }
  if (key_b && !ui_event.alt && !ui_event.ctrl) {
    if (mini_acid_.songModeEnabled()) return true;
    int nextBank = (activeBankCursor() + 1) % kBankCount;
    bank_cursor_ = nextBank;
    setBankIndex(nextBank);
    return true;
  }
  if (key_x) {
    applyToSelectionOrCursor([&](int step) {
      mini_acid_.adjust303StepOctave(voice_index_, step, -1);
    });
    return true;
  }
  if (key_g) {
    withAudioGuard([&]() { mini_acid_.randomize303Pattern(voice_index_); });
    return true;
  }
  if (key_f) {
    ensureStepFocus();
    int step = activePatternStep();
    withAudioGuard([&]() { mini_acid_.cycle303StepFx(voice_index_, step); });
    return true;
  }
  if (key_c && ui_event.ctrl) {
    ApplicationEventType type = GROOVEPUTER_APP_EVENT_COPY;
    UIEvent appEvent = ui_event;
    appEvent.event_type = GROOVEPUTER_APPLICATION_EVENT;
    appEvent.app_event_type = type;
    handleEvent(appEvent);
    return true;
  }
  if (key_v && ui_event.ctrl) {
    ApplicationEventType type = GROOVEPUTER_APP_EVENT_PASTE;
    UIEvent appEvent = ui_event;
    appEvent.event_type = GROOVEPUTER_APPLICATION_EVENT;
    appEvent.app_event_type = type;
    handleEvent(appEvent);
    return true;
  }
  if (key_r) { // R for REST
    ensureStepFocusAndCursor();
    int step = activePatternStep();
    withAudioGuard([&]() { mini_acid_.clear303Step(step, voice_index_); }); // Clean rest (clear all flags)
    return true;
  }

  // Alt + Backspace = Reset Pattern
  if (ui_event.alt && (key == '\b' || key == 0x7F)) {
    withAudioGuard([&]() { 
        for (int i=0; i<SEQ_STEPS; ++i) {
            mini_acid_.clear303Step(i, voice_index_);
        }
    });
    UI::showToast("Pattern Cleared");
    return true;
  }

  if (is_backspace && has_selection_) {
      int min_row, max_row, min_col, max_col;
      getSelectionBounds(min_row, max_row, min_col, max_col);
      withAudioGuard([&]() {
          for (int r = min_row; r <= max_row; ++r) {
              for (int c = min_col; c <= max_col; ++c) {
                  int stepIdx = r * 8 + c;
                  mini_acid_.clear303Step(stepIdx, voice_index_);
              }
          }
      });
      clearSelection();
      UI::showToast("Selection Cleared");
      return true;
  }

  if (is_backspace) { // Backspace / Del = Clear Step (REST)
    ensureStepFocusAndCursor();
    int step = activePatternStep();
    withAudioGuard([&]() { mini_acid_.clear303Step(step, voice_index_); }); // Use full clear
    return true;
  }

  return false;
}

std::unique_ptr<MultiPageHelpDialog> PatternEditPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int PatternEditPage::getHelpFrameCount() const {
  return 1;
}

void PatternEditPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPage303PatternEdit(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}



void PatternEditPage::draw(IGfx& gfx) {
  switch (UI::currentStyle) {
    case VisualStyle::RETRO_CLASSIC:
      drawRetroClassicStyle(gfx);
      break;
    case VisualStyle::AMBER:
      drawAmberStyle(gfx);
      break;
    case VisualStyle::MINIMAL:
    default:
      drawMinimalStyle(gfx);
      break;
  }
}

void PatternEditPage::drawMinimalStyle(IGfx& gfx) {
  bank_index_ = mini_acid_.current303BankIndex(voice_index_);
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  int body_y = y + 2;
  int body_h = h - 2;
  if (body_h <= 0) return;

  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide = mini_acid_.pattern303SlideSteps(voice_index_);
  int stepCursor = pattern_edit_cursor_;
  int playing = mini_acid_.currentStep();
  int selectedPattern = mini_acid_.display303PatternIndex(voice_index_);
  bool songMode = mini_acid_.songModeEnabled();
  bool patternFocus = !songMode && patternRowFocused();
  bool bankFocus = !songMode && focus_ == Focus::BankRow;
  bool stepFocus = !patternFocus && !bankFocus;
  int patternCursor = songMode && selectedPattern >= 0 ? selectedPattern : activePatternCursor();
  int bankCursor = activeBankCursor();

  PatternSelectionBarComponent::State pattern_state;
  pattern_state.pattern_count = Bank<SynthPattern>::kPatterns;
  pattern_state.selected_index = selectedPattern;
  pattern_state.cursor_index = patternCursor;
  pattern_state.show_cursor = patternFocus;
  pattern_state.song_mode = songMode;
  pattern_bar_->setState(pattern_state);
  pattern_bar_->setBoundaries(Rect{x, body_y, w, 0});
  int pattern_bar_h = pattern_bar_->barHeight(gfx);
  pattern_bar_->setBoundaries(Rect{x, body_y, w, pattern_bar_h});
  pattern_bar_->draw(gfx);

  BankSelectionBarComponent::State bank_state;
  bank_state.bank_count = kBankCount;
  bank_state.selected_index = bank_index_;
  bank_state.cursor_index = bankCursor;
  bank_state.show_cursor = bankFocus;
  bank_state.song_mode = songMode;
  bank_bar_->setState(bank_state);
  bank_bar_->setBoundaries(Rect{x, body_y - 1, w, 0});
  int bank_bar_h = bank_bar_->barHeight(gfx);
  bank_bar_->setBoundaries(Rect{x, body_y - 1, w, bank_bar_h});
  bank_bar_->draw(gfx);

  int spacing = 4;
  int grid_top = body_y + pattern_bar_h + 6;
  int cell_size = (w - spacing * 7 - 2) / 8;
  if (cell_size < 12) cell_size = 12;
  int indicator_h = 5;
  int indicator_gap = 1;
  int row_height = indicator_h + indicator_gap + cell_size + 4;

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int row = i / 8;
    int col = i % 8;
    int cell_x = x + col * (cell_size + spacing);
    int cell_y = grid_top + row * row_height;

    int indicator_w = (cell_size - 2) / 2;
    if (indicator_w < 4) indicator_w = 4;
    int slide_x = cell_x + cell_size - indicator_w;
    int indicator_y = cell_y;

    gfx.fillRect(cell_x, indicator_y, indicator_w, indicator_h, slide[i] ? COLOR_SLIDE : COLOR_GRAY_DARKER);
    gfx.drawRect(cell_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);
    gfx.fillRect(slide_x, indicator_y, indicator_w, indicator_h, accent[i] ? COLOR_ACCENT : COLOR_GRAY_DARKER);
    gfx.drawRect(slide_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);

    int note_box_y = indicator_y + indicator_h + indicator_gap;
    IGfxColor noteColor = voiceColor(voice_index_);
    IGfxColor fill = notes[i] >= 0 ? noteColor : COLOR_GRAY;
    gfx.fillRect(cell_x, note_box_y, cell_size, cell_size, fill);
    gfx.drawRect(cell_x, note_box_y, cell_size, cell_size, COLOR_WHITE);

    if (playing == i) {
      gfx.drawRect(cell_x - 1, note_box_y - 1, cell_size + 2, cell_size + 2, COLOR_STEP_HILIGHT);
    }
    if (stepFocus && stepCursor == i) {
      gfx.drawRect(cell_x - 2, note_box_y - 2, cell_size + 4, cell_size + 4, COLOR_STEP_SELECTED);
    }
    if (stepFocus && isStepSelected(i)) {
      gfx.drawRect(cell_x - 3, note_box_y - 3, cell_size + 6, cell_size + 6, COLOR_ACCENT);
    }

    char note_label[8];
    formatNoteName(notes[i], note_label, sizeof(note_label));
    int tw = textWidth(gfx, note_label);
    int tx = cell_x + (cell_size - tw) / 2;
    int ty = note_box_y + cell_size / 2 - gfx.fontHeight() / 2;
    gfx.setTextColor(notes[i] >= 0 ? COLOR_BLACK : COLOR_WHITE);
    gfx.drawText(tx, ty, note_label);
  }
  }

void PatternEditPage::drawRetroClassicStyle(IGfx& gfx) {
#ifdef USE_RETRO_THEME
  bank_index_ = mini_acid_.current303BankIndex(voice_index_);
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide = mini_acid_.pattern303SlideSteps(voice_index_);
  int stepCursor = pattern_edit_cursor_;
  int playing = mini_acid_.currentStep();
  int selectedPattern = mini_acid_.display303PatternIndex(voice_index_);
  bool songMode = mini_acid_.songModeEnabled();
  bool patternFocus = !songMode && patternRowFocused();
  bool bankFocus = !songMode && focus_ == Focus::BankRow;
  bool stepFocus = !patternFocus && !bankFocus;
  int patternCursor = songMode && selectedPattern >= 0 ? selectedPattern : activePatternCursor();
  int bankCursor = activeBankCursor();

  // 1. Header (from RetroWidgets, like GenrePage)
  char modeBuf[16];
  snprintf(modeBuf, sizeof(modeBuf), "P%d", selectedPattern + 1);
  char titleBuf[32];
  snprintf(titleBuf, sizeof(titleBuf), "%s%s", 
           voice_index_ == 0 ? "303 A" : "303 B",
           chaining_mode_ ? " [CHAIN]" : "");

  drawHeaderBar(gfx, x, y, w, 14, 
                titleBuf, 
                modeBuf, 
                mini_acid_.isPlaying(), 
                (int)(mini_acid_.bpm() + 0.5f), 
                playing);

  // 2. Background (deep black for contrast, like GenrePage)
  int contentY = y + 15;
  int contentH = h - 15 - 12;
  gfx.fillRect(x, contentY, w, contentH, IGfxColor(BG_DEEP_BLACK));

  // 3. Bank/Pattern Selectors (inline, with selective highlighting)
  gfx.setTextColor(IGfxColor(TEXT_SECONDARY));
  gfx.drawText(x + 4, contentY + 2, "BK");
  for (int i = 0; i < kBankCount; i++) {
    int slotX = x + 22 + i * 18;
    bool sel = (i == bank_index_);
    bool cur = (i == bankCursor);
    bool focused = bankFocus && cur;
    
    IGfxColor bankColor = retroVoiceColor(voice_index_);
    IGfxColor bgColor = sel ? bankColor : IGfxColor(BG_PANEL);
    gfx.fillRect(slotX, contentY + 1, 16, 10, bgColor);
    
    // Glow border only when focused
    if (focused) {
      drawGlowBorder(gfx, slotX, contentY + 1, 16, 10, bankColor, 1);
    } else if (cur) {
      gfx.drawRect(slotX, contentY + 1, 16, 10, IGfxColor(GRID_MEDIUM));
    }
    
    char c[2] = {static_cast<char>('A' + i), 0};
    gfx.setTextColor(sel ? IGfxColor(BG_DEEP_BLACK) : IGfxColor(TEXT_SECONDARY));
    gfx.drawText(slotX + 4, contentY + 2, c);
  }

  gfx.setTextColor(IGfxColor(TEXT_SECONDARY));
  gfx.drawText(x + 72, contentY + 2, "PTRN");
  for (int i = 0; i < 8; i++) {
    int slotX = x + 106 + i * 10;
    bool sel = (i == selectedPattern);
    bool cur = (i == patternCursor);
    bool focused = patternFocus && cur;
    
    IGfxColor selColor = retroVoiceColor(voice_index_);
    IGfxColor bgColor = sel ? selColor : IGfxColor(BG_PANEL);
    gfx.fillRect(slotX, contentY + 1, 9, 10, bgColor);
    
    if (focused) {
      drawGlowBorder(gfx, slotX, contentY + 1, 9, 10, selColor, 1);
    } else if (cur) {
      gfx.drawRect(slotX, contentY + 1, 9, 10, IGfxColor(GRID_MEDIUM));
    }
    
    char c1[2] = {static_cast<char>('1' + i), 0};
    gfx.setTextColor(sel ? IGfxColor(BG_DEEP_BLACK) : IGfxColor(TEXT_SECONDARY));
    gfx.drawText(slotX + 2, contentY + 2, c1);
  }

  // 4. Step Grid (Direct Scene Access - No Cache Lag)
  int gridY = contentY + 16;
  int spacing = 2;
  int cellW = (w - 10 - spacing * 7) / 8;
  int cellH = (contentH - 20 - spacing) / 2;

  // READ DIRECTLY from source of truth
  int patIdx = activePatternCursor();
  const SynthPattern& pattern = mini_acid_.sceneManager().getSynthPattern(voice_index_, patIdx);

  // Check if we are viewing the currently playing pattern
  bool isPlayingPattern = false;
  if (mini_acid_.isPlaying()) {
     int playingIdx = mini_acid_.current303PatternIndex(voice_index_); 
     // Note: current303PatternIndex returns what the engine is playing
     if (playingIdx == patIdx) isPlayingPattern = true;
  }

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int row = i / 8;
    int col = i % 8;
    int cellX = x + 5 + col * (cellW + spacing);
    int cellRowY = gridY + row * (cellH + spacing);

    bool isCurrent = (isPlayingPattern && playing == i); // Only show playhead if we are looking at the playing pattern
    bool isCursor = (stepFocus && stepCursor == i);
    bool isSelected = stepFocus && isStepSelected(i);
    
    int8_t note = pattern.steps[i].note;
    bool acc = pattern.steps[i].accent;
    bool sld = pattern.steps[i].slide;
    bool hasNote = (note >= 0);

    // Background (darker on beat markers for subtle rhythm guide)
    IGfxColor bgColor = (col % 4 == 0) ? IGfxColor(BG_INSET) : IGfxColor(BG_PANEL);
    gfx.fillRect(cellX, cellRowY, cellW, cellH, bgColor);

    // Border: glow on cursor, simple otherwise
    // Border: glow on cursor, simple otherwise
    if (isSelected) {
      drawGlowBorder(gfx, cellX, cellRowY, cellW, cellH, IGfxColor(NEON_ORANGE), 1);
    }
    if (isCursor) {
      // Use Voice Color for cursor to indicate which voice is being edited
      IGfxColor cursorColor = retroVoiceColor(voice_index_);
      drawGlowBorder(gfx, cellX, cellRowY, cellW, cellH, cursorColor, 1);
    } else if (!isSelected) {
      gfx.drawRect(cellX, cellRowY, cellW, cellH, IGfxColor(GRID_MEDIUM));
    }

    // Playing indicator: voice color glow (prominence)
    if (isCurrent) {
      IGfxColor playColor = retroVoiceColor(voice_index_);
      drawGlowBorder(gfx, cellX, cellRowY, cellW, cellH, playColor, 2);
    }

    // Note content
    if (hasNote) {
      char note_label[8];
      formatNoteName(note, note_label, sizeof(note_label));
      
      // "Teal & Orange" Harmony: Cleaner, distinct, professional
      // Voice Color = Normal, Orange = Accent
      IGfxColor baseColor = retroVoiceColor(voice_index_);
      IGfxColor noteColor = acc ? IGfxColor(NEON_ORANGE) : baseColor;
      
      int tw = textWidth(gfx, note_label);
      int tx = cellX + (cellW - tw) / 2;
      int ty = cellRowY + 3;
      
      // Glow text only when focused for emphasis
      if (isCursor) {
        drawGlowText(gfx, tx, ty, note_label, noteColor, IGfxColor(TEXT_PRIMARY));
      } else {
        gfx.setTextColor(noteColor);
        gfx.drawText(tx, ty, note_label);
      }
    } else {
      gfx.setTextColor(IGfxColor(TEXT_DIM));
      // Use a subtle dot for "no note" steps
      gfx.drawText(cellX + cellW/2 - 2, cellRowY + 3, ".");
    }

    // Indicators (Persistent dots below the note)
    int dotY = cellRowY + cellH - 4;
    // Slide LED (Purple or Magenta for better pop)
    RetroWidgets::drawLED(gfx, cellX + 4, dotY, 1, sld, IGfxColor(NEON_MAGENTA));
    // Accent LED (Matches Note Accent Color -> Orange)
    RetroWidgets::drawLED(gfx, cellX + cellW - 4, dotY, 1, acc, IGfxColor(NEON_ORANGE));

    // FX Indicator
    uint8_t fx = pattern.steps[i].fx;
    if (fx != 0) {
        gfx.setTextColor(IGfxColor(NEON_YELLOW));
        if (fx == (uint8_t)StepFx::Retrig) {
            char buf[8]; snprintf(buf, sizeof(buf), "R%d", pattern.steps[i].fxParam);
            gfx.drawText(cellX + cellW/2 - textWidth(gfx,buf)/2, dotY - 8, buf);
        } else if (fx == (uint8_t)StepFx::Reverse) {
            gfx.drawText(cellX + cellW/2 - textWidth(gfx,"RV")/2, dotY - 8, "RV");
        }
    }
  }

  // Scanlines disabled: caused flicker on small TFT

  // 5. Footer (consistent with header)
  const char* focusLabel = stepFocus ? "STEPS" : (bankFocus ? "BANK" : "PTRN");
  drawFooterBar(gfx, x, y + h - 12, w, 12, 
                "A/Z:Nt F:FX Alt+Arw:Prm", 
                "q..i:Ptrn B:Bank TAB:Vce", 
                focusLabel);

  // NO scanlines - clean and readable
#else
  drawMinimalStyle(gfx);
#endif
}

void PatternEditPage::drawAmberStyle(IGfx& gfx) {
#ifdef USE_AMBER_THEME
  bank_index_ = mini_acid_.current303BankIndex(voice_index_);
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide = mini_acid_.pattern303SlideSteps(voice_index_);
  int stepCursor = pattern_edit_cursor_;
  int playing = mini_acid_.currentStep();
  int selectedPattern = mini_acid_.display303PatternIndex(voice_index_);
  bool songMode = mini_acid_.songModeEnabled();
  bool patternFocus = !songMode && patternRowFocused();
  bool bankFocus = !songMode && focus_ == Focus::BankRow;
  bool stepFocus = !patternFocus && !bankFocus;
  int patternCursor = songMode && selectedPattern >= 0 ? selectedPattern : activePatternCursor();
  int bankCursor = activeBankCursor();

  char modeBuf[16];
  snprintf(modeBuf, sizeof(modeBuf), "P%d", selectedPattern + 1);
  char titleBuf[32];
  snprintf(titleBuf, sizeof(titleBuf), "%s%s", 
           voice_index_ == 0 ? "303 A" : "303 B",
           chaining_mode_ ? " [CHAIN]" : "");

  AmberWidgets::drawHeaderBar(
      gfx, x, y, w, 14,
      titleBuf,
      modeBuf,
      mini_acid_.isPlaying(),
      (int)(mini_acid_.bpm() + 0.5f),
      playing);

  int contentY = y + 15;
  int contentH = h - 15 - 12;
  gfx.fillRect(x, contentY, w, contentH, IGfxColor(AmberTheme::BG_DEEP_BLACK));

  gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
  gfx.drawText(x + 4, contentY + 2, "BK");
  for (int i = 0; i < kBankCount; i++) {
    int slotX = x + 22 + i * 18;
    bool sel = (i == bank_index_);
    bool cur = (i == bankCursor);
    bool focused = bankFocus && cur;
    
    IGfxColor bankColor = amberVoiceColor(voice_index_);
    IGfxColor bgColor = sel ? bankColor : IGfxColor(AmberTheme::BG_PANEL);
    gfx.fillRect(slotX, contentY + 1, 16, 10, bgColor);
    
    if (focused) {
      AmberWidgets::drawGlowBorder(gfx, slotX, contentY + 1, 16, 10, bankColor, 1);
    } else if (cur) {
      gfx.drawRect(slotX, contentY + 1, 16, 10, IGfxColor(AmberTheme::GRID_MEDIUM));
    }
    
    char c[2] = {static_cast<char>('A' + i), 0};
    gfx.setTextColor(sel ? IGfxColor(AmberTheme::BG_DEEP_BLACK) : IGfxColor(AmberTheme::TEXT_SECONDARY));
    gfx.drawText(slotX + 4, contentY + 2, c);
  }

  gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
  gfx.drawText(x + 72, contentY + 2, "PTRN");
  for (int i = 0; i < 8; i++) {
    int slotX = x + 106 + i * 10;
    bool sel = (i == selectedPattern);
    bool cur = (i == patternCursor);
    bool focused = patternFocus && cur;
    
    IGfxColor selColor = amberVoiceColor(voice_index_);
    IGfxColor bgColor = sel ? selColor : IGfxColor(AmberTheme::BG_PANEL);
    gfx.fillRect(slotX, contentY + 1, 9, 10, bgColor);
    
    if (focused) {
      AmberWidgets::drawGlowBorder(gfx, slotX, contentY + 1, 9, 10, selColor, 1);
    } else if (cur) {
      gfx.drawRect(slotX, contentY + 1, 9, 10, IGfxColor(AmberTheme::GRID_MEDIUM));
    }
    
    char c1[2] = {static_cast<char>('1' + i), 0};
    gfx.setTextColor(sel ? IGfxColor(AmberTheme::BG_DEEP_BLACK) : IGfxColor(AmberTheme::TEXT_SECONDARY));
    gfx.drawText(slotX + 2, contentY + 2, c1);
  }

  int gridY = contentY + 16;
  int spacing = 2;
  int cellW = (w - 10 - spacing * 7) / 8;
  int cellH = (contentH - 20 - spacing) / 2;

  int patIdx = activePatternCursor();
  const SynthPattern& pattern = mini_acid_.sceneManager().getSynthPattern(voice_index_, patIdx);

  bool isPlayingPattern = false;
  if (mini_acid_.isPlaying()) {
     int playingIdx = mini_acid_.current303PatternIndex(voice_index_); 
     if (playingIdx == patIdx) isPlayingPattern = true;
  }

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int row = i / 8;
    int col = i % 8;
    int cellX = x + 5 + col * (cellW + spacing);
    int cellRowY = gridY + row * (cellH + spacing);

    bool isCurrent = (isPlayingPattern && playing == i);
    bool isCursor = (stepFocus && stepCursor == i);
    bool isSelected = stepFocus && isStepSelected(i);
    
    int8_t note = pattern.steps[i].note;
    bool acc = pattern.steps[i].accent;
    bool sld = pattern.steps[i].slide;
    bool hasNote = (note >= 0);

    IGfxColor bgColor = (col % 4 == 0) ? IGfxColor(AmberTheme::BG_INSET) : IGfxColor(AmberTheme::BG_PANEL);
    gfx.fillRect(cellX, cellRowY, cellW, cellH, bgColor);

    if (isSelected) {
      AmberWidgets::drawGlowBorder(gfx, cellX, cellRowY, cellW, cellH, IGfxColor(AmberTheme::NEON_ORANGE), 1);
    }
    if (isCursor) {
      AmberWidgets::drawGlowBorder(gfx, cellX, cellRowY, cellW, cellH, IGfxColor(AmberTheme::SELECT_BRIGHT), 1);
    } else if (!isSelected) {
      gfx.drawRect(cellX, cellRowY, cellW, cellH, IGfxColor(AmberTheme::GRID_MEDIUM));
    }

    if (isCurrent) {
      AmberWidgets::drawGlowBorder(gfx, cellX, cellRowY, cellW, cellH, IGfxColor(AmberTheme::STATUS_PLAYING), 2);
    }

    if (hasNote) {
      char note_label[8];
      formatNoteName(note, note_label, sizeof(note_label));
      
      IGfxColor noteColor = acc ? IGfxColor(AmberTheme::NEON_ORANGE) : amberVoiceColor(voice_index_);
      
      int tw = textWidth(gfx, note_label);
      int tx = cellX + (cellW - tw) / 2;
      int ty = cellRowY + 3;
      
      if (isCursor) {
        AmberWidgets::drawGlowText(gfx, tx, ty, note_label, IGfxColor(AmberTheme::FOCUS_GLOW), noteColor);
      } else {
        gfx.setTextColor(noteColor);
        gfx.drawText(tx, ty, note_label);
      }
    } else {
      gfx.setTextColor(IGfxColor(AmberTheme::TEXT_DIM));
      gfx.drawText(cellX + cellW/2 - 2, cellRowY + 3, ".");
    }

    int dotY = cellRowY + cellH - 4;
    AmberWidgets::drawLED(gfx, cellX + 4, dotY, 1, sld, IGfxColor(AmberTheme::NEON_MAGENTA));
    AmberWidgets::drawLED(gfx, cellX + cellW - 4, dotY, 1, acc, IGfxColor(AmberTheme::NEON_ORANGE));
  }

  // Scanlines disabled: caused flicker on small TFT

  const char* focusLabel = stepFocus ? "STEPS" : (bankFocus ? "BANK" : "PTRN");
  AmberWidgets::drawFooterBar(
      gfx, x, y + h - 12, w, 12,
      "A/Z:Note  Alt+S/A:Slide/Acc  G:Rand",
      "q..i:Ptrn  B:Bank  TAB:Voice",
      focusLabel);
#else
  drawMinimalStyle(gfx);
#endif
}
