#include "pattern_edit_page.h"

#include <cctype>
#include <utility>

#include "../ui_input.h"
#include "../layout_manager.h"
#include "../help_dialog_frames.h"
#include "../components/bank_selection_bar.h"
#include "../components/pattern_selection_bar.h"

namespace {
struct PatternClipboard {
  bool has_pattern = false;
  SynthPattern pattern{};
};

PatternClipboard g_pattern_clipboard;
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
  bank_bar_ = std::make_shared<BankSelectionBarComponent>("BANK", "ABCD");
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
  switch (std::tolower(static_cast<unsigned char>(key))) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
  //  case 'i': return 7;
    default: return -1;
  }
}

int PatternEditPage::bankIndexFromKey(char key) const {
  switch (key) {
    case '1': return 0;
    case '2': return 1;
    case '3': return 2;
    case '4': return 3;
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

const std::string & PatternEditPage::getTitle() const {
  return title_;
}

bool PatternEditPage::handleEvent(UIEvent& ui_event) {
  // CRITICAL: Block numeric quick-select BEFORE forwarding to components
  // Otherwise BankSelectionBar/PatternSelectionBar intercept digits before we can block them
  if (ui_event.event_type == MINIACID_KEY_DOWN) {
    if (!ui_event.shift && !ui_event.ctrl && !ui_event.meta &&
        ui_event.key >= '0' && ui_event.key <= '9') {
      return true; // consume, do nothing
    }
  }

  if (pattern_bar_ && pattern_bar_->handleEvent(ui_event)) return true;
  if (bank_bar_ && bank_bar_->handleEvent(ui_event)) return true;
  if (ui_event.event_type == MINIACID_APPLICATION_EVENT) {
    switch (ui_event.app_event_type) {
      case MINIACID_APP_EVENT_COPY: {
        const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
        const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
        const bool* slide = mini_acid_.pattern303SlideSteps(voice_index_);
        for (int i = 0; i < SEQ_STEPS; ++i) {
          g_pattern_clipboard.pattern.steps[i].note = notes[i];
          g_pattern_clipboard.pattern.steps[i].accent = accent[i];
          g_pattern_clipboard.pattern.steps[i].slide = slide[i];
        }
        g_pattern_clipboard.has_pattern = true;
        return true;
      }
      case MINIACID_APP_EVENT_PASTE: {
        if (!g_pattern_clipboard.has_pattern) return false;
        int current_notes[SEQ_STEPS];
        bool current_accent[SEQ_STEPS];
        bool current_slide[SEQ_STEPS];
        const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
        const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
        const bool* slide = mini_acid_.pattern303SlideSteps(voice_index_);
        for (int i = 0; i < SEQ_STEPS; ++i) {
          current_notes[i] = notes[i];
          current_accent[i] = accent[i];
          current_slide[i] = slide[i];
        }
        const SynthPattern& src = g_pattern_clipboard.pattern;
        withAudioGuard([&]() {
          for (int i = 0; i < SEQ_STEPS; ++i) {
            int target_note = src.steps[i].note;
            int current_note = current_notes[i];
            if (target_note < 0) {
              if (current_note >= 0) {
                mini_acid_.clear303StepNote(voice_index_, i);
              }
            } else if (current_note < 0) {
              int delta = target_note - MiniAcid::kMin303Note;
              if (delta == 0) {
                mini_acid_.adjust303StepNote(voice_index_, i, 1);
                mini_acid_.adjust303StepNote(voice_index_, i, -1);
              } else {
                mini_acid_.adjust303StepNote(voice_index_, i, delta);
              }
            } else {
              int delta = target_note - current_note;
              if (delta != 0) {
                mini_acid_.adjust303StepNote(voice_index_, i, delta);
              }
            }

            if (current_accent[i] != src.steps[i].accent) {
              mini_acid_.toggle303AccentStep(voice_index_, i);
            }
            if (current_slide[i] != src.steps[i].slide) {
              mini_acid_.toggle303SlideStep(voice_index_, i);
            }
          }
        });
        return true;
      }
      default:
        return false;
    }
  }
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;

  // Let parent handle global navigation keys; do not steal them here.
  if (UIInput::isGlobalNav(ui_event)) return false;

  bool handled = false;
  
  // Arrow-first: Cardputer may deliver arrows in scancode OR key.
  // Keep vim-keys only as silent fallback (not in footer hints).
  int nav = UIInput::navCode(ui_event);
  switch (nav) {
    case MINIACID_LEFT:
      movePatternCursor(-1);
      handled = true;
      break;
    case MINIACID_RIGHT:
      movePatternCursor(1);
      handled = true;
      break;
    case MINIACID_UP:
      movePatternCursorVertical(-1);
      handled = true;
      break;
    case MINIACID_DOWN:
      movePatternCursorVertical(1);
      handled = true;
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
      
      // showToast(voice_index_ == 0 ? "Voice A (Bass)" : "Voice B (Lead)", 1000);
      return true;
  }

  char key = ui_event.key;
  if (!key) return false;

  /*
  int bankIdx = bankIndexFromKey(key);
  if (bankIdx >= 0) {
    setBankIndex(bankIdx);
    if (!mini_acid_.songModeEnabled()) focus_ = Focus::BankRow;
    return true;
  }
    */

  if (key == '\n' || key == '\r') {
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

  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
  
  // Q-I Pattern Selection (Standardized)
  if (!ui_event.shift && !ui_event.ctrl && !ui_event.meta) {
    int patternIdx = patternIndexFromKey(lowerKey);
    if (patternIdx >= 0) {
      if (mini_acid_.songModeEnabled()) return true;
      focusPatternRow();
      setPatternCursor(patternIdx);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, patternIdx); });
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

  switch (lowerKey) {
    case 'q': {
      if (ui_event.shift) {
        ensureStepFocusAndCursor();
        int step = activePatternStep();
        withAudioGuard([&]() { mini_acid_.toggle303SlideStep(voice_index_, step); });
        return true;
      }
      break;
    }
    case 'w': {
      if (ui_event.shift) {
        ensureStepFocusAndCursor();
        int step = activePatternStep();
        withAudioGuard([&]() { mini_acid_.toggle303AccentStep(voice_index_, step); });
        return true;
      }
      break;
    }
    case 'a': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepNote(voice_index_, step, 1); });
      return true;
    }
    case 'z': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepNote(voice_index_, step, -1); });
      return true;
    }
    case 's': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepOctave(voice_index_, step, 1); });
      return true;
    }
    case 'x': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepOctave(voice_index_, step, -1); });
      return true;
    }
    case 'g': {
      withAudioGuard([&]() { mini_acid_.randomize303Pattern(voice_index_); });
      return true;
    }
    default:
      break;
  }

  if (key == '\b') {
    ensureStepFocusAndCursor();
    int step = activePatternStep();
    withAudioGuard([&]() { mini_acid_.clear303StepNote(voice_index_, step); });
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
    IGfxColor fill = notes[i] >= 0 ? COLOR_303_NOTE : COLOR_GRAY;
    gfx.fillRect(cell_x, note_box_y, cell_size, cell_size, fill);
    gfx.drawRect(cell_x, note_box_y, cell_size, cell_size, COLOR_WHITE);

    if (playing == i) {
      gfx.drawRect(cell_x - 1, note_box_y - 1, cell_size + 2, cell_size + 2, COLOR_STEP_HILIGHT);
    }
    if (stepFocus && stepCursor == i) {
      gfx.drawRect(cell_x - 2, note_box_y - 2, cell_size + 4, cell_size + 4, COLOR_STEP_SELECTED);
    }

    char note_label[8];
    formatNoteName(notes[i], note_label, sizeof(note_label));
    int tw = textWidth(gfx, note_label);
    int tx = cell_x + (cell_size - tw) / 2;
    int ty = note_box_y + cell_size / 2 - gfx.fontHeight() / 2;
    gfx.drawText(tx, ty, note_label);
  }

  // v1.1 Pro Footer
  LayoutManager::drawFooter(gfx, "[ARROWS]NAV [ENT]SEL [TAB]VOICE", "[-/+]NOTE [G]RANDOM [SHIFT+W]ACC");
}
