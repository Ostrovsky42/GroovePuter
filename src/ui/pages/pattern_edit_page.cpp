#include "pattern_edit_page.h"

#include <cctype>
#include <utility>

#include "../help_dialog_frames.h"
#include "../components/automation_lane_editor.h"
#include "../components/bank_selection_bar.h"
#include "../components/combo_box.h"
#include "../components/label_component.h"
#include "../components/pattern_selection_bar.h"
#include "../ui_colors.h"
#include "pattern_automation_page.h"

namespace {
struct PatternClipboard {
  bool has_pattern = false;
  SynthPattern pattern{};
};

PatternClipboard g_pattern_clipboard;

struct PatternStepGridLayout {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int spacing = 4;
  int cell_size = 0;
  int indicator_h = 5;
  int indicator_gap = 1;
  int row_height = 0;
};

bool computePatternStepGridLayout(const Rect& bounds, PatternStepGridLayout& layout) {
  layout.x = bounds.x;
  layout.y = bounds.y;
  layout.w = bounds.w;
  if (layout.w <= 0) return false;
  layout.cell_size = (layout.w - layout.spacing * 7 - 2) / 8;
  if (layout.cell_size < 12) layout.cell_size = 12;
  layout.row_height = layout.indicator_h + layout.indicator_gap + layout.cell_size + 4;
  layout.h = layout.row_height * 2;
  return true;
}

class PatternStepGridComponent : public FocusableComponent {
 public:
  struct Callbacks {
    std::function<void(int step)> onSelect;
  };

  explicit PatternStepGridComponent(Callbacks callbacks)
      : callbacks_(std::move(callbacks)) {}

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type != MINIACID_MOUSE_DOWN) return false;
    if (ui_event.button != MOUSE_BUTTON_LEFT) return false;
    if (!contains(ui_event.x, ui_event.y)) return false;

    PatternStepGridLayout layout{};
    if (!computePatternStepGridLayout(getBoundaries(), layout)) return false;

    int col = (ui_event.x - layout.x) / (layout.cell_size + layout.spacing);
    int row = (ui_event.y - layout.y) / layout.row_height;
    if (col < 0 || col >= 8 || row < 0 || row >= 2) return false;

    int cell_x = layout.x + col * (layout.cell_size + layout.spacing);
    if (ui_event.x >= cell_x + layout.cell_size) return false;

    int step = row * 8 + col;
    if (callbacks_.onSelect) callbacks_.onSelect(step);
    return true;
  }

  void draw(IGfx& gfx) override { (void)gfx; }

 private:
  Callbacks callbacks_;
};

class NotesPatternEditPage : public IPage {
 public:
  NotesPatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override { return title_; }

 private:
  int clampCursor(int cursorIndex) const;
  int activeBankCursor() const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  void setBankIndex(int bankIndex);
  void withAudioGuard(const std::function<void()>& fn);

  int activePatternCursor() const;
  int activePatternStep() const;
  void setPatternCursor(int cursorIndex);
  void focusChild(Component* target);
  void focusStepGridFromPatternRow();
  bool patternRowFocused() const;
  bool bankRowFocused() const;
  void movePatternCursor(int delta);
  void movePatternCursorVertical(int delta);
  bool setStepNoteFromRemembered(int step);
  void captureRememberedNote(int step);
  void transposePattern(int semitoneDelta);
  void rotatePattern(int stepDelta);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  int voice_index_;
  int pattern_edit_cursor_;
  int pattern_row_cursor_;
  int bank_index_;
  int bank_cursor_;
  int last_note_;
  std::string title_;
  std::shared_ptr<LabelComponent> pattern_label_;
  std::shared_ptr<PatternSelectionBarComponent> pattern_bar_;
  std::shared_ptr<BankSelectionBarComponent> bank_bar_;
  std::shared_ptr<PatternStepGridComponent> step_grid_;
};
} // namespace

NotesPatternEditPage::NotesPatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    audio_guard_(audio_guard),
    voice_index_(voice_index),
    pattern_edit_cursor_(0),
    pattern_row_cursor_(0),
    bank_index_(0),
    bank_cursor_(0),
    last_note_(-1) {
  int idx = mini_acid_.current303PatternIndex(voice_index_);
  if (idx < 0 || idx >= Bank<SynthPattern>::kPatterns) idx = 0;
  pattern_row_cursor_ = idx;
  bank_index_ = mini_acid_.current303BankIndex(voice_index_);
  bank_cursor_ = bank_index_;
  title_ = voice_index_ == 0 ? "303A PATTERNS" : "303B PATTERNS";
  pattern_label_ = std::make_shared<LabelComponent>("PATTERNS");
  pattern_label_->setTextColor(COLOR_LABEL);
  pattern_bar_ = std::make_shared<PatternSelectionBarComponent>("PATTERNS");
  bank_bar_ = std::make_shared<BankSelectionBarComponent>("BANK", "ABCD");
  PatternSelectionBarComponent::Callbacks pattern_callbacks;
  pattern_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    setPatternCursor(index);
    withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, index); });
  };
  pattern_callbacks.onCursorMove = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    setPatternCursor(index);
  };
  pattern_bar_->setCallbacks(std::move(pattern_callbacks));
  BankSelectionBarComponent::Callbacks bank_callbacks;
  bank_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    bank_cursor_ = index;
    setBankIndex(index);
  };
  bank_callbacks.onCursorMove = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    bank_cursor_ = index;
  };
  bank_bar_->setCallbacks(std::move(bank_callbacks));
  PatternStepGridComponent::Callbacks grid_callbacks;
  grid_callbacks.onSelect = [this](int step) {
    pattern_edit_cursor_ = step;
  };
  step_grid_ = std::shared_ptr<PatternStepGridComponent>(
      new PatternStepGridComponent(std::move(grid_callbacks)));
  bank_bar_->setFocusable(true);
  addChild(bank_bar_);
  pattern_bar_->setFocusable(true);
  addChild(pattern_bar_);
  step_grid_->setFocusable(true);
  addChild(step_grid_);
  focusChild(step_grid_.get());
}

int NotesPatternEditPage::clampCursor(int cursorIndex) const {
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<SynthPattern>::kPatterns)
    cursor = Bank<SynthPattern>::kPatterns - 1;
  return cursor;
}

int NotesPatternEditPage::activeBankCursor() const {
  int cursor = bank_cursor_;
  if (cursor < 0) cursor = 0;
  if (cursor >= kBankCount) cursor = kBankCount - 1;
  return cursor;
}

int NotesPatternEditPage::patternIndexFromKey(char key) const {
  switch (std::tolower(static_cast<unsigned char>(key))) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
    case 'i': return 7;
    default: return -1;
  }
}

int NotesPatternEditPage::bankIndexFromKey(char key) const {
  switch (key) {
    case '1': return 0;
    case '2': return 1;
    case '3': return 2;
    case '4': return 3;
    default: return -1;
  }
}

void NotesPatternEditPage::setBankIndex(int bankIndex) {
  if (bankIndex < 0) bankIndex = 0;
  if (bankIndex >= kBankCount) bankIndex = kBankCount - 1;
  if (bank_index_ == bankIndex) return;
  bank_index_ = bankIndex;
  withAudioGuard([&]() { mini_acid_.set303BankIndex(voice_index_, bank_index_); });
}

void NotesPatternEditPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

int NotesPatternEditPage::activePatternCursor() const {
  return clampCursor(pattern_row_cursor_);
}

int NotesPatternEditPage::activePatternStep() const {
  int idx = pattern_edit_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}

void NotesPatternEditPage::setPatternCursor(int cursorIndex) {
  pattern_row_cursor_ = clampCursor(cursorIndex);
}

void NotesPatternEditPage::focusChild(Component* target) {
  if (!target) return;
  const auto& children = getChildren();
  bool hasTarget = false;
  for (const auto& child : children) {
    if (child.get() == target) {
      hasTarget = true;
      break;
    }
  }
  if (!hasTarget) return;
  int count = static_cast<int>(children.size());
  for (int i = 0; i < count && focusedChild() != target; ++i) {
    focusNext();
  }
}

void NotesPatternEditPage::focusStepGridFromPatternRow() {
  int row = pattern_edit_cursor_ / 8;
  if (row < 0 || row > 1) row = 0;
  pattern_edit_cursor_ = row * 8 + activePatternCursor();
  focusChild(step_grid_.get());
}

bool NotesPatternEditPage::patternRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return pattern_bar_ && pattern_bar_->isFocused();
}

bool NotesPatternEditPage::bankRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return bank_bar_ && bank_bar_->isFocused();
}

void NotesPatternEditPage::movePatternCursor(int delta) {
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  col = (col + delta) % 8;
  if (col < 0) col += 8;
  pattern_edit_cursor_ = row * 8 + col;
}

void NotesPatternEditPage::movePatternCursorVertical(int delta) {
  if (delta == 0) return;
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  int newRow = row + delta;
  if (newRow < 0) {
    if (mini_acid_.songModeEnabled()) newRow = 0;
    else {
      setPatternCursor(col);
      focusChild(pattern_bar_.get());
      return;
    }
  }
  if (newRow > 1) {
    if (mini_acid_.songModeEnabled()) newRow = 1;
    else {
      setPatternCursor(col);
      focusChild(pattern_bar_.get());
      return;
    }
  }
  pattern_edit_cursor_ = newRow * 8 + col;
}

bool NotesPatternEditPage::setStepNoteFromRemembered(int step) {
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  if (notes[step] >= 0) return false;
  if (last_note_ < MiniAcid::kMin303Note) return false;
  withAudioGuard([&]() {
    int delta = last_note_ - MiniAcid::kMin303Note;
    if (delta == 0) {
      mini_acid_.adjust303StepNote(voice_index_, step, 1);
      mini_acid_.adjust303StepNote(voice_index_, step, -1);
    } else {
      mini_acid_.adjust303StepNote(voice_index_, step, delta);
    }
  });
  return true;
}

void NotesPatternEditPage::captureRememberedNote(int step) {
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  int note = notes[step];
  if (note >= MiniAcid::kMin303Note) {
    last_note_ = note;
  }
}

void NotesPatternEditPage::transposePattern(int semitoneDelta) {
  if (semitoneDelta == 0) return;
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  withAudioGuard([&]() {
    for (int i = 0; i < SEQ_STEPS; ++i) {
      if (notes[i] >= 0) {
        mini_acid_.adjust303StepNote(voice_index_, i, semitoneDelta);
      }
    }
  });
}

void NotesPatternEditPage::rotatePattern(int stepDelta) {
  if (stepDelta == 0) return;
  int delta = stepDelta % SEQ_STEPS;
  if (delta < 0) delta += SEQ_STEPS;
  int current_notes[SEQ_STEPS];
  bool current_accent[SEQ_STEPS];
  bool current_slide[SEQ_STEPS];
  int target_notes[SEQ_STEPS];
  bool target_accent[SEQ_STEPS];
  bool target_slide[SEQ_STEPS];
  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide = mini_acid_.pattern303SlideSteps(voice_index_);
  for (int i = 0; i < SEQ_STEPS; ++i) {
    current_notes[i] = notes[i];
    current_accent[i] = accent[i];
    current_slide[i] = slide[i];
    int target = (i + delta) % SEQ_STEPS;
    target_notes[target] = notes[i];
    target_accent[target] = accent[i];
    target_slide[target] = slide[i];
  }
  withAudioGuard([&]() {
    for (int i = 0; i < SEQ_STEPS; ++i) {
      int next_note = target_notes[i];
      int cur_note = current_notes[i];
      if (next_note < 0) {
        if (cur_note >= 0) {
          mini_acid_.clear303StepNote(voice_index_, i);
        }
      } else if (cur_note < 0) {
        int delta_note = next_note - MiniAcid::kMin303Note;
        if (delta_note == 0) {
          mini_acid_.adjust303StepNote(voice_index_, i, 1);
          mini_acid_.adjust303StepNote(voice_index_, i, -1);
        } else {
          mini_acid_.adjust303StepNote(voice_index_, i, delta_note);
        }
      } else {
        int delta_note = next_note - cur_note;
        if (delta_note != 0) {
          mini_acid_.adjust303StepNote(voice_index_, i, delta_note);
        }
      }

      if (current_accent[i] != target_accent[i]) {
        mini_acid_.toggle303AccentStep(voice_index_, i);
      }
      if (current_slide[i] != target_slide[i]) {
        mini_acid_.toggle303SlideStep(voice_index_, i);
      }
    }
  });
}

bool NotesPatternEditPage::handleEvent(UIEvent& ui_event) {
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
        mini_acid_.copy303AutomationToPattern(g_pattern_clipboard.pattern, voice_index_);
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
          mini_acid_.paste303AutomationFromPattern(src, voice_index_);
        });
        return true;
      }
      default:
        return false;
    }
  }
  if (ui_event.event_type == MINIACID_KEY_DOWN) {
    if (ui_event.scancode == MINIACID_DOWN || ui_event.scancode == MINIACID_UP) {
      Component* focused = focusedChild();
      bool onPatternBar = focused == pattern_bar_.get();
      bool onBankBar = focused == bank_bar_.get();
      if (onPatternBar || onBankBar) {
        bool handledByChild = focused->handleEvent(ui_event);
        if (handledByChild) return true;
        if (!handledByChild) {
          if (ui_event.scancode == MINIACID_DOWN && onPatternBar) {
            int col = activePatternCursor();
            pattern_edit_cursor_ = col;
          }
          if (ui_event.scancode == MINIACID_DOWN) {
            focusNext();
            return true;
          }
          if (ui_event.scancode == MINIACID_UP) {
            focusPrev();
            return true;
          }
        }
      }
    }
    if (focusedChild() == step_grid_.get()) {
      switch (ui_event.scancode) {
        case MINIACID_LEFT:
          movePatternCursor(-1);
          return true;
        case MINIACID_RIGHT:
          movePatternCursor(1);
          return true;
        case MINIACID_UP:
          movePatternCursorVertical(-1);
          return true;
        case MINIACID_DOWN:
          movePatternCursorVertical(1);
          return true;
        default:
          break;
      }
    }
  }

  if (Container::handleEvent(ui_event)) return true;

  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;

  char key = ui_event.key;
  if (!key) return false;

  if (ui_event.alt) {
    char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    if (lowerKey == 'd') {
      transposePattern(1);
      return true;
    }
    if (lowerKey == 'c') {
      transposePattern(-1);
      return true;
    }
    if (lowerKey == 'f') {
      rotatePattern(1);
      return true;
    }
    if (lowerKey == 'v') {
      rotatePattern(-1);
      return true;
    }
    
  }

  /*
  int bankIdx = bankIndexFromKey(key);
  if (bankIdx >= 0) {
    setBankIndex(bankIdx);
    if (!mini_acid_.songModeEnabled()) focusChild(bank_bar_.get());
    return true;
  }
    */

  int patternIdx = patternIndexFromKey(key);
  bool patternKeyReserved = false;
  if (patternIdx >= 0) {
    char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    patternKeyReserved = (lowerKey == 'q' || lowerKey == 'w');
    if (!patternKeyReserved || patternRowFocused()) {
      if (mini_acid_.songModeEnabled()) return true;
      focusChild(pattern_bar_.get());
      setPatternCursor(patternIdx);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, patternIdx); });
      return true;
    }
  }

  auto ensureStepFocusAndCursor = [&]() {
    if (patternRowFocused()) {
      focusStepGridFromPatternRow();
    } else if (bankRowFocused()) {
      focusChild(step_grid_.get());
    }
  };

  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
  switch (lowerKey) {
    case 'q': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.toggle303SlideStep(voice_index_, step); });
      return true;
    }
    case 'w': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.toggle303AccentStep(voice_index_, step); });
      return true;
    }
    case 'a': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      if (!setStepNoteFromRemembered(step)) {
        withAudioGuard([&]() { mini_acid_.adjust303StepNote(voice_index_, step, 1); });
      }
      captureRememberedNote(step);
      return true;
    }
    case 'z': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      if (!setStepNoteFromRemembered(step)) {
        withAudioGuard([&]() { mini_acid_.adjust303StepNote(voice_index_, step, -1); });
      }
      captureRememberedNote(step);
      return true;
    }
    case 's': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      if (!setStepNoteFromRemembered(step)) {
        withAudioGuard([&]() { mini_acid_.adjust303StepOctave(voice_index_, step, 1); });
      }
      captureRememberedNote(step);
      return true;
    }
    case 'x': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      if (!setStepNoteFromRemembered(step)) {
        withAudioGuard([&]() { mini_acid_.adjust303StepOctave(voice_index_, step, -1); });
      }
      captureRememberedNote(step);
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

void NotesPatternEditPage::draw(IGfx& gfx) {
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
  if (bank_bar_) bank_bar_->setFocusable(!songMode);
  if (pattern_bar_) pattern_bar_->setFocusable(!songMode);
  if (songMode && (patternRowFocused() || bankRowFocused())) {
    focusChild(step_grid_.get());
  }
  bool patternFocus = !songMode && patternRowFocused();
  bool bankFocus = !songMode && bankRowFocused();
  bool stepFocus = step_grid_ && step_grid_->isFocused();
  int patternCursor = songMode && selectedPattern >= 0 ? selectedPattern : activePatternCursor();
  int bankCursor = activeBankCursor();
  int label_h = gfx.fontHeight();
  if (pattern_label_) {
    pattern_label_->setBoundaries(Rect{x, body_y, w, label_h});
    pattern_label_->draw(gfx);
  }
  int pattern_bar_y = body_y + label_h + 1;

  PatternSelectionBarComponent::State pattern_state;
  pattern_state.pattern_count = Bank<SynthPattern>::kPatterns;
  pattern_state.selected_index = selectedPattern;
  pattern_state.cursor_index = patternCursor;
  pattern_state.show_cursor = patternFocus;
  pattern_state.song_mode = songMode;
  pattern_bar_->setState(pattern_state);
  pattern_bar_->setBoundaries(Rect{x, pattern_bar_y, w, 0});
  int pattern_bar_h = pattern_bar_->barHeight(gfx);
  pattern_bar_->setBoundaries(Rect{x, pattern_bar_y, w, pattern_bar_h});
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

  int grid_top = pattern_bar_y + pattern_bar_h + 6;
  PatternStepGridLayout grid_layout{};
  if (!computePatternStepGridLayout(Rect{x, grid_top, w, 0}, grid_layout)) return;
  if (step_grid_) {
    step_grid_->setBoundaries(Rect{x, grid_top, w, grid_layout.h});
  }

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int row = i / 8;
    int col = i % 8;
    int cell_x = grid_layout.x + col * (grid_layout.cell_size + grid_layout.spacing);
    int cell_y = grid_layout.y + row * grid_layout.row_height;

    int indicator_w = (grid_layout.cell_size - 2) / 2;
    if (indicator_w < 4) indicator_w = 4;
    int slide_x = cell_x + grid_layout.cell_size - indicator_w;
    int indicator_y = cell_y;

    gfx.fillRect(cell_x, indicator_y, indicator_w, grid_layout.indicator_h,
                 slide[i] ? COLOR_SLIDE : COLOR_GRAY_DARKER);
    gfx.drawRect(cell_x, indicator_y, indicator_w, grid_layout.indicator_h, COLOR_WHITE);
    gfx.fillRect(slide_x, indicator_y, indicator_w, grid_layout.indicator_h,
                 accent[i] ? COLOR_ACCENT : COLOR_GRAY_DARKER);
    gfx.drawRect(slide_x, indicator_y, indicator_w, grid_layout.indicator_h, COLOR_WHITE);

    int note_box_y = indicator_y + grid_layout.indicator_h + grid_layout.indicator_gap;
    IGfxColor fill = notes[i] >= 0 ? COLOR_303_NOTE : COLOR_GRAY;
    gfx.fillRect(cell_x, note_box_y, grid_layout.cell_size, grid_layout.cell_size, fill);
    gfx.drawRect(cell_x, note_box_y, grid_layout.cell_size, grid_layout.cell_size, COLOR_WHITE);

    if (playing == i) {
      gfx.drawRect(cell_x - 1, note_box_y - 1, grid_layout.cell_size + 2,
                   grid_layout.cell_size + 2, COLOR_STEP_HILIGHT);
    }
    if (stepFocus && stepCursor == i) {
      gfx.drawRect(cell_x - 2, note_box_y - 2, grid_layout.cell_size + 4,
                   grid_layout.cell_size + 4, COLOR_STEP_SELECTED);
    }

    char note_label[8];
    formatNoteName(notes[i], note_label, sizeof(note_label));
    int tw = textWidth(gfx, note_label);
    int tx = cell_x + (grid_layout.cell_size - tw) / 2;
    int ty = note_box_y + grid_layout.cell_size / 2 - gfx.fontHeight() / 2;
    gfx.drawText(tx, ty, note_label);
  }
}



PatternEditPage::PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index)
  : title_(voice_index == 0 ? "303A PATTERNS" : "303B PATTERNS") {
  addPage(std::make_shared<NotesPatternEditPage>(gfx, mini_acid, audio_guard, voice_index));
  addPage(std::make_shared<PatternAutomationPage>(gfx, mini_acid, audio_guard, voice_index));
}

const std::string & PatternEditPage::getTitle() const {
  IPage* active = activePage();
  if (active) {
    const std::string & title = active->getTitle();
    if (!title.empty()) return title;
  }
  return title_;
}

std::unique_ptr<MultiPageHelpDialog> PatternEditPage::getHelpDialog() {
  IPage* active = activePage();
  if (active) {
    auto dialog = active->getHelpDialog();
    if (dialog) return dialog;
  }
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
