#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE = ROOT / "src/ui/pages/tb303_params_page.cpp"
TEST = ROOT / "tests/test_wavemorph_performance_source_regressions.py"
DOC = ROOT / "docs/stages/COMPACT_SYNTH_CONTROLS_STAGE.md"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_block(text: str, start: str, end: str, replacement: str, label: str) -> str:
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"{label}: start marker not found")
    finish = text.find(end, begin)
    if finish < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:begin] + replacement + text[finish:]


page = PAGE.read_text(encoding="utf-8")

compact_component = r'''class TB303ParamsPage::LabelValueComponent : public FocusableComponent {
 public:
  enum class Style : uint8_t {
    SelectorKnob,
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
  void setNormalized(float normalized) {
    normalized_ = std::clamp(normalized, 0.0f, 1.0f);
  }
  void setToggle(bool enabled) {
    toggled_ = enabled;
    normalized_ = enabled ? 1.0f : 0.0f;
  }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    if (bounds.w <= 0 || bounds.h <= 0) return;

    auto drawCentered = [&](int y, const char* text, IGfxColor color) {
      if (!text) return;
      gfx.setTextColor(color);
      gfx.drawText(bounds.x + (bounds.w - gfx.textWidth(text)) / 2, y, text);
    };

    char compactValue[16]{};
    std::snprintf(compactValue, sizeof(compactValue), "%s", value_.c_str());
    std::size_t valueLength = std::strlen(compactValue);
    while (valueLength > 1 && gfx.textWidth(compactValue) > bounds.w - 2) {
      compactValue[--valueLength] = '\0';
    }

    drawCentered(bounds.y, label_.c_str(), label_color_);

    if (style_ == Style::SelectorKnob) {
      constexpr int kSelectorRadius = 7;
      const int radius = std::min(kSelectorRadius,
                                  std::max(3, std::min(bounds.w, bounds.h) / 4));
      const int cx = bounds.x + bounds.w / 2;
      const int cy = bounds.y + bounds.h / 2;
      gfx.drawKnobFace(cx, cy, radius, focus_color_, COLOR_BLACK);

      constexpr float kDegToRad = 3.14159265f / 180.0f;
      float degrees = 135.0f + normalized_ * 270.0f;
      if (degrees >= 360.0f) degrees -= 360.0f;
      const float angle = degrees * kDegToRad;
      const int indicatorX = cx + static_cast<int>(
          roundf(cosf(angle) * std::max(1, radius - 2)));
      const int indicatorY = cy + static_cast<int>(
          roundf(sinf(angle) * std::max(1, radius - 2)));
      drawLineColored(gfx, cx, cy, indicatorX, indicatorY, focus_color_);
    } else {
      constexpr int kTrackWidth = 24;
      constexpr int kTrackHeight = 8;
      const int trackX = bounds.x + (bounds.w - kTrackWidth) / 2;
      const int trackY = bounds.y + bounds.h / 2 - kTrackHeight / 2;
      const IGfxColor trackColor = toggled_ ? focus_color_ : IGfxColor::DarkGray();
      gfx.fillRect(trackX, trackY, kTrackWidth, kTrackHeight, trackColor);
      gfx.drawRect(trackX, trackY, kTrackWidth, kTrackHeight, IGfxColor::White());
      const int knobX = toggled_ ? trackX + kTrackWidth - 4 : trackX + 4;
      gfx.fillCircle(knobX, trackY + kTrackHeight / 2, 3, IGfxColor::White());
    }

    drawCentered(bounds.y + bounds.h - gfx.fontHeight(), compactValue, value_color_);

    if (isFocused()) {
      constexpr int kFocusPadding = 1;
      gfx.drawRect(bounds.x - kFocusPadding,
                   bounds.y - kFocusPadding,
                   bounds.w + kFocusPadding * 2,
                   bounds.h + kFocusPadding * 2,
                   focus_color_);
    }
  }

 private:
  std::string label_;
  std::string value_;
  IGfxColor label_color_;
  IGfxColor value_color_;
  IGfxColor focus_color_;
  Style style_;
  float normalized_{0.0f};
  bool toggled_{false};
};

'''

page = replace_block(
    page,
    "class TB303ParamsPage::LabelValueComponent : public FocusableComponent {",
    "TB303ParamsPage::TB303ParamsPage(",
    compact_component,
    "compact control component",
)

page = replace_once(
    page,
    'engine_type_control_ = std::make_shared<LabelValueComponent>("TYPE:", IGfxColor::White(), focusColor, focusColor);',
    'engine_type_control_ = std::make_shared<LabelValueComponent>("TYPE", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::SelectorKnob);',
    "engine selector construction",
)
page = replace_once(
    page,
    'osc_control_ = std::make_shared<LabelValueComponent>("OSC:", IGfxColor::White(), focusColor, focusColor);',
    'osc_control_ = std::make_shared<LabelValueComponent>("OSC", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::SelectorKnob);',
    "osc selector construction",
)
page = replace_once(
    page,
    'filter_control_ = std::make_shared<LabelValueComponent>("FLT:", IGfxColor::White(), focusColor, focusColor);',
    'filter_control_ = std::make_shared<LabelValueComponent>("FLT", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::SelectorKnob);',
    "filter selector construction",
)
page = replace_once(
    page,
    'distortion_control_ = std::make_shared<LabelValueComponent>("DST:", IGfxColor::White(), focusColor, focusColor);',
    'distortion_control_ = std::make_shared<LabelValueComponent>("DST", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::Toggle);',
    "distortion toggle construction",
)
page = replace_once(
    page,
    'delay_control_ = std::make_shared<LabelValueComponent>("DLY:", IGfxColor::White(), focusColor, focusColor);',
    'delay_control_ = std::make_shared<LabelValueComponent>("DLY", IGfxColor::White(), focusColor, focusColor, LabelValueComponent::Style::Toggle);',
    "delay toggle construction",
)

compact_layout = r'''void TB303ParamsPage::layoutComponents() {
  const auto& content = Layout::CONTENT;
  const int x0 = content.x + Layout::CONTENT_PAD_X;
  const int width = content.w - Layout::CONTENT_PAD_X * 2;

  constexpr int kMainKnobRadius = 13;
  const int knobRowY = content.y + 29;
  const int spacing = width / 5;

  cutoff_knob_->setBoundaries(Rect(x0 + spacing * 1 - kMainKnobRadius,
                                   knobRowY - kMainKnobRadius,
                                   kMainKnobRadius * 2,
                                   kMainKnobRadius * 2));
  resonance_knob_->setBoundaries(Rect(x0 + spacing * 2 - kMainKnobRadius,
                                      knobRowY - kMainKnobRadius,
                                      kMainKnobRadius * 2,
                                      kMainKnobRadius * 2));
  env_amount_knob_->setBoundaries(Rect(x0 + spacing * 3 - kMainKnobRadius,
                                       knobRowY - kMainKnobRadius,
                                       kMainKnobRadius * 2,
                                       kMainKnobRadius * 2));
  env_decay_knob_->setBoundaries(Rect(x0 + spacing * 4 - kMainKnobRadius,
                                      knobRowY - kMainKnobRadius,
                                      kMainKnobRadius * 2,
                                      kMainKnobRadius * 2));

  const std::string engineName = mini_acid_.currentSynthEngineName(voice_index_);
  const bool tb303 = isTb303Engine();
  const int parameterCount = mini_acid_.synthParameterCount(voice_index_);

  engine_type_control_->setLabel("TYPE");
  engine_type_control_->setValue(engineName.c_str());
  const std::vector<std::string> engineOptions = availableSynthEngines(mini_acid_);
  const int engineIndex = findEngineIndex(engineOptions, engineName);
  engine_type_control_->setNormalized(
      engineOptions.size() > 1 && engineIndex >= 0
          ? static_cast<float>(engineIndex) /
                static_cast<float>(engineOptions.size() - 1)
          : 0.0f);

  osc_control_->setBoundaries(Rect{0, 0, 0, 0});
  filter_control_->setBoundaries(Rect{0, 0, 0, 0});

  char value[24];
  std::string label;
  if (tb303) {
    const Parameter& oscillator = mini_acid_.parameter303(TB303ParamId::Oscillator, voice_index_);
    const Parameter& filter = mini_acid_.parameter303(TB303ParamId::FilterType, voice_index_);
    osc_control_->setLabel("OSC");
    osc_control_->setValue(oscillator.optionLabel());
    osc_control_->setNormalized(oscillator.normalized());
    filter_control_->setLabel("FLT");
    filter_control_->setValue(filter.optionLabel());
    filter_control_->setNormalized(filter.normalized());
  } else {
    if (parameterCount > 4) {
      const Parameter& parameter = mini_acid_.synthParameter(voice_index_, 4);
      label = parameter.label() ? parameter.label() : "P5";
      formatParameterValue(parameter, value, sizeof(value));
      osc_control_->setLabel(label.c_str());
      osc_control_->setValue(value);
      osc_control_->setNormalized(parameter.normalized());
    }
    if (parameterCount > 5) {
      const Parameter& parameter = mini_acid_.synthParameter(voice_index_, 5);
      label = parameter.label() ? parameter.label() : "P6";
      formatParameterValue(parameter, value, sizeof(value));
      filter_control_->setLabel(label.c_str());
      filter_control_->setValue(value);
      filter_control_->setNormalized(parameter.normalized());
    }
  }

  const bool distortionEnabled = mini_acid_.is303DistortionEnabled(voice_index_);
  distortion_control_->setLabel("DST");
  distortion_control_->setValue(distortionEnabled ? "ON" : "OFF");
  distortion_control_->setToggle(distortionEnabled);

  const bool delayEnabled = mini_acid_.is303DelayEnabled(voice_index_);
  delay_control_->setLabel("DLY");
  delay_control_->setValue(delayEnabled ? "ON" : "OFF");
  delay_control_->setToggle(delayEnabled);

  std::shared_ptr<LabelValueComponent> visible[5]{};
  int visibleCount = 0;
  visible[visibleCount++] = engine_type_control_;
  if (tb303 || parameterCount > 4) visible[visibleCount++] = osc_control_;
  if (tb303 || parameterCount > 5) visible[visibleCount++] = filter_control_;
  visible[visibleCount++] = distortion_control_;
  visible[visibleCount++] = delay_control_;

  constexpr int kCompactY = content.y + 65;
  constexpr int kCompactHeight = 34;
  constexpr int kCompactGap = 2;
  const int compactWidth =
      (width - kCompactGap * (visibleCount - 1)) / visibleCount;
  int x = x0;
  for (int i = 0; i < visibleCount; ++i) {
    const int cellWidth = i + 1 == visibleCount
        ? x0 + width - x
        : compactWidth;
    visible[i]->setBoundaries(Rect{x, kCompactY, cellWidth, kCompactHeight});
    x += cellWidth + kCompactGap;
  }
}

'''

page = replace_block(
    page,
    "void TB303ParamsPage::layoutComponents() {",
    "void TB303ParamsPage::adjustFocusedElement(",
    compact_layout,
    "compact layout",
)
page = replace_once(
    page,
    "const int hintY = content.y + content.h / 2 + 22;",
    "const int hintY = content.y + 54;",
    "compact direct-key hint position",
)
PAGE.write_text(page, encoding="utf-8")


test = TEST.read_text(encoding="utf-8")
compact_test = r'''

def test_compact_synth_controls_fit_the_cardputer_screen() -> None:
    page = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")

    require("kMainKnobRadius = 13" in page and "kRadius = 18" not in page,
            "the four primary synth knobs must use the compact radius")
    require("enum class Style : uint8_t" in page and
            "SelectorKnob" in page and "Toggle" in page,
            "secondary controls must expose selector and toggle visuals")
    require(page.count("LabelValueComponent::Style::SelectorKnob") == 3,
            "TYPE/OSC/FLT must use compact selector knobs")
    require(page.count("LabelValueComponent::Style::Toggle") == 2,
            "DST/DLY must use explicit toggle switches")
    require("setNormalized(oscillator.normalized())" in page and
            "setNormalized(filter.normalized())" in page,
            "stepped TB303 selectors must show their current position")
    require("setToggle(distortionEnabled)" in page and
            "setToggle(delayEnabled)" in page,
            "effect switches must reflect their current on/off state")
    require("kCompactY = content.y + 65" in page and
            "kCompactHeight = 34" in page and
            "std::shared_ptr<LabelValueComponent> visible[5]" in page,
            "all secondary controls must share one bounded lower row")
    require("const int hintY = content.y + 54;" in page,
            "direct knob hints must remain visible above the compact row")
    require("new " not in block(page,
                                "void TB303ParamsPage::layoutComponents()",
                                "void TB303ParamsPage::adjustFocusedElement"),
            "compact layout must not add explicit heap allocation during draw")
'''

test = replace_once(
    test,
    "\ndef test_tr606_shared_clock_is_not_owned_by_kick() -> None:\n",
    compact_test + "\n\ndef test_tr606_shared_clock_is_not_owned_by_kick() -> None:\n",
    "compact UI source test insertion",
)
test = replace_once(
    test,
    "    test_knob_keys_use_coarse_and_fine_steps()\n    test_tr606_shared_clock_is_not_owned_by_kick()",
    "    test_knob_keys_use_coarse_and_fine_steps()\n    test_compact_synth_controls_fit_the_cardputer_screen()\n    test_tr606_shared_clock_is_not_owned_by_kick()",
    "compact UI source test invocation",
)
TEST.write_text(test, encoding="utf-8")

DOC.write_text(
    """# Compact synth controls\n\n"
    "## Purpose\n\n"
    "Fit all Synth A/B parameter controls on the Cardputer ADV 240x135 display without changing DSP behavior or keyboard mappings.\n\n"
    "## Hardware list\n\n"
    "- M5Stack Cardputer ADV\n"
    "- USB-C cable\n\n"
    "## Wiring\n\n"
    "No external wiring is required. The built-in display and keyboard are used.\n\n"
    "## Build and flash\n\n"
    "```bash\n"
    "./scripts/build_cardputer_adv.sh\n"
    "```\n\n"
    "Flash the generated Cardputer ADV firmware with the existing project workflow.\n\n"
    "## Expected behavior\n\n"
    "- The four continuous synth parameters remain rotary controls but are visibly smaller.\n"
    "- `TYPE`, `OSC`, and `FLT` appear as compact selector knobs in one lower row.\n"
    "- `DST` and `DLY` appear as explicit ON/OFF toggle switches in the same row.\n"
    "- Values, labels, focus outline, direct A/Z-S/X-D/C-F/V controls, arrows, and fine adjustment remain functional.\n"
    "- No parameter, engine, effect, DSP, persistence, or audio behavior changes.\n\n"
    "## Troubleshooting\n\n"
    "- If a label is clipped, record the active synth engine and parameter label; compact values are intentionally bounded to their cell.\n"
    "- If focus skips a control, verify that the active engine exposes parameter 5/6; unavailable selectors are intentionally hidden.\n"
    "- If the screen shows the old large knobs, verify the firmware was built from this branch head.\n\n"
    "## Acceptance checklist\n\n"
    "- [ ] Four main knobs fit above the lower control row without overlap.\n"
    "- [ ] TYPE/OSC/FLT selectors are readable and track their current values.\n"
    "- [ ] DST/DLY toggles clearly show ON and OFF.\n"
    "- [ ] Left/Right focus reaches every visible control.\n"
    "- [ ] Up/Down and direct key pairs modify the same parameters as before.\n"
    "- [ ] Ctrl/Shift fine adjustment remains unchanged.\n"
    "- [ ] SDL and Cardputer ADV builds pass.\n"
    """,
    encoding="utf-8",
)
