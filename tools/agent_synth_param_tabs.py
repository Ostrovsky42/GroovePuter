#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/ui/pages/tb303_params_page.h"
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


header = HEADER.read_text(encoding="utf-8")
header = replace_once(
    header,
    "  void layoutComponents();\n",
    "  void layoutComponents();\n"
    "  void setActiveTab(bool more);\n"
    "  void updateTabFocusability();\n"
    "  void rememberFocusedSlot();\n"
    "  void restoreFocusedSlot();\n"
    "  void focusComponent(Component* component);\n"
    "  void drawTabSwitcher(IGfx& gfx, const Rect& content);\n"
    "  void drawMainSummary(IGfx& gfx, const Rect& content);\n",
    "tab helper declarations",
)
header = replace_once(
    header,
    "  std::shared_ptr<LabelValueComponent> distortion_control_;\n  std::string title_;\n",
    "  std::shared_ptr<LabelValueComponent> distortion_control_;\n"
    "  bool more_tab_ = false;\n"
    "  uint8_t main_focus_slot_ = 0;\n"
    "  uint8_t more_focus_slot_ = 0;\n"
    "  std::string title_;\n",
    "tab state members",
)
HEADER.write_text(header, encoding="utf-8")


page = PAGE.read_text(encoding="utf-8")

row_component = r'''class TB303ParamsPage::LabelValueComponent : public FocusableComponent {
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

'''
page = replace_block(
    page,
    "class TB303ParamsPage::LabelValueComponent : public FocusableComponent {",
    "TB303ParamsPage::TB303ParamsPage(",
    row_component,
    "MORE row component",
)

page = page.replace("LabelValueComponent::Style::SelectorKnob",
                    "LabelValueComponent::Style::Stepper")

layout_and_tabs = r'''void TB303ParamsPage::layoutComponents() {
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
    constexpr int kMainKnobRadius = 13;
    const int knobRowY = content.y + 45;
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

'''
page = replace_block(
    page,
    "void TB303ParamsPage::layoutComponents() {",
    "void TB303ParamsPage::adjustFocusedElement(",
    layout_and_tabs,
    "tabbed synth layout",
)

new_draw = r'''void TB303ParamsPage::draw(IGfx& gfx) {
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
  drawTabSwitcher(gfx, content);
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
    drawMainSummary(gfx, content);
  }

  Container::draw(gfx_);

  if (!more_tab_) {
    UI::drawStandardFooter(gfx,
                           "[TAB]MORE [L/R]FOCUS [U/D]VAL",
                           "A/Z S/X D/C F/V [CTRL]FINE");
  } else {
    UI::drawStandardFooter(gfx,
                           "[TAB]MAIN [L/R]ROW [U/D]CHANGE",
                           "TYPE OSC FLT DST DLY");
  }
}

'''
page = replace_block(
    page,
    "void TB303ParamsPage::draw(IGfx& gfx) {",
    "const std::string& TB303ParamsPage::getTitle() const",
    new_draw,
    "tab-aware draw",
)

page = replace_once(
    page,
    "  if (UIInput::isGlobalNav(ui_event)) return false;\n  if (UIInput::isTab(ui_event)) return false;\n",
    "  if (UIInput::isGlobalNav(ui_event)) return false;\n"
    "  if (UIInput::isTab(ui_event)) {\n"
    "    if (ui_event.ctrl || ui_event.alt || ui_event.meta) return false;\n"
    "    setActiveTab(!more_tab_);\n"
    "    return true;\n"
    "  }\n",
    "local Tab handling",
)

page = replace_once(
    page,
    "  if (!ui_event.ctrl && !ui_event.meta) {\n    const int patternIndex = qwertyToPatternIndex(lowerKey);\n",
    "  const bool mainKnobKey = lowerKey == 'a' || lowerKey == 'z' ||\n"
    "      lowerKey == 's' || lowerKey == 'x' ||\n"
    "      lowerKey == 'd' || lowerKey == 'c' ||\n"
    "      lowerKey == 'f' || lowerKey == 'v';\n"
    "  if (more_tab_ && !ui_event.ctrl && !ui_event.alt && !ui_event.meta && mainKnobKey) {\n"
    "    return true;\n"
    "  }\n\n"
    "  if (!ui_event.ctrl && !ui_event.meta) {\n    const int patternIndex = qwertyToPatternIndex(lowerKey);\n",
    "MAIN-only direct knob keys",
)

PAGE.write_text(page, encoding="utf-8")


test = TEST.read_text(encoding="utf-8")
new_test = r'''def test_compact_synth_controls_fit_the_cardputer_screen() -> None:
    header = (ROOT / "src/ui/pages/tb303_params_page.h").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")

    require("kMainKnobRadius = 13" in page and "kRadius = 18" not in page,
            "the four MAIN synth knobs must retain the compact radius")
    require('drawSegment(x, "MAIN", !more_tab_)' in page and
            '"MORE", more_tab_' in page and '"TAB >"' in page,
            "MAIN/MORE discoverability must be visible on the parameter page")
    require("setActiveTab(!more_tab_)" in page and
            "if (ui_event.ctrl || ui_event.alt || ui_event.meta) return false;" in page,
            "plain Tab must toggle local tabs while Fn/meta Tab stays global")
    require("main_focus_slot_" in header and "more_focus_slot_" in header and
            "rememberFocusedSlot" in page and "restoreFocusedSlot" in page,
            "MAIN and MORE must remember focus independently")
    require(page.count("LabelValueComponent::Style::Stepper") == 3,
            "TYPE/OSC/FLT must use full-row steppers")
    require(page.count("LabelValueComponent::Style::Toggle") == 2,
            "DST/DLY must use full-row switches")
    require("if (focused)" in page and
            "gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, focus_color_)" in page,
            "the active MORE row must use a filled focus state")
    require("kMoreRowHeight = 15" in page and "LabelValueComponent* rows[5]" in page,
            "MORE must use five stable full-width rows")
    require("setEnabled(oscAvailable)" in page and
            "setEnabled(filterAvailable)" in page and
            'setValue("--")' in page,
            "unavailable engine parameters must remain visible but disabled")
    require("Both effects are per-voice post-engine stages" in page and
            "distortion_control_->setEnabled(true)" in page and
            "delay_control_->setEnabled(true)" in page,
            "current DST/DLY rows must remain available for every synth engine")
    require("const char* keyHints[4]" in page and
            "if (more_tab_ && !ui_event.ctrl" in page,
            "direct A/Z-S/X-D/C-F/V controls must be advertised and active only on MAIN")
    require("new " not in block(page,
                                "void TB303ParamsPage::layoutComponents()",
                                "void TB303ParamsPage::adjustFocusedElement"),
            "tab layout must not add explicit heap allocation during draw")


'''
test = replace_block(
    test,
    "def test_compact_synth_controls_fit_the_cardputer_screen() -> None:",
    "def test_tr606_shared_clock_is_not_owned_by_kick() -> None:",
    new_test,
    "tabbed synth source regression",
)
TEST.write_text(test, encoding="utf-8")


doc = r'''# Synth parameter MAIN / MORE tabs

## Purpose

Keep the four continuously performed Synth A/B parameters compact and immediately available while moving infrequent engine, oscillator, filter, distortion, and delay settings to a discoverable `MORE` tab.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable

## Wiring

No external wiring is required. The test uses the built-in 240×135 display and keyboard.

## Build and flash

```bash
./scripts/build_cardputer_adv.sh
```

Flash the generated Cardputer ADV firmware using the existing project workflow.

This UI stage expects the consolidated Cardputer word/HID Tab input fix from PR #63 before final hardware acceptance. `Fn+Tab` must remain global workflow navigation.

## Expected behavior

### MAIN

- `MAIN / MORE` is always visible at the top; `MAIN` is selected initially.
- Four radius-13 continuous knobs show parameters 1–4.
- `A/Z`, `S/X`, `D/C`, and `F/V` remain direct real-time controls only on `MAIN`.
- A bottom summary shows the current engine and the next two parameter values.
- `DST` and `DLY` badges show current effect state.
- `TAB >` advertises the second tab.

### MORE

- Plain `Tab` switches to `MORE`; another plain `Tab` returns to `MAIN`.
- `TYPE`, parameter 5, parameter 6, `DST`, and `DLY` use five stable full-width rows.
- `Left/Right` changes the focused row; `Up/Down` changes its value.
- The focused row uses a filled background, not a thin focus frame.
- Discrete values use `< value >`; effects use track-and-thumb switches.
- Missing parameter 5/6 rows remain visible as disabled `--` rows, so the list does not jump.
- Current distortion and delay are post-engine per-voice stages and remain available for all selectable synth engines.
- MAIN and MORE remember their own last focused control.

## Troubleshooting

- Plain `Tab` does nothing on hardware: verify the build includes the word-only/HID Tab input consolidation from PR #63.
- `Fn+Tab` opens `MORE`: verify the page rejects modified Tab events and top-level workflow navigation receives `event.meta`.
- Focus lands on a disabled row: verify `updateTabFocusability()` runs after the active engine changes.
- A row label is clipped: record the synth engine and parameter label; the right-side value is intentionally bounded.
- The old one-screen lower row is still visible: confirm the firmware was built from the latest PR #64 head.

## Acceptance checklist

- [ ] Page initially opens on `MAIN` with `MAIN / MORE` visible.
- [ ] Four continuous knobs are radius 13 and do not overlap values, labels, key hints, or summary.
- [ ] Plain `Tab` switches `MAIN → MORE → MAIN` exactly once per press.
- [ ] `Fn+Tab` changes workflow and never changes the local synth tab.
- [ ] `Left/Right` cycles only controls belonging to the visible tab.
- [ ] Returning to each tab restores that tab's previous focus.
- [ ] `Up/Down` changes the focused MORE row.
- [ ] Focus is shown by a filled row.
- [ ] Stepper arrows and switch positions match current values.
- [ ] Missing P5/P6 rows stay visible as disabled `--` rows.
- [ ] `DST/DLY` work on TB303 and at least one non-TB303 engine.
- [ ] `A/Z`, `S/X`, `D/C`, `F/V` change parameters only on MAIN.
- [ ] No DSP, audio routing, scene persistence, or MIDI behavior changes.
- [ ] Host tests and Cardputer ADV build pass.
'''
DOC.write_text(doc, encoding="utf-8")
