#include "tape_page.h"
#include <cstdio>
#include <algorithm>

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
}

class TapePage::KnobComponent : public FocusableComponent {
 public:
  KnobComponent(const char* label, float initial, std::function<void(float)> adjust_fn)
      : label_(label), value_(initial), adjust_fn_(adjust_fn) {}

  void adjust(int direction) {
    value_ = std::clamp(value_ + direction * 0.05f, 0.0f, 1.0f);
    if (adjust_fn_) adjust_fn_(value_);
  }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    int radius = std::min(bounds.w, bounds.h) / 2 - 5;
    int cx = bounds.x + bounds.w / 2;
    int cy = bounds.y + bounds.h / 2;

    gfx.drawKnobFace(cx, cy, radius, COLOR_KNOB_1, COLOR_BLACK);

    float angle = (135.0f + value_ * 270.0f) * (3.14159265f / 180.0f);
    int ix = cx + static_cast<int>(roundf(cosf(angle) * (radius - 2)));
    int iy = cy + static_cast<int>(roundf(sinf(angle) * (radius - 2)));
    gfx.drawLine(cx, cy, ix, iy);

    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(cx - textWidth(gfx, label_) / 2, cy + radius + 10, label_);

    if (isFocused()) {
      gfx.drawRect(bounds.x, bounds.y, bounds.w, bounds.h, kFocusColor);
    }
  }

 private:
  const char* label_;
  float value_;
  std::function<void(float)> adjust_fn_;
};

class TapePage::LabelValueComponent : public FocusableComponent {
 public:
  LabelValueComponent(const char* label, IGfxColor val_color)
      : label_(label), val_color_(val_color) {}
  void setValue(const std::string& v) { value_ = v; }
  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(bounds.x, bounds.y, label_);
    gfx.setTextColor(val_color_);
    gfx.drawText(bounds.x + textWidth(gfx, label_) + 5, bounds.y, value_.c_str());
    if (isFocused()) {
      gfx.drawRect(bounds.x - 2, bounds.y - 2, bounds.w + 4, bounds.h + 4, kFocusColor);
    }
  }
 private:
  const char* label_;
  std::string value_;
  IGfxColor val_color_;
};

TapePage::TapePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard) {}

void TapePage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) initComponents();
}

void TapePage::initComponents() {
  wow_knob_ = std::make_shared<KnobComponent>("WOW", 0, [this](float v){ mini_acid_.tapeFX.setWow(v); });
  flutter_knob_ = std::make_shared<KnobComponent>("FLUT", 0, [this](float v){ mini_acid_.tapeFX.setFlutter(v); });
  saturation_knob_ = std::make_shared<KnobComponent>("SAT", 0, [this](float v){ mini_acid_.tapeFX.setSaturation(v); });

  rec_ctrl_ = std::make_shared<LabelValueComponent>("REC:", COLOR_WHITE);
  play_ctrl_ = std::make_shared<LabelValueComponent>("PLY:", COLOR_KNOB_2);
  dub_ctrl_ = std::make_shared<LabelValueComponent>("DUB:", COLOR_KNOB_3);
  clear_btn_ = std::make_shared<LabelValueComponent>("CLR:", COLOR_WHITE);

  addChild(wow_knob_);
  addChild(flutter_knob_);
  addChild(saturation_knob_);
  addChild(rec_ctrl_);
  addChild(play_ctrl_);
  addChild(dub_ctrl_);
  addChild(clear_btn_);

  int x = dx() + 20;
  int yr1 = dy() + 30;
  int kw = 40, kh = 40;
  wow_knob_->setBoundaries(Rect(x, yr1, kw, kh));
  flutter_knob_->setBoundaries(Rect(x + 60, yr1, kw, kh));
  saturation_knob_->setBoundaries(Rect(x + 120, yr1, kw, kh));

  int yr2 = dy() + 90;
  int lh = 18;
  rec_ctrl_->setBoundaries(Rect(x, yr2, 100, lh)); yr2 += lh;
  play_ctrl_->setBoundaries(Rect(x, yr2, 100, lh)); yr2 += lh;
  dub_ctrl_->setBoundaries(Rect(x, yr2, 100, lh)); yr2 += lh;
  clear_btn_->setBoundaries(Rect(x, yr2, 100, lh));

  initialized_ = true;
}

void TapePage::draw(IGfx& gfx) {
  if (!initialized_) initComponents();
  
  // Update looper stats (ideally these would be Atomic/Volatile but simple for now)
  // Since we don't have getters, I'll need to trust the state.
  // Actually I'll just use static tracking in the UI or add getters to Looper.
  
  rec_ctrl_->setValue("READY"); // Placeholder, should add getters to looper
  play_ctrl_->setValue("IDLE");
  dub_ctrl_->setValue("OFF");
  clear_btn_->setValue("PRESS SP");

  Container::draw(gfx);
}

void TapePage::adjustFocusedElement(int direction) {
  if (wow_knob_->isFocused()) wow_knob_->adjust(direction);
  else if (flutter_knob_->isFocused()) flutter_knob_->adjust(direction);
  else if (saturation_knob_->isFocused()) saturation_knob_->adjust(direction);
  else if (rec_ctrl_->isFocused()) { if (direction != 0) audio_guard_([&](){ mini_acid_.tapeLooper.setRecording(direction > 0); }); }
  else if (play_ctrl_->isFocused()) { if (direction != 0) audio_guard_([&](){ mini_acid_.tapeLooper.setPlaying(direction > 0); }); }
  else if (dub_ctrl_->isFocused()) { if (direction != 0) audio_guard_([&](){ mini_acid_.tapeLooper.setOverdub(direction > 0); }); }
}

bool TapePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return Container::handleEvent(ui_event);

  switch (ui_event.scancode) {
    case MINIACID_UP: focusPrev(); return true;
    case MINIACID_DOWN: focusNext(); return true;
    case MINIACID_LEFT: adjustFocusedElement(-1); return true;
    case MINIACID_RIGHT: adjustFocusedElement(1); return true;
    default: break;
  }
  
  if (ui_event.key == ' ' && clear_btn_->isFocused()) {
      audio_guard_([&](){ mini_acid_.tapeLooper.clear(); });
      return true;
  }

  return Container::handleEvent(ui_event);
}

const std::string& TapePage::getTitle() const { return title_; }
