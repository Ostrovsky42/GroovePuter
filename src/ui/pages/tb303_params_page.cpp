#include "src/state/scene_revision.h"
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "../../../platform_sdl/arduino_compat.h"
#endif
#include "tb303_params_page.h"
#include "../ui_common.h"
#include "../ui_utils.h"
#include "../../debug_log.h"
#include "../key_normalize.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "../help_dialog_frames.h"
#include "../layout_manager.h"
#include "../screen_geometry.h"
#include "../ui_colors.h"
#include "../ui_input.h"

namespace {
inline constexpr IGfxColor kDimText = IGfxColor(0x808080);
inline constexpr int kKnobStepCoarse = 5;
inline constexpr int kKnobStepFine = 1;

inline IGfxColor voiceColor(int voiceIndex) {
  return (voiceIndex == 0) ? IGfxColor(0x33C8FF) : IGfxColor(0xFF4FCB);
}

inline std::string upperCopy(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return out;
}

inline bool isDisabledSynthEngine(const std::string& value) {
  const std::string upper = upperCopy(value);
  return upper == "OPL2" || upper == "YM3812";
}

inline void removeDisabledSynthEngines(std::vector<std::string>& options) {
  options.erase(std::remove_if(options.begin(), options.end(), isDisabledSynthEngine),
                options.end());
}

inline int findEngineIndex(const std::vector<std::string>& options,
                           const std::string& current) {
  if (options.empty()) return -1;
  const std::string target = upperCopy(current);
  for (int i = 0; i < static_cast<int>(options.size()); ++i) {
    if (upperCopy(options[i]) == target) return i;
  }
  return -1;
}

inline void appendEngineIfMissing(std::vector<std::string>& options,
                                  const char* engine) {
  if (!engine || findEngineIndex(options, engine) >= 0) return;
  options.emplace_back(engine);
}

std::vector<std::string> availableSynthEngines(MiniAcid& engine) {
  std::vector<std::string> options = engine.getAvailableSynthEngines();
  removeDisabledSynthEngines(options);
  if (options.empty()) options = {"TB303", "SID", "AY", "SH101", "SN76489"};
  appendEngineIfMissing(options, "SH101");
  appendEngineIfMissing(options, "SN76489");
  return options;
}

void formatParameterValue(const Parameter& parameter,
                          char* destination,
                          size_t capacity) {
  if (!destination || capacity == 0) return;
  if (parameter.hasOptions()) {
    const char* option = parameter.optionLabel();
    std::snprintf(destination, capacity, "%s", option ? option : "-");
    return;
  }

  const char* unit = parameter.unit();
  if (unit && unit[0]) {
    if (std::fabs(parameter.step()) >= 1.0f) {
      std::snprintf(destination, capacity, "%.0f%s", parameter.value(), unit);
    } else {
      std::snprintf(destination, capacity, "%.2f%s", parameter.value(), unit);
    }
    return;
  }

  if (std::fabs(parameter.step()) >= 1.0f) {
    std::snprintf(destination, capacity, "%.0f", parameter.value());
  } else {
    std::snprintf(destination, capacity, "%.2f", parameter.value());
  }
}
} // namespace

class TB303ParamsPage::KnobComponent : public FocusableComponent {
 public:
  KnobComponent(MiniAcid& engine,
                int voice_index,
                int knob_index,
                IGfxColor ring_color,
                IGfxColor indicator_color,
                IGfxColor focus_color)
      : engine_(engine),
        voice_index_(voice_index),
        knob_index_(knob_index),
        ring_color_(ring_color),
        indicator_color_(indicator_color),
        focus_color_(focus_color) {}

  void setValue(int direction) {
    engine_.adjustSynthParameter(voice_index_, knob_index_, direction);
    GroovePuterState::markSceneMutated();
  }

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type == GROOVEPUTER_MOUSE_DOWN) {
      if (ui_event.button != MOUSE_BUTTON_LEFT) return false;
      if (!contains(ui_event.x, ui_event.y)) return false;
      dragging_ = true;
      last_drag_y_ = ui_event.y;
      drag_accum_ = 0;
      return true;
    }

    if (ui_event.event_type == GROOVEPUTER_MOUSE_UP) {
      if (!dragging_) return false;
      dragging_ = false;
      drag_accum_ = 0;
      return true;
    }

    if (ui_event.event_type == GROOVEPUTER_MOUSE_DRAG) {
      if (!dragging_) return false;
      int delta = ui_event.dy;
      if (delta == 0) delta = ui_event.y - last_drag_y_;
      last_drag_y_ = ui_event.y;
      drag_accum_ += delta;
      constexpr int kPixelsPerStep = 4;
      while (drag_accum_ <= -kPixelsPerStep) {
        setValue(1);
        drag_accum_ += kPixelsPerStep;
      }
      while (drag_accum_ >= kPixelsPerStep) {
        setValue(-1);
        drag_accum_ -= kPixelsPerStep;
      }
      return true;
    }

    if (ui_event.event_type == GROOVEPUTER_MOUSE_SCROLL) {
      if (!contains(ui_event.x, ui_event.y)) return false;
      if (ui_event.wheel_dy > 0) {
        setValue(1);
        return true;
      }
      if (ui_event.wheel_dy < 0) {
        setValue(-1);
        return true;
      }
    }

    return false;
  }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    if (bounds.w <= 0 || bounds.h <= 0) return;

    const int radius = std::min(bounds.w, bounds.h) / 2;
    const int cx = bounds.x + bounds.w / 2;
    const int cy = bounds.y + bounds.h / 2;

    const Parameter& parameter = engine_.synthParameter(voice_index_, knob_index_);
    const float normalized = std::clamp(parameter.normalized(), 0.0f, 1.0f);

    gfx.drawKnobFace(cx, cy, radius, ring_color_, COLOR_BLACK);

    constexpr float kDegToRad = 3.14159265f / 180.0f;
    float degrees = 135.0f + normalized * 270.0f;
    if (degrees >= 360.0f) degrees -= 360.0f;
    const float angle = degrees * kDegToRad;

    const int indicatorX = cx + static_cast<int>(roundf(cosf(angle) * (radius - 2)));
    const int indicatorY = cy + static_cast<int>(roundf(sinf(angle) * (radius - 2)));
    drawLineColored(gfx, cx, cy, indicatorX, indicatorY, indicator_color_);

    const char* label = parameter.label() ? parameter.label() : "";
    gfx.setTextColor(kDimText);
    gfx.drawText(cx - gfx.textWidth(label) / 2, cy + radius + 4, label);

    char value[24];
    formatParameterValue(parameter, value, sizeof(value));
    gfx.setTextColor(IGfxColor::White());
    gfx.drawText(cx - gfx.textWidth(value) / 2, cy - radius - 10, value);

    if (isFocused()) {
      constexpr int kFocusPadding = 3;
      gfx.drawRect(bounds.x - kFocusPadding,
                   bounds.y - kFocusPadding,
                   bounds.w + kFocusPadding * 2,
                   bounds.h + kFocusPadding * 2,
                   focus_color_);
    }
  }

 private:
  MiniAcid& engine_;
  int voice_index_;
  int knob_index_;
  IGfxColor ring_color_;
  IGfxColor indicator_color_;
  IGfxColor focus_color_;
  bool dragging_ = false;
  int last_drag_y_ = 0;
  int drag_accum_ = 0;
};

class TB303ParamsPage::LabelValueComponent : public FocusableComponent {
 public:
  enum class Style : uint8_t {
    Stepper,
    Toggle,
  };

  LabelValueComponent(const char* label,
                      IGfxColor label_color,
                      IGfxColor value_color,
                      IGfxColor focus_color,
                      Style style)
      : label_(label ? label : ""),
        label_color_(label_color),
        value_color_(value_color),
        focus_color_(focus_color),
        style_(style) {}

  void setLabel(const char* label) { label_ = label ? label : ""; }
  void setValue(const char* value) { value_ = value ? value : ""; }
  void setToggle(bool enabled) { toggled_ = enabled; }
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    if (bounds.w <= 0 || bounds.h <= 0) return;

    const bool focused = isFocused() && enabled_;
    if (focused) {
      gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, focus_color_);
    }

    const IGfxColor labelColor = !enabled_
        ? kDimText
        : (focused ? IGfxColor::Black() : label_color_);
    const IGfxColor valueColor = !enabled_
        ? kDimText
        : (focused ? IGfxColor::Black() : value_color_);
    const int textY = bounds.y + (bounds.h - gfx.fontHeight()) / 2;

    gfx.setTextColor(labelColor);
    gfx.drawText(bounds.x + 6, textY, label_.c_str());

    if (!enabled_) {
      // Unavailable is distinct from an available OFF toggle: no track,
      // thumb, or stepper arrows are rendered for this row.
      gfx.setTextColor(kDimText);
      gfx.drawText(bounds.x + bounds.w - gfx.textWidth("--") - 8, textY, "--");
      return;
    }

    if (style_ == Style::Stepper) {
      char compactValue[24]{};
      std::snprintf(compactValue, sizeof(compactValue), "%s", value_.c_str());
      std::size_t valueLength = std::strlen(compactValue);
      const int maxValueWidth = std::max(18, bounds.w / 2 - 22);
      while (valueLength > 1 && gfx.textWidth(compactValue) > maxValueWidth) {
        compactValue[--valueLength] = '\0';
      }

      const int rightArrowX = bounds.x + bounds.w - gfx.textWidth(">") - 7;
      const int valueRight = rightArrowX - 7;
      const int valueX = valueRight - gfx.textWidth(compactValue);
      const int leftArrowX = valueX - gfx.textWidth("<") - 7;

      gfx.setTextColor(valueColor);
      gfx.drawText(leftArrowX, textY, "<");
      gfx.drawText(valueX, textY, compactValue);
      gfx.drawText(rightArrowX, textY, ">");
      return;
    }

    constexpr int kTrackWidth = 30;
    constexpr int kTrackHeight = 10;
    const int trackX = bounds.x + bounds.w - kTrackWidth - 7;
    const int trackY = bounds.y + (bounds.h - kTrackHeight) / 2;
    const IGfxColor switchColor = focused ? IGfxColor::Black() : focus_color_;
    const IGfxColor knobColor = focused ? focus_color_ : IGfxColor::Black();

    if (toggled_) {
      gfx.fillRect(trackX, trackY, kTrackWidth, kTrackHeight, switchColor);
    } else {
      gfx.drawRect(trackX, trackY, kTrackWidth, kTrackHeight, switchColor);
    }
    const int knobX = toggled_ ? trackX + kTrackWidth - 5 : trackX + 5;
    gfx.fillCircle(knobX, trackY + kTrackHeight / 2, 4,
                   toggled_ ? knobColor : switchColor);
  }

 private:
  std::string label_;
  std::string value_;
  IGfxColor label_color_;
  IGfxColor value_color_;
  IGfxColor focus_color_;
  Style style_;
  bool toggled_{false};
  bool enabled_{true};
};

TB303ParamsPage::TB303ParamsPage(IGfx& gfx,
                                 MiniAcid& mini_acid,
                                 AudioGuard audio_guard,
                                 int voice_index)
    : gfx_(gfx),
      mini_acid_(mini_acid),
      audio_guard_(audio_guard),
      voice_index_(voice_index) {
  title_ = (voice_index_ == 0) ? "SYNTH A PARAMS" : "SYNTH B PARAMS";
}

void TB303ParamsPage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) initComponents();
}

void TB303ParamsPage::initComponents() {
  const IGfxColor focusColor = voiceColor(voice_index_);

  cutoff_knob_ = std::make_shared<KnobComponent>(mini_acid_, voice_index_, 0, COLOR_KNOB_1, COLOR_KNOB_1, focusColor);
  resonance_knob_ = std::make_shared<KnobComponent>(mini_acid_, voice_index_, 1, COLOR_KNOB_2, COLOR_KNOB_2, focusColor);
  env_amount_knob_ = std::make_shared<KnobComponent>(mini_acid_, voice_index_, 2, COLOR_KNOB_3, COLOR_KNOB_3, focusColor);
  env_decay_knob_ = std::make_shared<KnobComponent>(mini_acid_, voice_index_, 3, COLOR_KNOB_4, COLOR_KNOB_4, focusColor);

  engine_type_control_ = std::make_shared<LabelValueComponent>("TYPE", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::Stepper);
  osc_control_ = std::make_shared<LabelValueComponent>("OSC", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::Stepper);
  filter_control_ = std::make_shared<LabelValueComponent>("FLT", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::Stepper);
  distortion_control_ = std::make_shared<LabelValueComponent>("DST", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::Toggle);
  delay_control_ = std::make_shared<LabelValueComponent>("DLY", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::Toggle);

  addChild(cutoff_knob_);
  addChild(resonance_knob_);
  addChild(env_amount_knob_);
  addChild(env_decay_knob_);
  addChild(engine_type_control_);
  addChild(osc_control_);
  addChild(filter_control_);
  addChild(distortion_control_);
  addChild(delay_control_);

  initialized_ = true;
}

bool TB303ParamsPage::isTb303Engine() const {
  return upperCopy(mini_acid_.currentSynthEngineName(voice_index_)) == "TB303";
}

void TB303ParamsPage::cycleEngine(int direction) {
  std::vector<std::string> options = availableSynthEngines(mini_acid_);
  if (options.empty()) return;

  int index = findEngineIndex(options, mini_acid_.currentSynthEngineName(voice_index_));
  if (index < 0) index = 0;
  index = (index + (direction >= 0 ? 1 : -1)) % static_cast<int>(options.size());
  if (index < 0) index += static_cast<int>(options.size());

  const std::string selected = options[index];
  withAudioGuard([&]() { mini_acid_.setSynthEngine(voice_index_, selected); });

  char toast[32];
  std::snprintf(toast, sizeof(toast), "SYNTH %c: %s",
                voice_index_ == 0 ? 'A' : 'B', selected.c_str());
  UI::showToast(toast, 800);
}

void TB303ParamsPage::adjustGenericParameter(int parameterIndex,
                                             int direction,
                                             bool fine) {
  if (parameterIndex < 0 ||
      parameterIndex >= static_cast<int>(mini_acid_.synthParameterCount(voice_index_))) {
    return;
  }

  const Parameter& parameter = mini_acid_.synthParameter(voice_index_, parameterIndex);
  const int magnitude = (fine || parameter.hasOptions()) ? 1 : kKnobStepCoarse;
  withAudioGuard([&]() {
    mini_acid_.adjustSynthParameter(voice_index_, parameterIndex,
                                    direction * magnitude);
  });
}

void TB303ParamsPage::layoutComponents() {
  const auto& content = Layout::CONTENT;
  const int x0 = content.x + Layout::CONTENT_PAD_X;
  const int width = content.w - Layout::CONTENT_PAD_X * 2;

  cutoff_knob_->setBoundaries(Rect{0, 0, 0, 0});
  resonance_knob_->setBoundaries(Rect{0, 0, 0, 0});
  env_amount_knob_->setBoundaries(Rect{0, 0, 0, 0});
  env_decay_knob_->setBoundaries(Rect{0, 0, 0, 0});
  engine_type_control_->setBoundaries(Rect{0, 0, 0, 0});
  osc_control_->setBoundaries(Rect{0, 0, 0, 0});
  filter_control_->setBoundaries(Rect{0, 0, 0, 0});
  distortion_control_->setBoundaries(Rect{0, 0, 0, 0});
  delay_control_->setBoundaries(Rect{0, 0, 0, 0});

  const std::string engineName = mini_acid_.currentSynthEngineName(voice_index_);
  const bool tb303 = isTb303Engine();
  const int parameterCount = mini_acid_.synthParameterCount(voice_index_);

  engine_type_control_->setLabel("TYPE");
  engine_type_control_->setValue(engineName.c_str());
  engine_type_control_->setEnabled(true);

  const bool oscAvailable = tb303 || parameterCount > 4;
  const bool filterAvailable = tb303 || parameterCount > 5;
  char value[24]{};
  std::string label;

  if (tb303) {
    const Parameter& oscillator = mini_acid_.parameter303(TB303ParamId::Oscillator, voice_index_);
    const Parameter& filter = mini_acid_.parameter303(TB303ParamId::FilterType, voice_index_);
    osc_control_->setLabel("OSC");
    osc_control_->setValue(oscillator.optionLabel());
    filter_control_->setLabel("FLT");
    filter_control_->setValue(filter.optionLabel());
  } else {
    if (oscAvailable) {
      const Parameter& parameter = mini_acid_.synthParameter(voice_index_, 4);
      label = parameter.label() ? parameter.label() : "P5";
      formatParameterValue(parameter, value, sizeof(value));
      osc_control_->setLabel(label.c_str());
      osc_control_->setValue(value);
    } else {
      osc_control_->setLabel("P5");
      osc_control_->setValue("--");
    }

    if (filterAvailable) {
      const Parameter& parameter = mini_acid_.synthParameter(voice_index_, 5);
      label = parameter.label() ? parameter.label() : "P6";
      formatParameterValue(parameter, value, sizeof(value));
      filter_control_->setLabel(label.c_str());
      filter_control_->setValue(value);
    } else {
      filter_control_->setLabel("P6");
      filter_control_->setValue("--");
    }
  }
  osc_control_->setEnabled(oscAvailable);
  filter_control_->setEnabled(filterAvailable);

  const bool distortionEnabled = mini_acid_.is303DistortionEnabled(voice_index_);
  distortion_control_->setLabel("DST");
  distortion_control_->setValue(distortionEnabled ? "ON" : "OFF");
  distortion_control_->setToggle(distortionEnabled);

  const bool delayEnabled = mini_acid_.is303DelayEnabled(voice_index_);
  delay_control_->setLabel("DLY");
  delay_control_->setValue(delayEnabled ? "ON" : "OFF");
  delay_control_->setToggle(delayEnabled);

  // Both effects are per-voice post-engine stages in MiniAcid, so they are
  // available for every currently selectable synth engine.
  distortion_control_->setEnabled(true);
  delay_control_->setEnabled(true);

  if (!more_tab_) {
    // Vertical budget inside the 103 px content area:
    // tab segment ends at +15, value text starts at +17,
    // the R18 circle spans +27..+63, label starts at +67,
    // key hint starts at +75, and the summary starts at +89.
    constexpr int kMainKnobRadius = 18;
    const int knobRowY = content.y + 45;
    const int spacing = width / 5;

    cutoff_knob_->setBoundaries(Rect{x0 + spacing * 1 - kMainKnobRadius,
                                     knobRowY - kMainKnobRadius,
                                     kMainKnobRadius * 2,
                                     kMainKnobRadius * 2});
    resonance_knob_->setBoundaries(Rect{x0 + spacing * 2 - kMainKnobRadius,
                                        knobRowY - kMainKnobRadius,
                                        kMainKnobRadius * 2,
                                        kMainKnobRadius * 2});
    env_amount_knob_->setBoundaries(Rect{x0 + spacing * 3 - kMainKnobRadius,
                                         knobRowY - kMainKnobRadius,
                                         kMainKnobRadius * 2,
                                         kMainKnobRadius * 2});
    env_decay_knob_->setBoundaries(Rect{x0 + spacing * 4 - kMainKnobRadius,
                                        knobRowY - kMainKnobRadius,
                                        kMainKnobRadius * 2,
                                        kMainKnobRadius * 2});
  } else {
    constexpr int kMoreRowY = 20;
    constexpr int kMoreRowHeight = 15;
    constexpr int kMoreRowGap = 1;
    LabelValueComponent* rows[5] = {
        engine_type_control_.get(),
        osc_control_.get(),
        filter_control_.get(),
        distortion_control_.get(),
        delay_control_.get(),
    };
    for (int i = 0; i < 5; ++i) {
      rows[i]->setBoundaries(Rect{x0 + 4,
                                  content.y + kMoreRowY + i * (kMoreRowHeight + kMoreRowGap),
                                  width - 8,
                                  kMoreRowHeight});
    }
  }

  updateTabFocusability();
  if (focusedChild() && !focusedChild()->isFocusable()) {
    restoreFocusedSlot();
  }
}

void TB303ParamsPage::focusComponent(Component* component) {
  if (!component || !component->isFocusable()) return;
  const int limit = static_cast<int>(getChildren().size()) + 1;
  for (int i = 0; i < limit; ++i) {
    if (focusedChild() == component) return;
    focusNext();
  }
}

void TB303ParamsPage::rememberFocusedSlot() {
  Component* focused = focusedChild();
  Component* mainControls[4] = {
      cutoff_knob_.get(),
      resonance_knob_.get(),
      env_amount_knob_.get(),
      env_decay_knob_.get(),
  };
  Component* moreControls[5] = {
      engine_type_control_.get(),
      osc_control_.get(),
      filter_control_.get(),
      distortion_control_.get(),
      delay_control_.get(),
  };

  if (!more_tab_) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (focused == mainControls[i]) {
        main_focus_slot_ = i;
        return;
      }
    }
    return;
  }

  for (uint8_t i = 0; i < 5; ++i) {
    if (focused == moreControls[i]) {
      more_focus_slot_ = i;
      return;
    }
  }
}

void TB303ParamsPage::restoreFocusedSlot() {
  Component* mainControls[4] = {
      cutoff_knob_.get(),
      resonance_knob_.get(),
      env_amount_knob_.get(),
      env_decay_knob_.get(),
  };
  Component* moreControls[5] = {
      engine_type_control_.get(),
      osc_control_.get(),
      filter_control_.get(),
      distortion_control_.get(),
      delay_control_.get(),
  };

  if (!more_tab_) {
    const uint8_t slot = std::min<uint8_t>(main_focus_slot_, 3);
    focusComponent(mainControls[slot]);
    return;
  }

  const uint8_t slot = std::min<uint8_t>(more_focus_slot_, 4);
  Component* target = moreControls[slot];
  if (!target->isFocusable()) {
    target = nullptr;
    for (Component* candidate : moreControls) {
      if (candidate->isFocusable()) {
        target = candidate;
        break;
      }
    }
  }
  focusComponent(target);
}

void TB303ParamsPage::updateTabFocusability() {
  const bool main = !more_tab_;
  cutoff_knob_->setFocusable(main);
  resonance_knob_->setFocusable(main);
  env_amount_knob_->setFocusable(main);
  env_decay_knob_->setFocusable(main);

  engine_type_control_->setFocusable(more_tab_ && engine_type_control_->enabled());
  osc_control_->setFocusable(more_tab_ && osc_control_->enabled());
  filter_control_->setFocusable(more_tab_ && filter_control_->enabled());
  distortion_control_->setFocusable(more_tab_ && distortion_control_->enabled());
  delay_control_->setFocusable(more_tab_ && delay_control_->enabled());
}

void TB303ParamsPage::setActiveTab(bool more) {
  if (more_tab_ == more) return;
  rememberFocusedSlot();
  more_tab_ = more;
  updateTabFocusability();
  restoreFocusedSlot();
}

void TB303ParamsPage::drawTabSwitcher(IGfx& gfx, const Rect& content) {
  constexpr int kSegmentWidth = 38;
  constexpr int kSegmentHeight = 14;
  constexpr int kSegmentGap = 2;
  const int totalWidth = kSegmentWidth * 2 + kSegmentGap;
  const int x = content.x + (content.w - totalWidth) / 2;
  const int y = content.y + 1;
  const IGfxColor accent = voiceColor(voice_index_);

  auto drawSegment = [&](int segmentX, const char* label, bool active) {
    if (active) {
      gfx.fillRect(segmentX, y, kSegmentWidth, kSegmentHeight, accent);
    } else {
      gfx.drawRect(segmentX, y, kSegmentWidth, kSegmentHeight, accent);
    }
    gfx.setTextColor(active ? IGfxColor::Black() : kDimText);
    gfx.drawText(segmentX + (kSegmentWidth - gfx.textWidth(label)) / 2,
                 y + (kSegmentHeight - gfx.fontHeight()) / 2,
                 label);
  };

  drawSegment(x, "MAIN", !more_tab_);
  drawSegment(x + kSegmentWidth + kSegmentGap, "MORE", more_tab_);
}

void TB303ParamsPage::drawMainSummary(IGfx& gfx, const Rect& content) {
  char first[20] = "--";
  char second[20] = "--";
  const bool tb303 = isTb303Engine();
  const int parameterCount = mini_acid_.synthParameterCount(voice_index_);

  if (tb303) {
    const char* osc = mini_acid_.parameter303(TB303ParamId::Oscillator, voice_index_).optionLabel();
    const char* filter = mini_acid_.parameter303(TB303ParamId::FilterType, voice_index_).optionLabel();
    std::snprintf(first, sizeof(first), "%s", osc ? osc : "--");
    std::snprintf(second, sizeof(second), "%s", filter ? filter : "--");
  } else {
    if (parameterCount > 4) {
      formatParameterValue(mini_acid_.synthParameter(voice_index_, 4), first, sizeof(first));
    }
    if (parameterCount > 5) {
      formatParameterValue(mini_acid_.synthParameter(voice_index_, 5), second, sizeof(second));
    }
  }

  char summary[64]{};
  const std::string engineName = mini_acid_.currentSynthEngineName(voice_index_);
  std::snprintf(summary, sizeof(summary), "%s %s %s",
                engineName.c_str(), first, second);

  const int y = content.y + 89;
  const int tabHintX = content.x + content.w - gfx.textWidth("TAB >") - 4;
  constexpr int kBadgeWidth = 29;
  constexpr int kBadgeHeight = 11;
  constexpr int kBadgeGap = 3;
  const int dlyX = tabHintX - kBadgeGap - kBadgeWidth;
  const int dstX = dlyX - kBadgeGap - kBadgeWidth;
  const int summaryX = content.x + 4;
  const int summaryWidth = std::max(1, dstX - summaryX - 4);
  std::size_t summaryLength = std::strlen(summary);
  while (summaryLength > 1 && gfx.textWidth(summary) > summaryWidth) {
    summary[--summaryLength] = '\0';
  }

  const IGfxColor accent = voiceColor(voice_index_);
  gfx.setTextColor(accent);
  gfx.drawText(summaryX, y + 2, summary);

  auto drawBadge = [&](int x, const char* label, bool active) {
    if (active) {
      gfx.fillRect(x, y, kBadgeWidth, kBadgeHeight, accent);
    } else {
      gfx.drawRect(x, y, kBadgeWidth, kBadgeHeight, accent);
    }
    gfx.setTextColor(active ? IGfxColor::Black() : accent);
    gfx.drawText(x + (kBadgeWidth - gfx.textWidth(label)) / 2, y + 2, label);
  };

  drawBadge(dstX, "DST", mini_acid_.is303DistortionEnabled(voice_index_));
  drawBadge(dlyX, "DLY", mini_acid_.is303DelayEnabled(voice_index_));
  gfx.setTextColor(kDimText);
  gfx.drawText(tabHintX, y + 2, "TAB >");
}

void TB303ParamsPage::adjustFocusedElement(int direction, bool fine) {
  const int step = fine ? kKnobStepFine : kKnobStepCoarse;

  if (cutoff_knob_ && cutoff_knob_->isFocused()) {
    cutoff_knob_->setValue(direction * step);
    return;
  }
  if (resonance_knob_ && resonance_knob_->isFocused()) {
    resonance_knob_->setValue(direction * step);
    return;
  }
  if (env_amount_knob_ && env_amount_knob_->isFocused()) {
    env_amount_knob_->setValue(direction * step);
    return;
  }
  if (env_decay_knob_ && env_decay_knob_->isFocused()) {
    env_decay_knob_->setValue(direction * step);
    return;
  }
  if (engine_type_control_ && engine_type_control_->isFocused()) {
    cycleEngine(direction);
    return;
  }
  if (osc_control_ && osc_control_->isFocused()) {
    if (isTb303Engine()) {
      withAudioGuard([&]() {
        mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, direction, voice_index_);
      });
    } else {
      adjustGenericParameter(4, direction, fine);
    }
    return;
  }
  if (filter_control_ && filter_control_->isFocused()) {
    if (isTb303Engine()) {
      withAudioGuard([&]() {
        mini_acid_.adjust303Parameter(TB303ParamId::FilterType, direction, voice_index_);
      });
    } else {
      adjustGenericParameter(5, direction, fine);
    }
    return;
  }
  if (distortion_control_ && distortion_control_->isFocused()) {
    const bool enabled = mini_acid_.is303DistortionEnabled(voice_index_);
    if ((direction > 0 && !enabled) || (direction < 0 && enabled)) {
      withAudioGuard([&]() { mini_acid_.toggleDistortion303(voice_index_); });
    }
    return;
  }
  if (delay_control_ && delay_control_->isFocused()) {
    const bool enabled = mini_acid_.is303DelayEnabled(voice_index_);
    if ((direction > 0 && !enabled) || (direction < 0 && enabled)) {
      withAudioGuard([&]() { mini_acid_.toggleDelay303(voice_index_); });
    }
  }
}

void TB303ParamsPage::draw(IGfx& gfx) {
  if (!initialized_) initComponents();

  UI::drawStandardHeader(gfx, mini_acid_, title_.c_str());
  LayoutManager::clearContent(gfx);
  layoutComponents();

  const char* modeName = "MIN";
  switch (mini_acid_.grooveboxMode()) {
    case GrooveboxMode::Acid: modeName = "ACID"; break;
    case GrooveboxMode::Minimal: modeName = "MIN"; break;
    case GrooveboxMode::Breaks: modeName = "BRK"; break;
    case GrooveboxMode::Dub: modeName = "DUB"; break;
    case GrooveboxMode::Electro: modeName = "ELC"; break;
    default: break;
  }

  const auto& content = Layout::CONTENT;
  const Rect contentRect{content.x, content.y, content.w, content.h};
  drawTabSwitcher(gfx, contentRect);
  gfx.setTextColor(kDimText);
  gfx.drawText(content.x + content.w - gfx.textWidth(modeName) - 4,
               content.y + 3,
               modeName);

  if (!more_tab_) {
    const int x0 = content.x + Layout::CONTENT_PAD_X;
    const int width = content.w - Layout::CONTENT_PAD_X * 2;
    const int spacing = width / 5;
    const char* keyHints[4] = {"A/Z", "S/X", "D/C", "F/V"};
    const int keyY = content.y + 75;
    gfx.setTextColor(kDimText);
    for (int i = 0; i < 4; ++i) {
      const int cx = x0 + spacing * (i + 1);
      gfx.drawText(cx - gfx.textWidth(keyHints[i]) / 2, keyY, keyHints[i]);
    }
    drawMainSummary(gfx, contentRect);
  }

  Container::draw(gfx_);

  if (!more_tab_) {
    UI::drawStandardFooter(gfx,
                           "[TAB]MORE [L/R]FOCUS [U/D]VAL",
                           "HOLD:ACCEL [CTRL]FINE");
  } else {
    UI::drawStandardFooter(gfx,
                           "[TAB]MAIN [U/D]ROW [L/R]CHANGE",
                           "TYPE OSC FLT DST DLY");
  }
}

const std::string& TB303ParamsPage::getTitle() const {
  return title_;
}

bool TB303ParamsPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) {
    return Container::handleEvent(ui_event);
  }

  static UIInput::HoldAccelerator knobAccelerator;

  if (UIInput::isGlobalNav(ui_event)) return false;
  if (UIInput::isTab(ui_event)) {
    if (ui_event.ctrl || ui_event.alt || ui_event.meta) return false;
    knobAccelerator.reset();
    setActiveTab(!more_tab_);
    return true;
  }

  const int nav = UIInput::navCode(ui_event);
  const bool fine = ui_event.shift || ui_event.ctrl;
  if (more_tab_) {
    // MORE is a vertical list: Up/Down selects a row and Left/Right edits it.
    switch (nav) {
      case GROOVEPUTER_UP: knobAccelerator.reset(); focusPrev(); return true;
      case GROOVEPUTER_DOWN: knobAccelerator.reset(); focusNext(); return true;
      case GROOVEPUTER_LEFT: knobAccelerator.reset(); adjustFocusedElement(-1, fine); return true;
      case GROOVEPUTER_RIGHT: knobAccelerator.reset(); adjustFocusedElement(1, fine); return true;
      default: break;
    }
  } else {
    // MAIN is a horizontal row of knobs: Left/Right selects and Up/Down edits.
    switch (nav) {
      case GROOVEPUTER_LEFT: knobAccelerator.reset(); focusPrev(); return true;
      case GROOVEPUTER_RIGHT: knobAccelerator.reset(); focusNext(); return true;
      case GROOVEPUTER_UP: {
        const int multiplier = fine ? 1 : knobAccelerator.multiplier(1);
        if (fine) knobAccelerator.reset();
        adjustFocusedElement(multiplier, fine);
        return true;
      }
      case GROOVEPUTER_DOWN: {
        const int multiplier = fine ? 1 : knobAccelerator.multiplier(-1);
        if (fine) knobAccelerator.reset();
        adjustFocusedElement(-multiplier, fine);
        return true;
      }
      default: break;
    }
  }

  const char key = ui_event.key;
  if (!key) return Container::handleEvent(ui_event);
  knobAccelerator.reset();
  const char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));

  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    if (key == '[' || key == '{') {
      cycleEngine(-1);
      return true;
    }
    if (key == ']' || key == '}') {
      cycleEngine(1);
      return true;
    }
  }

  if (isTb303Engine() && ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    if (lowerKey == 'z') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::Cutoff, 800.0f, voice_index_); }); return true; }
    if (lowerKey == 'x') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::Resonance, 0.0f, voice_index_); }); return true; }
    if (lowerKey == 'c') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::EnvAmount, 400.0f, voice_index_); }); return true; }
    if (lowerKey == 'v') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::EnvDecay, 420.0f, voice_index_); }); return true; }
  }

  if (ui_event.ctrl && !ui_event.alt && key >= '1' && key <= '2') {
    const int bankIndex = key - '1';
    withAudioGuard([&]() { mini_acid_.set303BankIndex(voice_index_, bankIndex); });
    UI::showToast(bankIndex == 0 ? "Bank: A" : "Bank: B", 800);
    return true;
  }

  const bool mainKnobKey = lowerKey == 'a' || lowerKey == 'z' ||
      lowerKey == 's' || lowerKey == 'x' ||
      lowerKey == 'd' || lowerKey == 'c' ||
      lowerKey == 'f' || lowerKey == 'v';
  if (more_tab_ && !ui_event.ctrl && !ui_event.alt && !ui_event.meta && mainKnobKey) {
    return true;
  }

  if (!ui_event.ctrl && !ui_event.meta) {
    const int patternIndex = qwertyToPatternIndex(lowerKey);
    if (patternIndex >= 0) {
      LOG_DEBUG_UI("Synth Pattern Select: %d", patternIndex);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, patternIndex); });
      char toast[32];
      std::snprintf(toast, sizeof(toast), "SYNTH %c -> Pat %d",
                    voice_index_ == 0 ? 'A' : 'B', patternIndex + 1);
      UI::showToast(toast, 800);
      return true;
    }
  }

  const int directKnobStep = fine ? kKnobStepFine : kKnobStepCoarse;

  switch (lowerKey) {
    case 't':
      if (isTb303Engine()) {
        withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, 1, voice_index_); });
      } else {
        adjustGenericParameter(4, 1, fine);
      }
      return true;
    case 'g':
      if (isTb303Engine()) {
        withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, -1, voice_index_); });
      } else {
        adjustGenericParameter(4, -1, fine);
      }
      return true;
    case 'y':
      if (isTb303Engine()) {
        withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::FilterType, 1, voice_index_); });
      } else {
        adjustGenericParameter(5, 1, fine);
      }
      return true;
    case 'h':
      if (isTb303Engine()) {
        withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::FilterType, -1, voice_index_); });
      } else {
        adjustGenericParameter(5, -1, fine);
      }
      return true;

    case 'a': if (cutoff_knob_) cutoff_knob_->setValue(directKnobStep); return true;
    case 'z': if (cutoff_knob_) cutoff_knob_->setValue(-directKnobStep); return true;
    case 's': if (resonance_knob_) resonance_knob_->setValue(directKnobStep); return true;
    case 'x': if (resonance_knob_) resonance_knob_->setValue(-directKnobStep); return true;
    case 'd': if (env_amount_knob_) env_amount_knob_->setValue(directKnobStep); return true;
    case 'c': if (env_amount_knob_) env_amount_knob_->setValue(-directKnobStep); return true;
    case 'f': if (env_decay_knob_) env_decay_knob_->setValue(directKnobStep); return true;
    case 'v': if (env_decay_knob_) env_decay_knob_->setValue(-directKnobStep); return true;

    case 'n':
      withAudioGuard([&]() { mini_acid_.toggleDistortion303(voice_index_); });
      return true;
    case 'm':
      withAudioGuard([&]() { mini_acid_.toggleDelay303(voice_index_); });
      return true;
    default:
      break;
  }

  return Container::handleEvent(ui_event);
}

void TB303ParamsPage::loadModePreset(int index) {
  withAudioGuard([&]() {
    mini_acid_.modeManager().apply303Preset(voice_index_, index);
    current_preset_index_ = index;
  });
}

std::unique_ptr<MultiPageHelpDialog> TB303ParamsPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int TB303ParamsPage::getHelpFrameCount() const {
  return 1;
}

void TB303ParamsPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  if (frameIndex == 0) {
    drawHelpPage303(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
  }
}
