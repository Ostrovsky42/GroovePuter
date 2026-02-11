#include "drum_sequencer_page.h"
#include "drum_automation_page.h"
#include "../ui_common.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
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
#include "../ui_widgets.h"

namespace retro = RetroWidgets;
namespace amber = AmberWidgets;

#include "../ui_clipboard.h"
#include "../../debug_log.h"
#include "../key_normalize.h"

namespace {
struct DrumAreaClipboard {
  bool has_area = false;
  int step_count = 0;
  int voice_count = 0;
  std::vector<DrumStep> steps;
};

DrumAreaClipboard g_drum_area_clipboard;

const char* drumGenreTag(GenerativeMode mode) {
  switch (mode) {
    case GenerativeMode::Acid: return "ACID";
    case GenerativeMode::Outrun: return "MINIMAL";
    case GenerativeMode::Darksynth: return "TECHNO";
    case GenerativeMode::Electro: return "ELECTRO";
    case GenerativeMode::Rave: return "RAVE";
    case GenerativeMode::Reggae: return "REGGAE";
    case GenerativeMode::TripHop: return "TRIPHOP";
    case GenerativeMode::Broken: return "BROKEN";
    case GenerativeMode::Chip: return "CHIP";
    default: return "ACID";
  }
}
} // namespace

class DrumSequencerMainPage : public Container {
 public:
  DrumSequencerMainPage(MiniAcid& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  void setContext(int context); // context: (voice << 8) | step

 private:
  void drawMinimalStyle(IGfx& gfx);
  void drawRetroClassicStyle(IGfx& gfx);
  void drawAmberStyle(IGfx& gfx);
  int activeDrumPatternCursor() const;
  int activeDrumStep() const;
  int activeDrumVoice() const;
  int activeBankCursor() const;
  void setDrumPatternCursor(int cursorIndex);
  void moveDrumCursor(int delta);
  void moveDrumCursorVertical(int delta);
  bool moveSelectionFrameBy(int deltaVoice, int deltaStep);
  void focusPatternRow();
  void focusGrid();
  bool patternRowFocused() const;
  void startSelection();
  void updateSelection();
  void clearSelection();
  bool hasSelection() const;
  void getSelectionBounds(int& min_voice, int& max_voice, int& min_step, int& max_step) const;
  bool isCellSelected(int step, int voice) const;
  int patternIndexFromKey(char key) const;
  int bankIndexFromKey(char key) const;
  void setBankIndex(int bankIndex);
  bool bankRowFocused() const;
  template <typename F>
  void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
  }

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int drum_step_cursor_;
  int drum_voice_cursor_;
  int drum_pattern_cursor_;
  int bank_index_;
  int bank_cursor_;
  bool bank_focus_;
  bool drum_pattern_focus_;
  std::shared_ptr<Component> grid_component_;
  std::shared_ptr<PatternSelectionBarComponent> pattern_bar_;
  std::shared_ptr<BankSelectionBarComponent> bank_bar_;
  bool chaining_mode_ = false;
  bool has_selection_ = false;
  int selection_start_step_ = 0;
  int selection_start_voice_ = 0;
  bool selection_locked_ = false;
};

class GlobalDrumSettingsPage : public Container {
 public:
  explicit GlobalDrumSettingsPage(MiniAcid& mini_acid);
  bool handleEvent(UIEvent& ui_event) override;
 void draw(IGfx& gfx) override;

 private:
  static constexpr int kDrumFxRows = 5;
  static constexpr float kDrumStep = 0.05f;
  static constexpr int kTotalRows = 1 + kDrumFxRows; // engine + FX rows

  void adjustDrumFx(int row, float delta);
  void applyDrumEngineSelection();
  void syncDrumEngineSelection();

  MiniAcid& mini_acid_;
  std::vector<std::string> drum_engine_options_;
  std::shared_ptr<LabelOptionComponent> character_control_;
  int selected_row_ = 0;
};

DrumSequencerMainPage::DrumSequencerMainPage(MiniAcid& mini_acid, AudioGuard audio_guard)
  : mini_acid_(mini_acid),
    audio_guard_(std::move(audio_guard)),
    drum_step_cursor_(0),
    drum_voice_cursor_(0),
    drum_pattern_cursor_(0),
    bank_index_(0),
    bank_cursor_(0),
    bank_focus_(false),
    drum_pattern_focus_(true) {
  int drumIdx = mini_acid_.currentDrumPatternIndex();
  if (drumIdx < 0 || drumIdx >= Bank<DrumPatternSet>::kPatterns) drumIdx = 0;
  drum_pattern_cursor_ = drumIdx;
  bank_index_ = mini_acid_.currentDrumBankIndex();
  bank_cursor_ = bank_index_;
  pattern_bar_ = std::make_shared<PatternSelectionBarComponent>("PATTERN");
  bank_bar_ = std::make_shared<BankSelectionBarComponent>("BANK", "AB");
  PatternSelectionBarComponent::Callbacks pattern_callbacks;
  pattern_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    drum_pattern_focus_ = true;
    bank_focus_ = false;
    setDrumPatternCursor(index);
    withAudioGuard([&]() { mini_acid_.setDrumPatternIndex(index); });
  };
  pattern_bar_->setCallbacks(std::move(pattern_callbacks));
  BankSelectionBarComponent::Callbacks bank_callbacks;
  bank_callbacks.onSelect = [this](int index) {
    if (mini_acid_.songModeEnabled()) return;
    bank_focus_ = true;
    drum_pattern_focus_ = false;
    bank_cursor_ = index;
    setBankIndex(index);
  };
  bank_bar_->setCallbacks(std::move(bank_callbacks));
  DrumSequencerGridComponent::Callbacks callbacks;
  callbacks.onToggle = [this](int step, int voice) {
    focusGrid();
    drum_step_cursor_ = step;
    drum_voice_cursor_ = voice;
    withAudioGuard([&]() { mini_acid_.toggleDrumStep(voice, step); });
  };
  callbacks.onToggleAccent = [this](int step) {
    focusGrid();
    drum_step_cursor_ = step;
    withAudioGuard([&]() { mini_acid_.toggleDrumAccentStep(step); });
  };
  callbacks.cursorStep = [this]() { return activeDrumStep(); };
  callbacks.cursorVoice = [this]() { return activeDrumVoice(); };
  callbacks.gridFocused = [this]() { return !patternRowFocused() && !bankRowFocused(); };
  callbacks.currentStep = [this]() { return mini_acid_.currentStep(); };
  callbacks.isSelected = [this](int step, int voice) { return isCellSelected(step, voice); };
  grid_component_ = std::make_shared<DrumSequencerGridComponent>(mini_acid_, std::move(callbacks));
  addChild(grid_component_);
}

int DrumSequencerMainPage::activeDrumPatternCursor() const {
  int idx = drum_pattern_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= Bank<DrumPatternSet>::kPatterns)
    idx = Bank<DrumPatternSet>::kPatterns - 1;
  return idx;
}

int DrumSequencerMainPage::activeDrumStep() const {
  int idx = drum_step_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}

int DrumSequencerMainPage::activeDrumVoice() const {
  int idx = drum_voice_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= NUM_DRUM_VOICES) idx = NUM_DRUM_VOICES - 1;
  return idx;
}

int DrumSequencerMainPage::activeBankCursor() const {
  int cursor = bank_cursor_;
  if (cursor < 0) cursor = 0;
  if (cursor >= kBankCount) cursor = kBankCount - 1;
  return cursor;
}

void DrumSequencerMainPage::setDrumPatternCursor(int cursorIndex) {
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<DrumPatternSet>::kPatterns)
    cursor = Bank<DrumPatternSet>::kPatterns - 1;
  drum_pattern_cursor_ = cursor;
}

void DrumSequencerMainPage::moveDrumCursor(int delta) {
  if (mini_acid_.songModeEnabled()) {
    drum_pattern_focus_ = false;
    bank_focus_ = false;
  }
  if (bank_focus_) {
    int cursor = activeBankCursor();
    cursor = (cursor + delta) % kBankCount;
    if (cursor < 0) cursor += kBankCount;
    bank_cursor_ = cursor;
    return;
  }
  if (drum_pattern_focus_) {
    int cursor = activeDrumPatternCursor();
    cursor = (cursor + delta) % Bank<DrumPatternSet>::kPatterns;
    if (cursor < 0) cursor += Bank<DrumPatternSet>::kPatterns;
    drum_pattern_cursor_ = cursor;
    return;
  }
  int step = activeDrumStep();
  step = (step + delta) % SEQ_STEPS;
  if (step < 0) step += SEQ_STEPS;
  drum_step_cursor_ = step;
}

void DrumSequencerMainPage::moveDrumCursorVertical(int delta) {
  if (delta == 0) return;
  if (mini_acid_.songModeEnabled()) {
    drum_pattern_focus_ = false;
    bank_focus_ = false;
  }
  if (bank_focus_) {
    if (delta > 0) {
      drum_pattern_focus_ = true;
      bank_focus_ = false;
    }
    return;
  }
  if (drum_pattern_focus_) {
    if (delta > 0) {
      drum_pattern_focus_ = false;
    }
    if (delta < 0 && !mini_acid_.songModeEnabled()) {
      bank_cursor_ = bank_index_;
      bank_focus_ = true;
      drum_pattern_focus_ = false;
    }
    return;
  }

  int voice = activeDrumVoice();
  int newVoice = voice + delta;
  if (newVoice < 0 || newVoice >= NUM_DRUM_VOICES) {
    drum_pattern_focus_ = true;
    drum_pattern_cursor_ = activeDrumStep() % Bank<DrumPatternSet>::kPatterns;
    return;
  }
  drum_voice_cursor_ = newVoice;
}

void DrumSequencerMainPage::startSelection() {
  has_selection_ = true;
  selection_locked_ = false;
  selection_start_step_ = activeDrumStep();
  selection_start_voice_ = activeDrumVoice();
}

void DrumSequencerMainPage::updateSelection() {
  if (!has_selection_) startSelection();
}

void DrumSequencerMainPage::clearSelection() {
  has_selection_ = false;
  selection_locked_ = false;
}

bool DrumSequencerMainPage::hasSelection() const {
  return has_selection_;
}

void DrumSequencerMainPage::getSelectionBounds(int& min_voice, int& max_voice, int& min_step, int& max_step) const {
  int a_step = selection_start_step_;
  if (a_step < 0) a_step = 0;
  if (a_step >= SEQ_STEPS) a_step = SEQ_STEPS - 1;
  int b_step = drum_step_cursor_;
  if (b_step < 0) b_step = 0;
  if (b_step >= SEQ_STEPS) b_step = SEQ_STEPS - 1;

  int a_voice = selection_start_voice_;
  if (a_voice < 0) a_voice = 0;
  if (a_voice >= NUM_DRUM_VOICES) a_voice = NUM_DRUM_VOICES - 1;
  int b_voice = drum_voice_cursor_;
  if (b_voice < 0) b_voice = 0;
  if (b_voice >= NUM_DRUM_VOICES) b_voice = NUM_DRUM_VOICES - 1;

  min_step = std::min(a_step, b_step);
  max_step = std::max(a_step, b_step);
  min_voice = std::min(a_voice, b_voice);
  max_voice = std::max(a_voice, b_voice);
}

bool DrumSequencerMainPage::isCellSelected(int step, int voice) const {
  if (!has_selection_) return false;
  int min_voice, max_voice, min_step, max_step;
  getSelectionBounds(min_voice, max_voice, min_step, max_step);
  return voice >= min_voice && voice <= max_voice &&
         step >= min_step && step <= max_step;
}

bool DrumSequencerMainPage::moveSelectionFrameBy(int deltaVoice, int deltaStep) {
  if (!has_selection_) return false;
  int min_voice, max_voice, min_step, max_step;
  getSelectionBounds(min_voice, max_voice, min_step, max_step);

  int dst_min_voice = min_voice + deltaVoice;
  int dst_max_voice = max_voice + deltaVoice;
  int dst_min_step = min_step + deltaStep;
  int dst_max_step = max_step + deltaStep;
  if (dst_min_voice < 0 || dst_max_voice >= NUM_DRUM_VOICES) return false;
  if (dst_min_step < 0 || dst_max_step >= SEQ_STEPS) return false;

  selection_start_voice_ += deltaVoice;
  selection_start_step_ += deltaStep;
  drum_voice_cursor_ += deltaVoice;
  drum_step_cursor_ += deltaStep;
  return true;
}

void DrumSequencerMainPage::focusPatternRow() {
  setDrumPatternCursor(drum_pattern_cursor_);
  drum_pattern_focus_ = true;
  bank_focus_ = false;
}

void DrumSequencerMainPage::focusGrid() {
  drum_pattern_focus_ = false;
  bank_focus_ = false;
  drum_step_cursor_ = activeDrumStep();
  drum_voice_cursor_ = activeDrumVoice();
}

bool DrumSequencerMainPage::patternRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return drum_pattern_focus_;
}

bool DrumSequencerMainPage::bankRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return bank_focus_;
}

int DrumSequencerMainPage::patternIndexFromKey(char key) const {
  return qwertyToPatternIndex(key);
}

int DrumSequencerMainPage::bankIndexFromKey(char key) const {
  switch (key) {
    case '1': return 0;
    case '2': return 1;
    case '3': return 2;
    case '4': return 3;
    default: return -1;
  }
}

void DrumSequencerMainPage::setBankIndex(int bankIndex) {
  if (bankIndex < 0) bankIndex = 0;
  if (bankIndex >= kBankCount) bankIndex = kBankCount - 1;
  if (bank_index_ == bankIndex) return;
  bank_index_ = bankIndex;
  withAudioGuard([&]() { mini_acid_.setDrumBankIndex(bank_index_); });
}

void DrumSequencerMainPage::setContext(int context) {
    int voice = (context >> 8) & 0xFF;
    int step = context & 0xFF;
    
    if (voice >= 0 && voice < NUM_DRUM_VOICES) drum_voice_cursor_ = voice;
    if (step >= 0 && step < SEQ_STEPS) drum_step_cursor_ = step;
    
    // Sync cursors with engine
    drum_pattern_cursor_ = mini_acid_.currentDrumPatternIndex();
    bank_index_ = mini_acid_.currentDrumBankIndex();
    bank_cursor_ = bank_index_;
    
    drum_pattern_focus_ = false;
    bank_focus_ = false;
    focusGrid();
}

bool DrumSequencerMainPage::handleEvent(UIEvent& ui_event) {
  if (pattern_bar_ && pattern_bar_->handleEvent(ui_event)) return true;
  if (bank_bar_ && bank_bar_->handleEvent(ui_event)) return true;
  if (Container::handleEvent(ui_event)) return true;

  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT) {
    switch (ui_event.app_event_type) {
      case GROOVEPUTER_APP_EVENT_COPY: {
        const bool* hits[NUM_DRUM_VOICES] = {
          mini_acid_.patternKickSteps(),
          mini_acid_.patternSnareSteps(),
          mini_acid_.patternHatSteps(),
          mini_acid_.patternOpenHatSteps(),
          mini_acid_.patternMidTomSteps(),
          mini_acid_.patternHighTomSteps(),
          mini_acid_.patternRimSteps(),
          mini_acid_.patternClapSteps()
        };
        const bool* accents[NUM_DRUM_VOICES] = {
          mini_acid_.patternKickAccentSteps(),
          mini_acid_.patternSnareAccentSteps(),
          mini_acid_.patternHatAccentSteps(),
          mini_acid_.patternOpenHatAccentSteps(),
          mini_acid_.patternMidTomAccentSteps(),
          mini_acid_.patternHighTomAccentSteps(),
          mini_acid_.patternRimAccentSteps(),
          mini_acid_.patternClapAccentSteps()
        };
        if (has_selection_) {
          int min_voice, max_voice, min_step, max_step;
          getSelectionBounds(min_voice, max_voice, min_step, max_step);
          g_drum_area_clipboard.voice_count = max_voice - min_voice + 1;
          g_drum_area_clipboard.step_count = max_step - min_step + 1;
          g_drum_area_clipboard.steps.clear();
          g_drum_area_clipboard.steps.reserve(g_drum_area_clipboard.voice_count * g_drum_area_clipboard.step_count);
          for (int v = min_voice; v <= max_voice; ++v) {
            for (int i = min_step; i <= max_step; ++i) {
              DrumStep s{};
              s.hit = hits[v][i];
              s.accent = accents[v][i];
              g_drum_area_clipboard.steps.push_back(s);
            }
          }
          g_drum_area_clipboard.has_area = true;
          selection_locked_ = true;
          g_drum_pattern_clipboard.has_pattern = false;
          return true;
        }

        for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
          for (int i = 0; i < SEQ_STEPS; ++i) {
            g_drum_pattern_clipboard.pattern.voices[v].steps[i].hit = hits[v][i];
            g_drum_pattern_clipboard.pattern.voices[v].steps[i].accent = accents[v][i];
          }
        }
        g_drum_pattern_clipboard.has_pattern = true;
        g_drum_area_clipboard.has_area = false;
        return true;
      }
      case GROOVEPUTER_APP_EVENT_PASTE: {
        if (!g_drum_pattern_clipboard.has_pattern && !g_drum_area_clipboard.has_area) return false;
        bool current_hits[NUM_DRUM_VOICES][SEQ_STEPS];
        bool current_accents[NUM_DRUM_VOICES][SEQ_STEPS];
        const bool* hits[NUM_DRUM_VOICES] = {
          mini_acid_.patternKickSteps(),
          mini_acid_.patternSnareSteps(),
          mini_acid_.patternHatSteps(),
          mini_acid_.patternOpenHatSteps(),
          mini_acid_.patternMidTomSteps(),
          mini_acid_.patternHighTomSteps(),
          mini_acid_.patternRimSteps(),
          mini_acid_.patternClapSteps()
        };
        const bool* accents[NUM_DRUM_VOICES] = {
          mini_acid_.patternKickAccentSteps(),
          mini_acid_.patternSnareAccentSteps(),
          mini_acid_.patternHatAccentSteps(),
          mini_acid_.patternOpenHatAccentSteps(),
          mini_acid_.patternMidTomAccentSteps(),
          mini_acid_.patternHighTomAccentSteps(),
          mini_acid_.patternRimAccentSteps(),
          mini_acid_.patternClapAccentSteps()
        };
        for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
          for (int i = 0; i < SEQ_STEPS; ++i) {
            current_hits[v][i] = hits[v][i];
            current_accents[v][i] = accents[v][i];
          }
        }
        const DrumPatternSet& src = g_drum_pattern_clipboard.pattern;
        withAudioGuard([&]() {
          if (g_drum_area_clipboard.has_area) {
            int start_step = activeDrumStep();
            int start_voice = activeDrumVoice();
            if (has_selection_) {
              int min_voice, max_voice, min_step, max_step;
              getSelectionBounds(min_voice, max_voice, min_step, max_step);
              start_step = min_step;
              start_voice = min_voice;
            }
            int idx = 0;
            for (int v = 0; v < g_drum_area_clipboard.voice_count; ++v) {
              for (int i = 0; i < g_drum_area_clipboard.step_count; ++i) {
                if (idx >= static_cast<int>(g_drum_area_clipboard.steps.size())) break;
                int tv = start_voice + v;
                int ts = start_step + i;
                if (tv < 0 || tv >= NUM_DRUM_VOICES || ts < 0 || ts >= SEQ_STEPS) {
                  ++idx;
                  continue;
                }
                bool desiredHit = g_drum_area_clipboard.steps[idx].hit;
                bool desiredAccent = g_drum_area_clipboard.steps[idx].accent && desiredHit;
                if (current_hits[tv][ts] != desiredHit) {
                  mini_acid_.toggleDrumStep(tv, ts);
                }
                if (current_accents[tv][ts] != desiredAccent) {
                  mini_acid_.setDrumAccentStep(tv, ts, desiredAccent);
                }
                ++idx;
              }
            }
          } else {
            for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
              for (int i = 0; i < SEQ_STEPS; ++i) {
                bool desiredHit = src.voices[v].steps[i].hit;
                bool desiredAccent = src.voices[v].steps[i].accent && desiredHit;
                if (current_hits[v][i] != desiredHit) {
                  mini_acid_.toggleDrumStep(v, i);
                }
                if (current_accents[v][i] != desiredAccent) {
                  mini_acid_.setDrumAccentStep(v, i, desiredAccent);
                }
              }
            }
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
  if ((ui_event.scancode == GROOVEPUTER_ESCAPE || ui_event.key == 0x1B) && ui_event.alt) {
    chaining_mode_ = !chaining_mode_;
    return true;
  }

  // Local ESC/backtick: clear selection first.
  if ((ui_event.scancode == GROOVEPUTER_ESCAPE || ui_event.key == '`' || ui_event.key == '~') && has_selection_) {
    clearSelection();
    return true;
  }

  // Let parent handle global navigation ([ ] page jumps, help, back, etc.)
  // IMPORTANT: we do NOT want to steal them here.
  if (UIInput::isGlobalNav(ui_event)) return false;

  bool handled = false;
  
  // Arrow-first: Cardputer may deliver arrows in scancode OR key.
  // Keep vim-keys only as silent fallback (not in footer hints).
  int nav = UIInput::navCode(ui_event);
  bool extend_selection = ui_event.shift || ui_event.ctrl;
  if (extend_selection && selection_locked_) selection_locked_ = false;
  if (selection_locked_ && has_selection_ && !extend_selection && !patternRowFocused() && !bankRowFocused()) {
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
      if (ui_event.alt) {
        int next = mini_acid_.currentPageIndex() - 1;
        if (next < 0) next = kMaxPages - 1;
        mini_acid_.requestPageSwitch(next);
        handled = true;
        break;
      }
      if (extend_selection && !patternRowFocused() && !bankRowFocused()) updateSelection();
      moveDrumCursor(-1);
      handled = true;
      break;
    case GROOVEPUTER_RIGHT:
      if (ui_event.alt) {
        int next = (mini_acid_.currentPageIndex() + 1) % kMaxPages;
        mini_acid_.requestPageSwitch(next);
        handled = true;
        break;
      }
      if (extend_selection && !patternRowFocused() && !bankRowFocused()) updateSelection();
      moveDrumCursor(1);
      handled = true;
      break;
    case GROOVEPUTER_UP:
      if (extend_selection && !patternRowFocused() && !bankRowFocused()) updateSelection();
      moveDrumCursorVertical(-1);
      handled = true;
      break;
    case GROOVEPUTER_DOWN:
      if (extend_selection && !patternRowFocused() && !bankRowFocused()) updateSelection();
      moveDrumCursorVertical(1);
      handled = true;
      break;
    default:
      break;
  }

  if (handled) return true;

  char key = ui_event.key;
  if (key == 0) {
    if (ui_event.scancode >= GROOVEPUTER_F1 && ui_event.scancode <= GROOVEPUTER_F8) {
      key = static_cast<char>('1' + (ui_event.scancode - GROOVEPUTER_F1));
    }
  }
  char lowerKey = key ? static_cast<char>(std::tolower(static_cast<unsigned char>(key))) : 0;

  // Bank Selection (Ctrl + 1..2)
  if (ui_event.ctrl && !ui_event.alt && key >= '1' && key <= '2') {
    int bankIdx = bankIndexFromKey(key);
    if (bankIdx >= 0) {
      setBankIndex(bankIdx);
      if (!mini_acid_.songModeEnabled()) {
        bank_focus_ = true;
        drum_pattern_focus_ = false;
      }
      UI::showToast(bankIdx == 0 ? "Bank: A" : "Bank: B", 800);
      return true;
    }
  }

  if (key == '\n' || key == '\r') {
    if (has_selection_) {
      int min_voice, max_voice, min_step, max_step;
      getSelectionBounds(min_voice, max_voice, min_step, max_step);
      if (min_voice == max_voice && min_step == max_step) {
        clearSelection();
        return true;
      }
    }
    if (bankRowFocused()) {
      if (mini_acid_.songModeEnabled()) return true;
      setBankIndex(activeBankCursor());
    } else if (patternRowFocused()) {
      int cursor = activeDrumPatternCursor();
      withAudioGuard([&]() { mini_acid_.setDrumPatternIndex(cursor); });
    } else {
      int step = activeDrumStep();
      int voice = activeDrumVoice();
      withAudioGuard([&]() { mini_acid_.toggleDrumStep(voice, step); });
    }
    return true;
  }

  // Pattern quick select (Q-I) - only if NO modifiers
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    int patternIdx = patternIndexFromKey(lowerKey);
    if (patternIdx < 0) {
        patternIdx = scancodeToPatternIndex(ui_event.scancode);
    }
    
    if (patternIdx >= 0) {
      if (mini_acid_.songModeEnabled()) return true;
      focusPatternRow();
      setDrumPatternCursor(patternIdx);
      withAudioGuard([&]() { 
          mini_acid_.setDrumPatternIndex(patternIdx); 
          if (chaining_mode_) {
              // Find next empty position in song and append
              SongTrack track = SongTrack::Drums;
              int nextPos = -1;
              for (int i = 0; i < Song::kMaxPositions; ++i) {
                  if (mini_acid_.songPatternAt(i, track) == -1) {
                      nextPos = i;
                      break;
                  }
              }
              if (nextPos != -1) {
                  mini_acid_.setSongPattern(nextPos, track, patternIdx);
              }
          }
      });
      return true;
    }
  }

  bool key_a = (lowerKey == 'a') || (ui_event.scancode == GROOVEPUTER_A);
  bool key_b = (lowerKey == 'b') || (ui_event.scancode == GROOVEPUTER_B);
  bool key_g = (lowerKey == 'g') || (ui_event.scancode == GROOVEPUTER_G);
  bool key_c = (lowerKey == 'c') || (ui_event.scancode == GROOVEPUTER_C);
  bool key_v = (lowerKey == 'v') || (ui_event.scancode == GROOVEPUTER_V);

  if (key_a) {
    focusGrid();
    int step = activeDrumStep();
    withAudioGuard([&]() { mini_acid_.toggleDrumAccentStep(step); });
    return true;
  }
  if (key_b && !ui_event.alt && !ui_event.ctrl) {
    if (mini_acid_.songModeEnabled()) return true;
    int nextBank = (activeBankCursor() + 1) % kBankCount;
    bank_cursor_ = nextBank;
    setBankIndex(nextBank);
    return true;
  }
  if (key_g) {
    if (ui_event.ctrl) {
      int voice = activeDrumVoice();
      withAudioGuard([&]() { mini_acid_.randomizeDrumVoice(voice); });
    } else if (ui_event.alt) {
      withAudioGuard([&]() { mini_acid_.randomizeDrumPatternChaos(); });
    } else {
      withAudioGuard([&]() { mini_acid_.randomizeDrumPattern(); });
    }
    return true;
  }
  if (key_c && ui_event.ctrl) {
    UIEvent app_evt{};
    app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
    app_evt.app_event_type = GROOVEPUTER_APP_EVENT_COPY;
    return handleEvent(app_evt);
  }
  if (key_v && ui_event.ctrl) {
    UIEvent app_evt{};
    app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
    app_evt.app_event_type = GROOVEPUTER_APP_EVENT_PASTE;
    return handleEvent(app_evt);
  }

  if (key == '\b' || key == 0x7F) {
    if (ui_event.alt) {
        // Alt+Backspace = Clear Pattern
        withAudioGuard([&]() {
            for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
                for (int i = 0; i < SEQ_STEPS; ++i) {
                    mini_acid_.sceneManager().setDrumStep(v, i, false, false);
                }
            }
        });
        UI::showToast("Drums Cleared");
        return true;
    } else if (has_selection_) {
        int min_v, max_v, min_s, max_s;
        getSelectionBounds(min_v, max_v, min_s, max_s);
        withAudioGuard([&]() {
            for (int v = min_v; v <= max_v; ++v) {
                for (int s = min_s; s <= max_s; ++s) {
                    mini_acid_.sceneManager().setDrumStep(v, s, false, false);
                }
            }
        });
        clearSelection();
        return true;
    } else {
        // Backspace = Clear current voice/step
        int voice = activeDrumVoice();
        int step = activeDrumStep();
        if (!patternRowFocused() && !bankRowFocused()) {
            withAudioGuard([&]() { mini_acid_.sceneManager().setDrumStep(voice, step, false, false); });
            return true;
        }
    }
  }

  return false;
}

void DrumSequencerMainPage::draw(IGfx& gfx) {
  GrooveboxStyle style = UI::currentStyle;
  std::static_pointer_cast<DrumSequencerGridComponent>(grid_component_)->setStyle(style);

  switch (style) {
    case GrooveboxStyle::RETRO_CLASSIC:
      drawRetroClassicStyle(gfx);
      break;
    case GrooveboxStyle::AMBER:
      drawAmberStyle(gfx);
      break;
    default:
      drawMinimalStyle(gfx);
      break;
  }
}

void DrumSequencerMainPage::drawMinimalStyle(IGfx& gfx) {
  bank_index_ = mini_acid_.currentDrumBankIndex();
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  if (chaining_mode_) {
      gfx.setTextColor(COLOR_ACCENT);
      gfx.drawText(x + w - 40, y + 1, "CHAIN");
  }

  int body_y = y + 2;
  int body_h = h - 2;
  if (body_h <= 0) return;

  bool songMode = mini_acid_.songModeEnabled();
  bool bankFocus = !songMode && bankRowFocused();
  int bankCursor = activeBankCursor();

  int selectedPattern = mini_acid_.displayDrumPatternIndex();
  int patternCursor = activeDrumPatternCursor();
  bool patternFocus = !songMode && patternRowFocused();
  if (songMode && selectedPattern >= 0) patternCursor = selectedPattern;
  PatternSelectionBarComponent::State pattern_state;
  pattern_state.pattern_count = Bank<DrumPatternSet>::kPatterns;
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
  bank_bar_->setBoundaries(Rect{x, body_y + pattern_bar_h, w, 0});
  int bank_bar_h = bank_bar_->barHeight(gfx);
  bank_bar_->setBoundaries(Rect{x, body_y + pattern_bar_h, w, bank_bar_h});
  bank_bar_->draw(gfx);

  // Page Indicator
  char pageBuf[8];
  snprintf(pageBuf, sizeof(pageBuf), "P%d", mini_acid_.currentPageIndex() + 1);
  char genreBuf[20];
  std::snprintf(genreBuf, sizeof(genreBuf), "[%s]",
                drumGenreTag(mini_acid_.genreManager().generativeMode()));
  gfx.setTextColor(COLOR_WHITE);
  int genreX = x + w - 28 - gfx.textWidth(genreBuf);
  if (genreX < x + 2) genreX = x + 2;
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(genreX, y + 2, genreBuf);
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x + w - 24, y + 2, pageBuf);

  int grid_y = body_y + pattern_bar_h + bank_bar_h;
  int grid_h = body_h - (pattern_bar_h + bank_bar_h);
  if (grid_h <= 0) {
    if (grid_component_) {
      grid_component_->setBoundaries(Rect{0, 0, 0, 0});
    }
    return;
  }
  grid_component_->setBoundaries(Rect{x, grid_y, w, grid_h});
  grid_component_->draw(gfx);
  Container::draw(gfx); // This line was originally outside the if (grid_h <= 0) block, but inside the original draw.
                        // It should probably be called after grid_component_->draw(gfx) if grid_component_ is a child.
                        // For minimal style, it's fine here.
}

void DrumSequencerMainPage::drawRetroClassicStyle(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    char modeBuf[32];
    std::snprintf(modeBuf, sizeof(modeBuf), "%s [%s]",
                  mini_acid_.currentDrumEngineName().c_str(),
                  drumGenreTag(mini_acid_.genreManager().generativeMode()));
    char titleBuf[16];
    std::snprintf(titleBuf, sizeof(titleBuf), "DRUMS P%d", mini_acid_.currentPageIndex() + 1);
    retro::drawHeaderBar(gfx, x, y, w, 12, titleBuf, modeBuf, mini_acid_.isPlaying(), (int)mini_acid_.bpm(), mini_acid_.currentSongPosition());

    bool songMode = mini_acid_.songModeEnabled();
    int selectedPattern = mini_acid_.displayDrumLocalPatternIndex();
    
    retro::SelectorConfig pCfg;
    pCfg.x = x + 4; pCfg.y = y + 14; pCfg.w = w - 8; pCfg.h = 10;
    pCfg.label = "PTRN";
    pCfg.count = Bank<DrumPatternSet>::kPatterns;
    pCfg.selected = selectedPattern;
    pCfg.cursor = activeDrumPatternCursor();
    pCfg.showCursor = !songMode && patternRowFocused();
    pCfg.enabled = !songMode;
    retro::drawSelector(gfx, pCfg);

    retro::SelectorConfig bCfg;
    bCfg.x = x + w - 52; bCfg.y = y + 26; bCfg.w = 48; bCfg.h = 10;
    bCfg.label = "BK";
    bCfg.count = kBankCount;
    bCfg.selected = mini_acid_.currentDrumBankIndex();
    bCfg.cursor = activeBankCursor();
    bCfg.showCursor = !songMode && bankRowFocused();
    bCfg.enabled = !songMode;
    bCfg.alphaLabels = true;
    retro::drawSelector(gfx, bCfg);

    int grid_y = y + 38;
    int grid_h = h - 38 - 12; // footer is 12
    grid_component_->setBoundaries(Rect{x, grid_y, w, grid_h});
    grid_component_->draw(gfx);

    retro::drawFooterBar(gfx, x, y + h - 12, w, 12, "f:GEN Alt+G:ALL 1..8:Edit B:Bank", "DRUM");
}

void DrumSequencerMainPage::drawAmberStyle(IGfx& gfx) {
    const Rect& bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    char modeBuf[32];
    std::snprintf(modeBuf, sizeof(modeBuf), "%s [%s]",
                  mini_acid_.currentDrumEngineName().c_str(),
                  drumGenreTag(mini_acid_.genreManager().generativeMode()));
    char titleBuf[16];
    std::snprintf(titleBuf, sizeof(titleBuf), "DRUMS P%d", mini_acid_.currentPageIndex() + 1);
    amber::drawHeaderBar(gfx, x, y, w, 12, titleBuf, modeBuf, mini_acid_.isPlaying(), (int)mini_acid_.bpm(), mini_acid_.currentSongPosition());

    bool songMode = mini_acid_.songModeEnabled();
    int selectedPattern = mini_acid_.displayDrumLocalPatternIndex();

    amber::SelectionBarConfig pCfg;
    pCfg.x = x + 4; pCfg.y = y + 14; pCfg.w = w - 8; pCfg.h = 10;
    pCfg.label = "PTRN";
    pCfg.count = Bank<DrumPatternSet>::kPatterns;
    pCfg.selected = selectedPattern;
    pCfg.cursor = activeDrumPatternCursor();
    pCfg.showCursor = !songMode && patternRowFocused();
    amber::drawSelectionBar(gfx, pCfg);

    amber::SelectionBarConfig bCfg;
    bCfg.x = x + w - 52; bCfg.y = y + 26; bCfg.w = 48; bCfg.h = 10;
    bCfg.label = "BK";
    bCfg.count = kBankCount;
    bCfg.selected = mini_acid_.currentDrumBankIndex();
    bCfg.cursor = activeBankCursor();
    bCfg.showCursor = !songMode && bankRowFocused();
    bCfg.alphaLabels = true;
    amber::drawSelectionBar(gfx, bCfg);

    int grid_y = y + 38;
    int grid_h = h - 38 - 12;
    grid_component_->setBoundaries(Rect{x, grid_y, w, grid_h});
    grid_component_->draw(gfx);

    amber::drawFooterBar(gfx, x, y + h - 12, w, 12, "f:GEN Alt+G:ALL 1..8:Edit B:Bank", "DRUM");
}
#include "../retro_widgets.h"
#include "../amber_widgets.h"
#include "../retro_ui_theme.h"
#include "../amber_ui_theme.h"



GlobalDrumSettingsPage::GlobalDrumSettingsPage(MiniAcid& mini_acid)
  : mini_acid_(mini_acid) {
  character_control_ = std::make_shared<LabelOptionComponent>(
      "Character", COLOR_LABEL, COLOR_WHITE);
  drum_engine_options_ = mini_acid_.getAvailableDrumEngines();
  if (drum_engine_options_.empty()) {
    drum_engine_options_ = {"808", "909", "606"};
  }
  character_control_->setOptions(drum_engine_options_);
  addChild(character_control_);
}

bool GlobalDrumSettingsPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN) {
    const int nav = UIInput::navCode(ui_event);
    if (nav == GROOVEPUTER_UP) {
      if (selected_row_ > 0) selected_row_--;
      return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
      if (selected_row_ < kTotalRows - 1) selected_row_++;
      return true;
    }
    if (selected_row_ > 0 && (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)) {
      adjustDrumFx(selected_row_ - 1, nav == GROOVEPUTER_LEFT ? -kDrumStep : kDrumStep);
      return true;
    }
  }

  if (selected_row_ != 0) return false;
  int before = character_control_ ? character_control_->optionIndex() : -1;
  bool handled = Container::handleEvent(ui_event);
  int after = character_control_ ? character_control_->optionIndex() : -1;
  if (before != after) {
    applyDrumEngineSelection();
  }
  return handled;
}

void GlobalDrumSettingsPage::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  if (bounds.w <= 0 || bounds.h <= 0) return;
  syncDrumEngineSelection();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;

  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, y, "GLOBAL SETTINGS");
  gfx.setTextColor(COLOR_WHITE);

  int row_y = y + gfx.fontHeight() + 4;
  if (character_control_) {
    character_control_->setBoundaries(Rect{x, row_y, w, gfx.fontHeight()});
  }
  Container::draw(gfx);

  const DrumFX& dfx = mini_acid_.sceneManager().currentScene().drumFX;
  int y_cursor = row_y + gfx.fontHeight() + 4;
  char buf[24];

  int compPct = static_cast<int>(std::clamp(dfx.compression, 0.0f, 1.0f) * 100.0f + 0.5f);
  std::snprintf(buf, sizeof(buf), "DR CMP %d%%", compPct);
  Widgets::drawListRow(gfx, x, y_cursor, w, buf, selected_row_ == 1);
  y_cursor += gfx.fontHeight() + 2;

  int attPct = static_cast<int>(std::clamp(dfx.transientAttack, -1.0f, 1.0f) * 100.0f + 0.5f);
  std::snprintf(buf, sizeof(buf), "DR ATT %+d%%", attPct);
  Widgets::drawListRow(gfx, x, y_cursor, w, buf, selected_row_ == 2);
  y_cursor += gfx.fontHeight() + 2;

  int susPct = static_cast<int>(std::clamp(dfx.transientSustain, -1.0f, 1.0f) * 100.0f + 0.5f);
  std::snprintf(buf, sizeof(buf), "DR SUS %+d%%", susPct);
  Widgets::drawListRow(gfx, x, y_cursor, w, buf, selected_row_ == 3);
  y_cursor += gfx.fontHeight() + 2;

  int mixPct = static_cast<int>(std::clamp(dfx.reverbMix, 0.0f, 1.0f) * 100.0f + 0.5f);
  std::snprintf(buf, sizeof(buf), "DR REV %d%%", mixPct);
  Widgets::drawListRow(gfx, x, y_cursor, w, buf, selected_row_ == 4);
  y_cursor += gfx.fontHeight() + 2;

  int decPct = static_cast<int>(std::clamp(dfx.reverbDecay, 0.05f, 0.95f) * 100.0f + 0.5f);
  std::snprintf(buf, sizeof(buf), "DR DEC %d%%", decPct);
  Widgets::drawListRow(gfx, x, y_cursor, w, buf, selected_row_ == 5);
}

void GlobalDrumSettingsPage::adjustDrumFx(int row, float delta) {
  auto& dfx = mini_acid_.sceneManager().currentScene().drumFX;
  if (row == 0) {
    dfx.compression = std::clamp(dfx.compression + delta, 0.0f, 1.0f);
    mini_acid_.updateDrumCompression(dfx.compression);
    return;
  }
  if (row == 1) {
    dfx.transientAttack = std::clamp(dfx.transientAttack + delta, -1.0f, 1.0f);
    mini_acid_.updateDrumTransientAttack(dfx.transientAttack);
    return;
  }
  if (row == 2) {
    dfx.transientSustain = std::clamp(dfx.transientSustain + delta, -1.0f, 1.0f);
    mini_acid_.updateDrumTransientSustain(dfx.transientSustain);
    return;
  }
  if (row == 3) {
    dfx.reverbMix = std::clamp(dfx.reverbMix + delta, 0.0f, 1.0f);
    mini_acid_.updateDrumReverbMix(dfx.reverbMix);
    return;
  }
  if (row == 4) {
    dfx.reverbDecay = std::clamp(dfx.reverbDecay + delta, 0.05f, 0.95f);
    mini_acid_.updateDrumReverbDecay(dfx.reverbDecay);
  }
}

void GlobalDrumSettingsPage::applyDrumEngineSelection() {
  if (!character_control_) return;
  int index = character_control_->optionIndex();
  if (index < 0 || index >= static_cast<int>(drum_engine_options_.size())) return;
  mini_acid_.setDrumEngine(drum_engine_options_[index]);
}

void GlobalDrumSettingsPage::syncDrumEngineSelection() {
  if (!character_control_) return;
  std::string current = mini_acid_.currentDrumEngineName();
  if (current.empty()) return;
  int target = -1;
  for (int i = 0; i < static_cast<int>(drum_engine_options_.size()); ++i) {
    if (drum_engine_options_[i] == current) {
      target = i;
      break;
    }
  }
  if (target < 0) return;
  if (character_control_->optionIndex() == target) return;
  character_control_->setOptionIndex(target);
}



DrumSequencerPage::DrumSequencerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard) {
  (void)gfx;
  addPage(std::make_shared<DrumSequencerMainPage>(mini_acid, audio_guard));
  addPage(std::make_shared<GlobalDrumSettingsPage>(mini_acid));
  addPage(std::make_shared<DrumAutomationPage>(mini_acid));
}

bool DrumSequencerPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN && UIInput::isTab(ui_event)) {
    return stepActivePage(1);
  }
  return MultiPage::handleEvent(ui_event);
}

const std::string & DrumSequencerPage::getTitle() const {
  static std::string title = "DRUM SEQUENCER";
  return title;
}

std::unique_ptr<MultiPageHelpDialog> DrumSequencerPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int DrumSequencerPage::getHelpFrameCount() const {
  return 1;
}

void DrumSequencerPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPageDrumPatternEdit(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}
void DrumSequencerPage::setContext(int context) {
    // Jump to the first page (Main Sequencer)
    setActivePageIndex(0);
    // Pass it down (we know index 0 is Main Page)
    auto mainPage = std::static_pointer_cast<DrumSequencerMainPage>(getPagePtr(0)); 
    if (mainPage) mainPage->setContext(context);
}
