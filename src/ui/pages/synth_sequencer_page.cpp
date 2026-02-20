#include "synth_sequencer_page.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "../../../platform_sdl/arduino_compat.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "pattern_edit_page.h"
#include "../components/label_option.h"
#include "../help_dialog_frames.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../ui_widgets.h"

namespace {
inline std::string upperCopy(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return s;
}

inline int findOptionIndex(const std::vector<std::string>& options, const std::string& value) {
  if (options.empty()) return -1;
  const std::string target = upperCopy(value);
  for (int i = 0; i < static_cast<int>(options.size()); ++i) {
    if (upperCopy(options[i]) == target) return i;
  }
  return -1;
}

class GlobalSynthSettingsPage final : public Container {
 public:
  GlobalSynthSettingsPage(MiniAcid& mini_acid, AudioGuard audio_guard, int voice_index)
      : mini_acid_(mini_acid),
        audio_guard_(std::move(audio_guard)),
        voice_index_(voice_index) {
    engine_control_ = std::make_shared<LabelOptionComponent>("Engine", COLOR_LABEL, COLOR_WHITE);
    synth_engine_options_ = mini_acid_.getAvailableSynthEngines();
    if (synth_engine_options_.empty()) synth_engine_options_ = {"TB303", "SID", "AY", "OPL2"};
    engine_control_->setOptions(synth_engine_options_);
    addChild(engine_control_);
  }

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type == GROOVEPUTER_KEY_DOWN) {
      const int nav = UIInput::navCode(ui_event);
      const int max_row = visibleParamCount();
      if (nav == GROOVEPUTER_UP) {
        if (selected_row_ > 0) --selected_row_;
        return true;
      }
      if (nav == GROOVEPUTER_DOWN) {
        if (selected_row_ < max_row) ++selected_row_;
        return true;
      }
      if (selected_row_ > 0 && (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)) {
        int delta = (nav == GROOVEPUTER_LEFT) ? -1 : 1;
        if (!ui_event.ctrl && !ui_event.shift) delta *= kCoarseStep;
        adjustParam(selected_row_ - 1, delta);
        return true;
      }
    }

    if (selected_row_ != 0) return false;
    int before = engine_control_ ? engine_control_->optionIndex() : -1;
    bool handled = Container::handleEvent(ui_event);
    int after = engine_control_ ? engine_control_->optionIndex() : -1;
    if (before != after) applyEngineSelection();
    return handled;
  }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    if (bounds.w <= 0 || bounds.h <= 0) return;

    syncEngineSelection();
    clampSelectedRow();

    const int x = bounds.x;
    const int y = bounds.y;
    const int w = bounds.w;

    char header[28];
    std::snprintf(header, sizeof(header), "SYNTH %c SETTINGS", voice_index_ == 0 ? 'A' : 'B');
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x, y, header);

    int row_y = y + gfx.fontHeight() + 4;
    if (engine_control_) {
      engine_control_->setBoundaries(Rect{x, row_y, w, gfx.fontHeight()});
    }
    Container::draw(gfx);

    int y_cursor = row_y + gfx.fontHeight() + 4;
    int param_count = visibleParamCount();
    if (param_count <= 0) {
      Widgets::drawListRow(gfx, x, y_cursor, w, "No engine parameters", selected_row_ == 1);
      return;
    }

    for (int i = 0; i < param_count; ++i) {
      const Parameter& p = mini_acid_.synthParameter(voice_index_, i);
      char line[56];
      formatParameterLine(p, line, sizeof(line));
      Widgets::drawListRow(gfx, x, y_cursor, w, line, selected_row_ == (i + 1));
      y_cursor += gfx.fontHeight() + 2;
    }
  }

 private:
  static constexpr int kMaxParamRows = 6;
  static constexpr int kCoarseStep = 3;

  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
  }

  int visibleParamCount() const {
    int count = static_cast<int>(mini_acid_.synthParameterCount(voice_index_));
    if (count < 0) count = 0;
    if (count > kMaxParamRows) count = kMaxParamRows;
    return count;
  }

  void clampSelectedRow() {
    int max_row = visibleParamCount();
    if (selected_row_ < 0) selected_row_ = 0;
    if (selected_row_ > max_row) selected_row_ = max_row;
  }

  void adjustParam(int param_index, int steps) {
    if (param_index < 0 || param_index >= visibleParamCount()) return;
    withAudioGuard([&]() { mini_acid_.adjustSynthParameter(voice_index_, param_index, steps); });
  }

  void applyEngineSelection() {
    if (!engine_control_) return;
    int index = engine_control_->optionIndex();
    if (index < 0 || index >= static_cast<int>(synth_engine_options_.size())) return;

    withAudioGuard([&]() { mini_acid_.setSynthEngine(voice_index_, synth_engine_options_[index]); });
    char toast[30];
    std::snprintf(toast, sizeof(toast), "SYNTH %c: %s",
                  voice_index_ == 0 ? 'A' : 'B',
                  synth_engine_options_[index].c_str());
    UI::showToast(toast, 800);
    clampSelectedRow();
  }

  void syncEngineSelection() {
    if (!engine_control_) return;
    std::string current = mini_acid_.currentSynthEngineName(voice_index_);
    if (current.empty()) return;
    int target = findOptionIndex(synth_engine_options_, current);
    if (target < 0) return;
    if (engine_control_->optionIndex() == target) return;
    engine_control_->setOptionIndex(target);
  }

  static void formatParameterLine(const Parameter& p, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    const char* label = p.label() ? p.label() : "Param";

    if (p.hasOptions()) {
      const char* opt = p.optionLabel();
      std::snprintf(out, out_size, "%s %s", label, opt ? opt : "-");
      return;
    }

    const char* unit = p.unit();
    const float v = p.value();
    if (unit && unit[0]) {
      if (std::fabs(p.step()) >= 1.0f) std::snprintf(out, out_size, "%s %.0f%s", label, v, unit);
      else std::snprintf(out, out_size, "%s %.2f%s", label, v, unit);
      return;
    }

    if (std::fabs(p.step()) >= 1.0f) std::snprintf(out, out_size, "%s %.0f", label, v);
    else std::snprintf(out, out_size, "%s %.2f", label, v);
  }

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_ = 0;
  std::vector<std::string> synth_engine_options_;
  std::shared_ptr<LabelOptionComponent> engine_control_;
  int selected_row_ = 0;
};
} // namespace

SynthSequencerPage::SynthSequencerPage(IGfx& gfx,
                                       MiniAcid& mini_acid,
                                       AudioGuard audio_guard,
                                       int voice_index)
    : voice_index_(voice_index) {
  fallback_title_ = (voice_index_ == 0) ? "SYNTH A SETTINGS" : "SYNTH B SETTINGS";

  pattern_page_ = std::make_shared<PatternEditPage>(gfx, mini_acid, audio_guard, voice_index_);
  settings_page_ = std::make_shared<GlobalSynthSettingsPage>(mini_acid, audio_guard, voice_index_);
  addPage(pattern_page_);
  addPage(settings_page_);
}

bool SynthSequencerPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN && UIInput::isTab(ui_event)) {
    const uint32_t now = millis();
    // Ignore key-repeat bounce for TAB so one physical press == one section step.
    if (last_tab_switch_ms_ != 0 && (now - last_tab_switch_ms_) < 250u) return true;
    last_tab_switch_ms_ = now;

    if (!stepActivePage(1)) return false;
    const bool patternActive = (activePageIndex() == 0);
    char toast[40];
    std::snprintf(toast, sizeof(toast), "SYNTH %c: %s",
                  (voice_index_ == 0 ? 'A' : 'B'),
                  patternActive ? "PATTERN" : "SETTINGS");
    UI::showToast(toast, 900);
    return true;
  }
  return MultiPage::handleEvent(ui_event);
}

const std::string& SynthSequencerPage::getTitle() const {
  if (activePageIndex() == 0 && pattern_page_) return pattern_page_->getTitle();
  return fallback_title_;
}

void SynthSequencerPage::setContext(int context) {
  setActivePageIndex(0);
  if (pattern_page_) pattern_page_->setContext(context);
}

void SynthSequencerPage::setVisualStyle(VisualStyle style) {
  if (pattern_page_) pattern_page_->setVisualStyle(style);
}

void SynthSequencerPage::tick() {
  if (activePageIndex() == 0 && pattern_page_) {
    pattern_page_->tick();
  }
}

std::unique_ptr<MultiPageHelpDialog> SynthSequencerPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int SynthSequencerPage::getHelpFrameCount() const {
  return 2;
}

void SynthSequencerPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPage303PatternEdit(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    case 1:
      drawHelpPage303(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}
