#include "tb303_params_page.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <functional>

#include "../help_dialog_frames.h"

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
} // namespace

class Synth303ParamsPage::KnobComponent : public FocusableComponent {
 public:
  KnobComponent(const Parameter& param, IGfxColor ring_color,
                IGfxColor indicator_color,
                std::function<void(int)> adjust_fn)
      : param_(param),
        ring_color_(ring_color),
        indicator_color_(indicator_color),
        adjust_fn_(std::move(adjust_fn)) {}

  void setValue(int direction) {
    if (adjust_fn_) {
      adjust_fn_(direction);
    }
  }

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type == MINIACID_MOUSE_DOWN) {
      if (ui_event.button != MOUSE_BUTTON_LEFT) {
        return false;
      }
      if (!contains(ui_event.x, ui_event.y)) {
        return false;
      }
      dragging_ = true;
      last_drag_y_ = ui_event.y;
      drag_accum_ = 0;
      return true;
    }

    if (ui_event.event_type == MINIACID_MOUSE_UP) {
      if (!dragging_) {
        return false;
      }
      dragging_ = false;
      drag_accum_ = 0;
      return true;
    }

    if (ui_event.event_type == MINIACID_MOUSE_DRAG) {
      if (!dragging_) {
        return false;
      }
      int delta = ui_event.dy;
      if (delta == 0) {
        delta = ui_event.y - last_drag_y_;
      }
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
      if (!contains(ui_event.x, ui_event.y)) {
        return false;
      }
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
    if (deg_angle >= 360.0f) {
      deg_angle -= 360.0f;
    }
    float angle = (deg_angle) * kDegToRad;

    int ix = cx + static_cast<int>(roundf(cosf(angle) * (radius - 2)));
    int iy = cy + static_cast<int>(roundf(sinf(angle) * (radius - 2)));

    drawLineColored(gfx, cx, cy, ix, iy, indicator_color_);

    const char* label = param_.label();
    if (!label) {
      label = "";
    }
    gfx.setTextColor(COLOR_LABEL);
    int label_x = cx - gfx.textWidth(label) / 2;
    gfx.drawText(label_x, cy + radius + 4, label);  // Reduced from +6

    char buf[48];
    const char* unit = param_.unit();
    float value = param_.value();
    if (unit && unit[0]) {
      snprintf(buf, sizeof(buf), "%.0f%s", value, unit);
    } else {
      snprintf(buf, sizeof(buf), "%.2f", value);
    }
    int val_x = cx - gfx.textWidth(buf) / 2;
    gfx.drawText(val_x, cy - radius - 10, buf);  // Reduced from -14 to fit tighter space

    if (isFocused()) {
      int pad = 3;
      gfx.drawRect(bounds.x - pad, bounds.y - pad,
                   bounds.w + pad * 2, bounds.h + pad * 2, kFocusColor);
    }
  }

 private:
  const Parameter& param_;
  IGfxColor ring_color_;
  IGfxColor indicator_color_;
  std::function<void(int)> adjust_fn_;
  bool dragging_ = false;
  int last_drag_y_ = 0;
  int drag_accum_ = 0;
};

class Synth303ParamsPage::LabelValueComponent : public FocusableComponent {
 public:
  LabelValueComponent(const char* label, IGfxColor label_color,
                      IGfxColor value_color)
      : label_(label ? label : ""),
        label_color_(label_color),
        value_color_(value_color) {}

  void setValue(const char* value) { value_ = value ? value : ""; }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    gfx.setTextColor(label_color_);
    gfx.drawText(bounds.x, bounds.y, label_);
    int label_w = textWidth(gfx, label_);
    gfx.setTextColor(value_color_);
    gfx.drawText(bounds.x + label_w + 3, bounds.y, value_);

    if (isFocused()) {
      int pad = 2;
      gfx.drawRect(bounds.x - pad, bounds.y - pad,
                   bounds.w + pad * 2, bounds.h + pad * 2, kFocusColor);
    }
  }

 private:
  const char* label_ = "";
  const char* value_ = "";
  IGfxColor label_color_;
  IGfxColor value_color_;
};

Synth303ParamsPage::Synth303ParamsPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index) :
    gfx_(gfx),
    mini_acid_(mini_acid),
    audio_guard_(audio_guard),
    voice_index_(voice_index)
{
  title_ = voice_index_ == 0 ? "303A PARAMS" : "303B PARAMS";
}

void Synth303ParamsPage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) {
    initComponents();
  }
}

void Synth303ParamsPage::initComponents() {
  const Parameter& pCut = mini_acid_.parameter303(TB303ParamId::Cutoff, voice_index_);
  const Parameter& pRes = mini_acid_.parameter303(TB303ParamId::Resonance, voice_index_);
  const Parameter& pEnv = mini_acid_.parameter303(TB303ParamId::EnvAmount, voice_index_);
  const Parameter& pDec = mini_acid_.parameter303(TB303ParamId::EnvDecay, voice_index_);

  cutoff_knob_ = std::make_shared<KnobComponent>(
      pCut, COLOR_KNOB_1, COLOR_KNOB_1,
      [this](int direction) {
        int steps = 5;
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::Cutoff, steps * direction, voice_index_);
        });
      });
  resonance_knob_ = std::make_shared<KnobComponent>(
      pRes, COLOR_KNOB_2, COLOR_KNOB_2,
      [this](int direction) {
        int steps = 5;
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::Resonance, steps * direction, voice_index_);
        });
      });
  env_amount_knob_ = std::make_shared<KnobComponent>(
      pEnv, COLOR_KNOB_3, COLOR_KNOB_3,
      [this](int direction) {
        int steps = 5;
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::EnvAmount, steps * direction, voice_index_);
        });
      });
  env_decay_knob_ = std::make_shared<KnobComponent>(
      pDec, COLOR_KNOB_4, COLOR_KNOB_4,
      [this](int direction) {
        int steps = 5;
        withAudioGuard([&]() {
          mini_acid_.adjust303Parameter(TB303ParamId::EnvDecay, steps * direction, voice_index_);
        });
      });
  osc_control_ = std::make_shared<LabelValueComponent>("OSC:", COLOR_WHITE,
                                                       IGfxColor::Cyan());
  filter_control_ = std::make_shared<LabelValueComponent>("FLT:", COLOR_WHITE,
                                                          IGfxColor::Cyan());
  delay_control_ = std::make_shared<LabelValueComponent>("DLY:", COLOR_WHITE,
                                                         IGfxColor::Cyan());
  distortion_control_ = std::make_shared<LabelValueComponent>("DST:", COLOR_WHITE,
                                                              IGfxColor::Cyan());

  addChild(cutoff_knob_);
  addChild(resonance_knob_);
  addChild(env_amount_knob_);
  addChild(env_decay_knob_);
  addChild(osc_control_);
  addChild(filter_control_);
  addChild(distortion_control_);
  addChild(delay_control_);

  // setting boundaries really belongs in a layout pass, but for now do it here
  int x_margin = 4;  // Fixed: was -10 causing overflow!
  int usable_w = width() - x_margin * 2;

  int radius = 14;  // Reduced from 18 to fit better
  int spacing = usable_w / 5;
  
  int cx1 = dx() + x_margin + spacing * 1;
  int cx2 = dx() + x_margin + spacing * 2;
  int cx3 = dx() + x_margin + spacing * 3;
  int cx4 = dx() + x_margin + spacing * 4;
  int center_y_for_knobs = dy() + height() / 2 - 13;

  cutoff_knob_->setBoundaries(Rect(cx1 - radius, center_y_for_knobs - radius,
                                     radius * 2, radius * 2));
  resonance_knob_->setBoundaries(Rect(cx2 - radius, center_y_for_knobs - radius,
                                      radius * 2, radius * 2));
  env_amount_knob_->setBoundaries(Rect(cx3 - radius, center_y_for_knobs - radius,
                                      radius * 2, radius * 2));
  env_decay_knob_->setBoundaries(Rect(cx4 - radius, center_y_for_knobs - radius,
                                      radius * 2, radius * 2));


  initialized_ = true;
}

void Synth303ParamsPage::adjustFocusedElement(int direction) {
  if (cutoff_knob_ && cutoff_knob_->isFocused()) {
    cutoff_knob_->setValue(direction);
    return;
  }
  if (resonance_knob_ && resonance_knob_->isFocused()) {
    resonance_knob_->setValue(direction);
    return;
  }
  if (env_amount_knob_ && env_amount_knob_->isFocused()) {
    env_amount_knob_->setValue(direction);
    return;
  }
  if (env_decay_knob_ && env_decay_knob_->isFocused()) {
    env_decay_knob_->setValue(direction);
    return;
  }
  if (osc_control_ && osc_control_->isFocused()) {
    withAudioGuard([&]() {
      mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, direction, voice_index_);
    });
    return;
  }
  if (filter_control_ && filter_control_->isFocused()) {
    withAudioGuard([&]() {
      mini_acid_.adjust303Parameter(TB303ParamId::FilterType, direction, voice_index_);
    });
    return;
  }
  if (delay_control_ && delay_control_->isFocused()) {
    bool enabled = mini_acid_.is303DelayEnabled(voice_index_);
    if ((direction > 0 && !enabled) || (direction < 0 && enabled)) {
      withAudioGuard([&]() {
        mini_acid_.toggleDelay303(voice_index_);
      });
    }
  }
  if (distortion_control_ && distortion_control_->isFocused()) {
    bool enabled = mini_acid_.is303DistortionEnabled(voice_index_);
    if ((direction > 0 && !enabled) || (direction < 0 && enabled)) {
      withAudioGuard([&]() {
        mini_acid_.toggleDistortion303(voice_index_);
      });
    }
  }
}

void Synth303ParamsPage::draw(IGfx& gfx) {
  (void)gfx;
  if (!initialized_) {
    initComponents();
  }

  char buf[128];
  auto print = [&](int px, int py, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gfx_.drawText(px, py, buf);
  };

  int center_y_for_knobs = dy() + height() / 2 - 13;

  int x_margin = 4;  // Fixed: was -10 causing overflow!
  int usable_w = width() - x_margin * 2;

  int radius = 14;  // Reduced from 18 to fit better
  int spacing = usable_w / 5;
  
  int cx1 = dx() + x_margin + spacing * 1;
  int cx2 = dx() + x_margin + spacing * 2;
  int cx3 = dx() + x_margin + spacing * 3;
  int cx4 = dx() + x_margin + spacing * 4;

  int delta_y_for_controls = 35;
  int delta_x_for_controls = -9;
  int rowLeft = dx() + 6;
  int labelValueGap = 2;
  int controlGap = 8;

  // oscillator type control
  const Parameter& pOsc = mini_acid_.parameter303(TB303ParamId::Oscillator, voice_index_);
  const Parameter& pFilter = mini_acid_.parameter303(TB303ParamId::FilterType, voice_index_);

  const char* oscLabel = pOsc.optionLabel();
  if (!oscLabel) oscLabel = "";
  int oscLabelX = rowLeft;
  int oscSwitchesY = dy() + height() - 13;
  int oscLabelW = textWidth(gfx_, "OSC:");
  int oscValueW = textWidth(gfx_, oscLabel);
  int oscValueMaxW = textWidth(gfx_, "super");
  int oscValueX = oscLabelX + oscLabelW + labelValueGap;
  if (osc_control_) {
    osc_control_->setValue(oscLabel);
    int oscFocusW = oscLabelW + labelValueGap + oscValueW;
    int oscFocusH = gfx_.fontHeight();
    osc_control_->setBoundaries(Rect(oscLabelX, oscSwitchesY,
                                     oscFocusW, oscFocusH));
  }

  // filter type control
  const char* filterLabel = pFilter.optionLabel();
  if (!filterLabel) filterLabel = "";
  int filterLabelX = oscValueX + oscValueMaxW + controlGap;
  int filterLabelW = textWidth(gfx_, "FLT:");
  int filterValueW = textWidth(gfx_, filterLabel);
  int filterValueMaxW = textWidth(gfx_, "lp-1");
  int filterValueX = filterLabelX + filterLabelW + labelValueGap;
  if (filter_control_) {
    filter_control_->setValue(filterLabel);
    int filterFocusW = filterLabelW + labelValueGap + filterValueW;
    int filterFocusH = gfx_.fontHeight();
    filter_control_->setBoundaries(Rect(filterLabelX, oscSwitchesY,
                                        filterFocusW, filterFocusH));
  }

  // distortion toggle control
  bool distortionEnabled = mini_acid_.is303DistortionEnabled(voice_index_);
  int distLabelX = filterValueX + filterValueMaxW + controlGap;
  int distLabelW = textWidth(gfx_, "DST:");
  int distLabelMaxW = textWidth(gfx_, "off");
  const char* distortionValue = distortionEnabled ? "on" : "off";
  int delayLabelX = distLabelX + distLabelW + labelValueGap + distLabelMaxW + controlGap;
  int delayLabelW = textWidth(gfx_, "DLY:");
  if (distortion_control_) {
    distortion_control_->setValue(distortionValue);
    int distortionValueW = textWidth(gfx_, distortionValue);
    int distortionFocusW = distLabelW + labelValueGap + distortionValueW;
    int distortionFocusH = gfx_.fontHeight();
    distortion_control_->setBoundaries(Rect(distLabelX, oscSwitchesY,
                                            distortionFocusW, distortionFocusH));
  }

  // delay toggle control
  bool delayEnabled = mini_acid_.is303DelayEnabled(voice_index_);
  const char* delayValue = delayEnabled ? "on" : "off";
  // int delayLabelW = textWidth(gfx_, "DLY:");
  // int delayValueMaxW = textWidth(gfx_, "off");
  if (delay_control_) {
    delay_control_->setValue(delayValue);
    int delayValueW = textWidth(gfx_, delayValue);
    int delayFocusW = delayLabelW + labelValueGap + delayValueW;
    int delayFocusH = gfx_.fontHeight();
    delay_control_->setBoundaries(Rect(delayLabelX, oscSwitchesY,
                                       delayFocusW, delayFocusH));
  }

  // Mode badge
  GrooveboxMode mode = mini_acid_.grooveboxMode();
  uint16_t modeColor = (mode == GrooveboxMode::Acid) ? kAcidConfig.accentColor : kMinimalConfig.accentColor;
  const char* modeName = (mode == GrooveboxMode::Acid) ? "ACID" : "MINIMAL";
  int badgeW = textWidth(gfx_, modeName) + 6;
  int badgeX = dx() + width() - badgeW - 2;
  gfx_.fillRect(badgeX, dy(), badgeW, 10, (IGfxColor)modeColor);
  gfx_.setTextColor(COLOR_BLACK);
  gfx_.drawText(badgeX + 3, dy() + 2, modeName);

  // Hint (simplified - removed presets section for space)
  gfx_.setTextColor(COLOR_LABEL);
  gfx_.drawText(dx() + 6, dy() + height() - 10, "[M] Mode  [1-4] Preset  [G] Random");

  // finally draw all child components
  Container::draw(gfx_);
}

void Synth303ParamsPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

const std::string& Synth303ParamsPage::getTitle() const
{
  return title_;
}

bool Synth303ParamsPage::handleEvent(UIEvent& ui_event) 
{
  if (ui_event.event_type != MINIACID_KEY_DOWN) {
    return Container::handleEvent(ui_event);
  }

  switch (ui_event.scancode) {
    case MINIACID_LEFT:
      focusPrev();
      return true;
    case MINIACID_RIGHT:
      focusNext();
      return true;
    case MINIACID_UP:
      adjustFocusedElement(1);
      return true;
    case MINIACID_DOWN:
      adjustFocusedElement(-1);
      return true;
    default:
      break;
  }

  bool event_handled = false;
  switch(ui_event.key){
    case 't':
      withAudioGuard([&]() {
        mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, 1, voice_index_);
      });
      event_handled = true;
      break;
    case 'g':
      withAudioGuard([&]() {
        mini_acid_.adjust303Parameter(TB303ParamId::Oscillator, -1, voice_index_);
      });
      event_handled = true;
      break;
    case 'a':
      if (cutoff_knob_) {
        cutoff_knob_->setValue(1);
      }
      event_handled = true;
      break;
    case 'z':
      if (cutoff_knob_) {
        cutoff_knob_->setValue(-1);
      }
      event_handled = true;
      break;
    case 's':
      if (resonance_knob_) {
        resonance_knob_->setValue(1);
      }
      event_handled = true;
      break;
    case 'x':
      if (resonance_knob_) {
        resonance_knob_->setValue(-1);
      }
      event_handled = true;
      break;
    case 'd':
      if (env_amount_knob_) {
        env_amount_knob_->setValue(1);
      }
      event_handled = true;
      break;
    case 'c':
      if (env_amount_knob_) {
        env_amount_knob_->setValue(-1);
      }
      event_handled = true;
      break;
    case 'f':
      if (env_decay_knob_) {
        env_decay_knob_->setValue(1);
      }
      event_handled = true;
      break;
    case 'v':
      if (env_decay_knob_) {
        env_decay_knob_->setValue(-1);
      }
      event_handled = true;
      break;
    case 'm':
    case 'M':
      withAudioGuard([&]() { 
        mini_acid_.toggleGrooveboxMode();
        loadModePreset(0);
      });
      event_handled = true;
      break;
    case 'n':
      withAudioGuard([&]() {
        mini_acid_.toggleDistortion303(voice_index_);
      });
      event_handled = true;
      break;
    case '1':
    case '2':
    case '3':
    case '4':
      loadModePreset(ui_event.key - '1');
      event_handled = true;
      break;
    default:
      break;
  }
  if (event_handled) {
    return true;
  }
  return Container::handleEvent(ui_event);
}

void Synth303ParamsPage::loadModePreset(int index) {
  withAudioGuard([&]() {
    mini_acid_.modeManager().apply303Preset(voice_index_, index);
    current_preset_index_ = index;
  });
}


std::unique_ptr<MultiPageHelpDialog> Synth303ParamsPage::getHelpDialog() {
  auto dialog = std::make_unique<MultiPageHelpDialog>(*this);
  return dialog;
}

int Synth303ParamsPage::getHelpFrameCount() const {
  return 1;
}

void Synth303ParamsPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPage303(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}
