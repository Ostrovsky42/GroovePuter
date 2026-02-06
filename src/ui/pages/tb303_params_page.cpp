#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif
#include "tb303_params_page.h"

#include "../ui_common.h"
#include "../ui_utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>

#include "../help_dialog_frames.h"
#include "../layout_manager.h"
#include "../screen_geometry.h"
#include "../ui_colors.h"
#include "../ui_input.h"

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
inline constexpr IGfxColor kDimText = IGfxColor(0x808080);
inline constexpr IGfxColor kValueText = IGfxColor::Cyan();
inline constexpr int kKnobStepCoarse = 5;
inline constexpr int kKnobStepFine = 1;
} // namespace

class TB303ParamsPage::KnobComponent : public FocusableComponent {
 public:
  KnobComponent(const Parameter& param,
                IGfxColor ring_color,
                IGfxColor indicator_color,
                IGfxColor focus_color,
                std::function<void(int)> adjust_fn)
      : param_(param),
        ring_color_(ring_color),
        indicator_color_(indicator_color),
        focus_color_(focus_color),
        adjust_fn_(std::move(adjust_fn)) {}

  void setValue(int direction) {
    if (adjust_fn_) adjust_fn_(direction);
  }

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type == MINIACID_MOUSE_DOWN) {
      if (ui_event.button != MOUSE_BUTTON_LEFT) return false;
      if (!contains(ui_event.x, ui_event.y)) return false;
      dragging_ = true;
      last_drag_y_ = ui_event.y;
      drag_accum_ = 0;
      return true;
    }

    if (ui_event.event_type == MINIACID_MOUSE_UP) {
      if (!dragging_) return false;
      dragging_ = false;
      drag_accum_ = 0;
      return true;
    }

    if (ui_event.event_type == MINIACID_MOUSE_DRAG) {
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

    if (ui_event.event_type == MINIACID_MOUSE_SCROLL) {
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
    int radius = std::min(bounds.w, bounds.h) / 2;
    int cx = bounds.x + bounds.w / 2;
    int cy = bounds.y + bounds.h / 2;

    float norm = std::clamp(param_.normalized(), 0.0f, 1.0f);

    gfx.drawKnobFace(cx, cy, radius, ring_color_, COLOR_BLACK);

    constexpr float kDegToRad = 3.14159265f / 180.0f;
    float deg_angle = (135.0f + norm * 270.0f);
    if (deg_angle >= 360.0f) deg_angle -= 360.0f;
    float angle = deg_angle * kDegToRad;

    int ix = cx + static_cast<int>(roundf(cosf(angle) * (radius - 2)));
    int iy = cy + static_cast<int>(roundf(sinf(angle) * (radius - 2)));
    drawLineColored(gfx, cx, cy, ix, iy, indicator_color_);

    const char* label = param_.label();
    if (!label) label = "";
    gfx.setTextColor(kDimText);
    int label_x = cx - gfx.textWidth(label) / 2;
    gfx.drawText(label_x, cy + radius + 4, label);

    char buf[24];
    const char* unit = param_.unit();
    float value = param_.value();
    if (unit && unit[0]) {
      std::snprintf(buf, sizeof(buf), "%.0f%s", value, unit);
    } else {
      std::snprintf(buf, sizeof(buf), "%.2f", value);
    }
    gfx.setTextColor(IGfxColor::White());
    int val_x = cx - gfx.textWidth(buf) / 2;
    gfx.drawText(val_x, cy - radius - 10, buf);

    if (isFocused()) {
      int pad = 3;
      gfx.drawRect(bounds.x - pad, bounds.y - pad,
                   bounds.w + pad * 2, bounds.h + pad * 2, focus_color_);
    }
  }

 private:
  const Parameter& param_;
  IGfxColor ring_color_;
  IGfxColor indicator_color_;
  IGfxColor focus_color_;
  std::function<void(int)> adjust_fn_;
  bool dragging_ = false;
  int last_drag_y_ = 0;
  int drag_accum_ = 0;
};

class TB303ParamsPage::LabelValueComponent : public FocusableComponent {
 public:
  LabelValueComponent(const char* label, IGfxColor label_color, IGfxColor value_color, IGfxColor focus_color)
      : label_(label ? label : ""),
        label_color_(label_color),
        value_color_(value_color),
        focus_color_(focus_color) {}

  void setValue(const char* value) { value_ = value ? value : ""; }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    gfx.setTextColor(label_color_);
    gfx.drawText(bounds.x, bounds.y, label_);
    int label_w = gfx.textWidth(label_);
    gfx.setTextColor(value_color_);
    gfx.drawText(bounds.x + label_w + 3, bounds.y, value_);

    if (isFocused()) {
      int pad = 2;
      gfx.drawRect(bounds.x - pad, bounds.y - pad,
                   bounds.w + pad * 2, bounds.h + pad * 2, focus_color_);
    }
  }

 private:
  const char* label_ = "";
  const char* value_ = "";
  IGfxColor label_color_;
  IGfxColor value_color_;
  IGfxColor focus_color_;
};

TB303ParamsPage::TB303ParamsPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voice_index)
    : gfx_(gfx),
      mini_acid_(mini_acid),
      audio_guard_(audio_guard),
      voice_index_(voice_index) {
  title_ = (voice_index_ == 0) ? "303A PARAMS" : "303B PARAMS";
}

void TB303ParamsPage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) {
    initComponents();
  }
}

void TB303ParamsPage::initComponents() {
  const Parameter& pCut = mini_acid_.parameter303(TB303ParamId::Cutoff, voice_index_);
  const Parameter& pRes = mini_acid_.parameter303(TB303ParamId::Resonance, voice_index_);
  const Parameter& pEnv = mini_acid_.parameter303(TB303ParamId::EnvAmount, voice_index_);
  const Parameter& pDec = mini_acid_.parameter303(TB303ParamId::EnvDecay, voice_index_);

  IGfxColor focusColor = (voice_index_ == 0) ? COLOR_SYNTH_A : COLOR_SYNTH_B;

  cutoff_knob_ = std::make_shared<KnobComponent>(
      pCut, COLOR_KNOB_1, COLOR_KNOB_1, focusColor,
      [this](int direction) {
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::Cutoff, kKnobStepCoarse * direction, voice_index_);
        });
      });

  resonance_knob_ = std::make_shared<KnobComponent>(
      pRes, COLOR_KNOB_2, COLOR_KNOB_2, focusColor,
      [this](int direction) {
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::Resonance, kKnobStepCoarse * direction, voice_index_);
        });
      });

  env_amount_knob_ = std::make_shared<KnobComponent>(
      pEnv, COLOR_KNOB_3, COLOR_KNOB_3, focusColor,
      [this](int direction) {
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::EnvAmount, kKnobStepCoarse * direction, voice_index_);
        });
      });

  env_decay_knob_ = std::make_shared<KnobComponent>(
      pDec, COLOR_KNOB_4, COLOR_KNOB_4, focusColor,
      [this](int direction) {
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::EnvDecay, kKnobStepCoarse * direction, voice_index_);
        });
      });

  osc_control_ = std::make_shared<LabelValueComponent>("OSC:", IGfxColor::White(), kValueText, focusColor);
  filter_control_ = std::make_shared<LabelValueComponent>("FLT:", IGfxColor::White(), kValueText, focusColor);
  distortion_control_ = std::make_shared<LabelValueComponent>("DST:", IGfxColor::White(), kValueText, focusColor);
  delay_control_ = std::make_shared<LabelValueComponent>("DLY:", IGfxColor::White(), kValueText, focusColor);

  addChild(cutoff_knob_);
  addChild(resonance_knob_);
  addChild(env_amount_knob_);
  addChild(env_decay_knob_);
  addChild(osc_control_);
  addChild(filter_control_);
  addChild(distortion_control_);
  addChild(delay_control_);

  initialized_ = true;
}

void TB303ParamsPage::layoutComponents() {
  const auto& c = Layout::CONTENT;
  const int padX = Layout::CONTENT_PAD_X;
  const int x0 = c.x + padX;
  const int w = c.w - 2 * padX;

  const int radius = 18;
  const int knobRowY = c.y + (c.h / 2) - 18;
  const int spacing = w / 5;

  const int cx1 = x0 + spacing * 1;
  const int cx2 = x0 + spacing * 2;
  const int cx3 = x0 + spacing * 3;
  const int cx4 = x0 + spacing * 4;

  cutoff_knob_->setBoundaries(Rect(cx1 - radius, knobRowY - radius, radius * 2, radius * 2));
  resonance_knob_->setBoundaries(Rect(cx2 - radius, knobRowY - radius, radius * 2, radius * 2));
  env_amount_knob_->setBoundaries(Rect(cx3 - radius, knobRowY - radius, radius * 2, radius * 2));
  env_decay_knob_->setBoundaries(Rect(cx4 - radius, knobRowY - radius, radius * 2, radius * 2));

  const int rowTopY = c.y + c.h - (gfx_.fontHeight() * 2) - 5;
  const int rowBottomY = rowTopY + gfx_.fontHeight() + 2;

  const Parameter& pOsc = mini_acid_.parameter303(TB303ParamId::Oscillator, voice_index_);
  const Parameter& pFlt = mini_acid_.parameter303(TB303ParamId::FilterType, voice_index_);

  const char* oscLabel = pOsc.optionLabel();
  if (!oscLabel) oscLabel = "";
  osc_control_->setValue(oscLabel);

  const char* fltLabel = pFlt.optionLabel();
  if (!fltLabel) fltLabel = "";
  filter_control_->setValue(fltLabel);

  const bool dstOn = mini_acid_.is303DistortionEnabled(voice_index_);
  distortion_control_->setValue(dstOn ? "on" : "off");

  const bool dlyOn = mini_acid_.is303DelayEnabled(voice_index_);
  delay_control_->setValue(dlyOn ? "on" : "off");

  const int gap = 8;
  const int labelGap = 3;

  int x = x0;
  int rowY = rowTopY;

  auto place = [&](std::shared_ptr<LabelValueComponent>& cpt, const char* label, const char* valueMax) {
    const int lw = gfx_.textWidth(label);
    const int vwMax = gfx_.textWidth(valueMax);
    const int fw = lw + labelGap + vwMax;
    if ((x + fw) > (x0 + w)) {
      x = x0;
      rowY = rowBottomY;
    }
    cpt->setBoundaries(Rect(x, rowY, fw, gfx_.fontHeight()));
    x += fw + gap;
  };

  place(osc_control_, "OSC:", "super");
  place(filter_control_, "FLT:", "soft");
  place(distortion_control_, "DST:", "off");
  place(delay_control_, "DLY:", "off");
}

void TB303ParamsPage::adjustFocusedElement(int direction, bool fine) {
  int step = fine ? kKnobStepFine : kKnobStepCoarse;
  
  if (cutoff_knob_ && cutoff_knob_->isFocused()) {
    withAudioGuard([&]() {
      mini_acid_.adjust303Parameter(TB303ParamId::Cutoff, step * direction, voice_index_);
    });
    return;
  }
  if (resonance_knob_ && resonance_knob_->isFocused()) {
    withAudioGuard([&]() {
      mini_acid_.adjust303Parameter(TB303ParamId::Resonance, step * direction, voice_index_);
    });
    return;
  }
  if (env_amount_knob_ && env_amount_knob_->isFocused()) {
    withAudioGuard([&]() {
      mini_acid_.adjust303Parameter(TB303ParamId::EnvAmount, step * direction, voice_index_);
    });
    return;
  }
  if (env_decay_knob_ && env_decay_knob_->isFocused()) {
    withAudioGuard([&]() {
      mini_acid_.adjust303Parameter(TB303ParamId::EnvDecay, step * direction, voice_index_);
    });
    return;
  }

  if (osc_control_ && osc_control_->isFocused()) {
    withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, direction, voice_index_); });
    return;
  }
  if (filter_control_ && filter_control_->isFocused()) {
    withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::FilterType, direction, voice_index_); });
    return;
  }
  if (distortion_control_ && distortion_control_->isFocused()) {
    bool enabled = mini_acid_.is303DistortionEnabled(voice_index_);
    if ((direction > 0 && !enabled) || (direction < 0 && enabled)) {
      withAudioGuard([&]() { mini_acid_.toggleDistortion303(voice_index_); });
    }
    return;
  }
  if (delay_control_ && delay_control_->isFocused()) {
    bool enabled = mini_acid_.is303DelayEnabled(voice_index_);
    if ((direction > 0 && !enabled) || (direction < 0 && enabled)) {
      withAudioGuard([&]() { mini_acid_.toggleDelay303(voice_index_); });
    }
    return;
  }
}

void TB303ParamsPage::draw(IGfx& gfx) {
  if (!initialized_) initComponents();

  const char* title = (voice_index_ == 0) ? "303A BASS" : "303B LEAD";
  IGfxColor headerColor = (voice_index_ == 0) ? COLOR_SYNTH_A : COLOR_SYNTH_B;
  UI::drawStandardHeader(gfx, mini_acid_, title);
  LayoutManager::clearContent(gfx);

  layoutComponents();

  GrooveboxMode mode = mini_acid_.grooveboxMode();
  const char* modeName = (mode == GrooveboxMode::Acid) ? "ACID" : "MIN";
  gfx.setTextColor(kDimText);
  gfx.drawText(Layout::CONTENT.x + Layout::CONTENT.w - 34, Layout::CONTENT.y + 2, modeName);

  gfx.setTextColor(kDimText);
  const auto& c = Layout::CONTENT;
  const int hintY = c.y + c.h / 2 + 22;
  gfx.drawText(c.x + 10, hintY, "A/Z  S/X  D/C  F/V");

  Container::draw(gfx_);

  UI::drawStandardFooter(gfx, "[L/R]FOCUS [U/D]VAL [CTRL]FINE", "[T/G]OSC [Y/H]FLT [N/M]FX");
}

const std::string& TB303ParamsPage::getTitle() const {
  return title_;
}

bool TB303ParamsPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) {
    return Container::handleEvent(ui_event);
  }

  if (UIInput::isGlobalNav(ui_event)) return false;

  int nav = UIInput::navCode(ui_event);
  bool fine = ui_event.shift || ui_event.ctrl;
  
  switch (nav) {
    case MINIACID_LEFT:  focusPrev(); return true;
    case MINIACID_RIGHT: focusNext(); return true;
    case MINIACID_UP:    adjustFocusedElement(1, fine); return true;
    case MINIACID_DOWN:  adjustFocusedElement(-1, fine); return true;
    default: break;
  }

  const char key = ui_event.key;
  if (!key) return Container::handleEvent(ui_event);

  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));

  // Explicit reset shortcuts to avoid accidental resets on key auto-repeat.
  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    if (lowerKey == 'z') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::Cutoff, 800.0f, voice_index_); }); return true; }
    if (lowerKey == 'x') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::Resonance, 0.0f, voice_index_); }); return true; }
    if (lowerKey == 'c') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::EnvAmount, 400.0f, voice_index_); }); return true; }
    if (lowerKey == 'v') { withAudioGuard([&]() { mini_acid_.set303Parameter(TB303ParamId::EnvDecay, 420.0f, voice_index_); }); return true; }
  }

  if (!ui_event.shift && !ui_event.ctrl && !ui_event.meta) {
    const char* patternKeys = "qwertyui";
    const char* found = std::strchr(patternKeys, lowerKey);
    if (found) {
      int idx = static_cast<int>(found - patternKeys);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, idx); });
      return true;
    }
  }

  switch (lowerKey) {
    case 't':
      withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, 1, voice_index_); });
      return true;
    case 'g':
      withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, -1, voice_index_); });
      return true;
    case 'y':
      withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::FilterType, 1, voice_index_); });
      return true;
    case 'h':
      withAudioGuard([&]() { mini_acid_.adjust303Parameter(TB303ParamId::FilterType, -1, voice_index_); });
      return true;

    case 'a': if (cutoff_knob_) cutoff_knob_->setValue(1); return true;
    case 'z': if (cutoff_knob_) cutoff_knob_->setValue(-1); return true;
    case 's': if (resonance_knob_) resonance_knob_->setValue(1); return true;
    case 'x': if (resonance_knob_) resonance_knob_->setValue(-1); return true;
    case 'd': if (env_amount_knob_) env_amount_knob_->setValue(1); return true;
    case 'c': if (env_amount_knob_) env_amount_knob_->setValue(-1); return true;
    case 'f': if (env_decay_knob_) env_decay_knob_->setValue(1); return true;
    case 'v': if (env_decay_knob_) env_decay_knob_->setValue(-1); return true;

    case 'n':
      withAudioGuard([&]() { mini_acid_.toggleDistortion303(voice_index_); });
      return true;
    case 'm':
      withAudioGuard([&]() { mini_acid_.toggleDelay303(voice_index_); });
      return true;
/*
    case '1':
    case '2':
    case '3':
    case '4':
      loadModePreset(static_cast<int>(lowerKey - '1'));
      return true;
*/
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
  switch (frameIndex) {
    case 0:
      drawHelpPage303(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}
