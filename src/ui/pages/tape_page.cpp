#include "tape_page.h"
#include "../../dsp/tape_presets.h"
#include <cstdio>
#include <algorithm>

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
}

class TapePage::SliderComponent : public FocusableComponent {
 public:
  SliderComponent(const char* label, int initial, int maxVal, std::function<void(int)> change_fn)
      : label_(label), value_(initial), maxValue_(maxVal), change_fn_(change_fn) {}

  void adjust(int direction, bool fine = false) {
    int step = fine ? 1 : 5;
    value_ = std::clamp(value_ + direction * step, 0, maxValue_);
    if (change_fn_) change_fn_(value_);
  }

  void setValue(int v) { value_ = std::clamp(v, 0, maxValue_); }
  int value() const { return value_; }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    
    // Label
    gfx.setTextColor(isFocused() ? COLOR_KNOB_1 : COLOR_LABEL);
    gfx.drawText(bounds.x, bounds.y, label_);
    
    // Slider bar (dynamic layout)
    int labelW = gfx.textWidth(label_);
    int barX = bounds.x + labelW + 6;
    int barW = bounds.w - labelW - 35; // Save space for value text
    int barY = bounds.y + 3;
    int barH = 4;
    
    // Background
    gfx.fillRect(barX, barY, barW, barH, COLOR_BLACK);
    
    // Fill
    int fillW = (barW * value_) / maxValue_;
    gfx.fillRect(barX, barY, fillW, barH, isFocused() ? COLOR_KNOB_1 : COLOR_KNOB_2);
    
    // Value text
    char buf[12];
    if (maxValue_ == 100) {
        snprintf(buf, sizeof(buf), "%d%%", value_);
    } else {
        snprintf(buf, sizeof(buf), "%d", value_);
    }
    gfx.drawText(barX + barW + 4, bounds.y, buf);
    
    // Focus indicator
    if (isFocused()) {
      gfx.drawRect(bounds.x - 2, bounds.y - 1, bounds.w + 4, bounds.h + 2, kFocusColor);
    }
  }

 private:
  const char* label_;
  int value_;
  int maxValue_;
  std::function<void(int)> change_fn_;
};

class TapePage::ModeComponent : public FocusableComponent {
 public:
  ModeComponent(MiniAcid& synth, AudioGuard& guard) : synth_(synth), guard_(guard) {}

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    TapeMode mode = synth_.sceneManager().currentScene().tape.mode;
    const char* modeStr = tapeModeName(mode);
    
    // Mode indicator with color
    IGfxColor modeColor;
    switch(mode) {
      case TapeMode::Stop: modeColor = COLOR_LABEL; break;
      case TapeMode::Rec:  modeColor = IGfxColor(0xFF2020); break;
      case TapeMode::Dub:  modeColor = IGfxColor(0xFF8800); break;
      case TapeMode::Play: modeColor = IGfxColor(0x20FF20); break;
    }
    
    gfx.setTextColor(isFocused() ? COLOR_WHITE : COLOR_LABEL);
    gfx.drawText(bounds.x, bounds.y, "MODE:");
    gfx.setTextColor(modeColor);
    gfx.drawText(bounds.x + 35, bounds.y, modeStr);
    
    if (isFocused()) {
      gfx.drawRect(bounds.x - 2, bounds.y - 1, bounds.w + 4, bounds.h + 2, kFocusColor);
    }
  }

  void cycleMode() {
    guard_([this](){
      TapeState& tape = synth_.sceneManager().currentScene().tape;
      tape.mode = nextTapeMode(tape.mode);
      synth_.tapeLooper.setMode(tape.mode);
    });
  }

 private:
  MiniAcid& synth_;
  AudioGuard& guard_;
};

class TapePage::PresetComponent : public FocusableComponent {
 public:
  PresetComponent(MiniAcid& synth, AudioGuard& guard) : synth_(synth), guard_(guard) {}

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    TapeState& tape = synth_.sceneManager().currentScene().tape;
    int count;
    const TapeModePreset* presets = synth_.modeManager().getTapePresets(count);
    const char* pName = "LEGACY";
    if (presets && static_cast<int>(tape.preset) < count) {
      pName = presets[static_cast<int>(tape.preset)].name;
    } else {
      pName = tapePresetName(tape.preset);
    }

    gfx.setTextColor(isFocused() ? COLOR_WHITE : COLOR_LABEL);
    gfx.drawText(bounds.x, bounds.y, "PRESET:");
    gfx.setTextColor(COLOR_KNOB_2);
    gfx.drawText(bounds.x + 50, bounds.y, pName);
    
    if (isFocused()) {
      gfx.drawRect(bounds.x - 2, bounds.y - 1, bounds.w + 4, bounds.h + 2, kFocusColor);
    }
  }

  void cyclePreset() {
    guard_([this](){
      TapeState& tape = synth_.sceneManager().currentScene().tape;
      int count;
      const TapeModePreset* presets = synth_.modeManager().getTapePresets(count);
      
      int nextIdx = (static_cast<int>(tape.preset) + 1) % (count > 0 ? count : 4);
      tape.preset = static_cast<TapePreset>(nextIdx);
      
      if (presets && nextIdx < count) {
        tape.macro = presets[nextIdx].macro;
      } else {
        loadTapePreset(tape.preset, tape.macro);
      }
      synth_.tapeFX.applyMacro(tape.macro);
    });
  }

 private:
  MiniAcid& synth_;
  AudioGuard& guard_;
};

TapePage::TapePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard) {}

void TapePage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) initComponents();
}

void TapePage::initComponents() {
  // Macro sliders
  auto updateMacro = [this](int idx, int val) {
    audio_guard_([this, idx, val](){
      TapeMacro& macro = mini_acid_.sceneManager().currentScene().tape.macro;
      switch(idx) {
        case 0: macro.wow = val; break;
        case 1: macro.age = val; break;
        case 2: macro.sat = val; break;
        case 3: macro.tone = val; break;
        case 4: macro.crush = val; break;
      }
      mini_acid_.tapeFX.applyMacro(macro);
    });
  };

  wow_slider_ = std::make_shared<SliderComponent>("WOW", 12, 100, [=](int v){ updateMacro(0, v); });
  age_slider_ = std::make_shared<SliderComponent>("AGE", 20, 100, [=](int v){ updateMacro(1, v); });
  sat_slider_ = std::make_shared<SliderComponent>("SAT", 35, 100, [=](int v){ updateMacro(2, v); });
  tone_slider_ = std::make_shared<SliderComponent>("TONE", 60, 100, [=](int v){ updateMacro(3, v); });
  crush_slider_ = std::make_shared<SliderComponent>("CRUSH", 0, 3, [=](int v){ updateMacro(4, v); });

  mode_ctrl_ = std::make_shared<ModeComponent>(mini_acid_, audio_guard_);
  preset_ctrl_ = std::make_shared<PresetComponent>(mini_acid_, audio_guard_);

  addChild(wow_slider_);
  addChild(age_slider_);
  addChild(sat_slider_);
  addChild(tone_slider_);
  addChild(crush_slider_);
  addChild(mode_ctrl_);
  addChild(preset_ctrl_);

  int x = dx() + 5;
  int y = dy() + 5;
  int lh = 12;
  int sliderW = getBoundaries().w - 10;

  // Layout
  mode_ctrl_->setBoundaries(Rect(x, y, 80, lh)); 
  preset_ctrl_->setBoundaries(Rect(x + 85, y, 80, lh)); 
  y += lh + 4;

  wow_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 2;
  age_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 2;
  sat_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 2;
  tone_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 2;
  crush_slider_->setBoundaries(Rect(x, y, sliderW, lh));

  initialized_ = true;
}

void TapePage::syncFromState() {
  const TapeMacro& macro = mini_acid_.sceneManager().currentScene().tape.macro;
  wow_slider_->setValue(macro.wow);
  age_slider_->setValue(macro.age);
  sat_slider_->setValue(macro.sat);
  tone_slider_->setValue(macro.tone);
  crush_slider_->setValue(macro.crush);
}

void TapePage::draw(IGfx& gfx) {
  if (!initialized_) initComponents();
  syncFromState();
  Container::draw(gfx);
  
  // Draw looper info at bottom
  int y = dy() + getBoundaries().h - 14;
  int x = dx() + 5;
  
  const TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
  
  // Speed indicator
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, y, "SPD:");
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x + 28, y, tapeSpeedName(tape.speed));
  
  // Loop length
  if (mini_acid_.tapeLooper.hasLoop()) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1fs", mini_acid_.tapeLooper.loopLengthSeconds());
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x + 70, y, "LEN:");
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 98, y, buf);
  }
  
  // FX status
  gfx.setTextColor(tape.fxEnabled ? COLOR_KNOB_1 : COLOR_LABEL);
  gfx.drawText(x + 140, y, tape.fxEnabled ? "FX:ON" : "FX:--");
}

bool TapePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return Container::handleEvent(ui_event);

  bool shift = ui_event.shift;

  switch (ui_event.scancode) {
    case MINIACID_UP: focusPrev(); return true;
    case MINIACID_DOWN: focusNext(); return true;
    case MINIACID_LEFT:
    case MINIACID_RIGHT: {
      int dir = (ui_event.scancode == MINIACID_RIGHT) ? 1 : -1;
      // Adjust focused slider
      if (wow_slider_->isFocused()) wow_slider_->adjust(dir, shift);
      else if (age_slider_->isFocused()) age_slider_->adjust(dir, shift);
      else if (sat_slider_->isFocused()) sat_slider_->adjust(dir, shift);
      else if (tone_slider_->isFocused()) tone_slider_->adjust(dir, shift);
      else if (crush_slider_->isFocused()) crush_slider_->adjust(dir, shift);
      else if (mode_ctrl_->isFocused()) {
        std::static_pointer_cast<ModeComponent>(mode_ctrl_)->cycleMode();
      } else if (preset_ctrl_->isFocused()) {
        std::static_pointer_cast<PresetComponent>(preset_ctrl_)->cyclePreset();
      }
      return true;
    }
    default: break;
  }
  
  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)));
  
  // Q-I Pattern Selection (Standardized)
  if (!ui_event.shift && !ui_event.ctrl && !ui_event.meta) {
    const char* patternKeys = "qwertyui";
    const char* found = strchr(patternKeys, lowerKey);
    if (found) {
      int patternIdx = found - patternKeys;
      audio_guard_([&]() { mini_acid_.setDrumPatternIndex(patternIdx); });
      return true;
    }
  }

  // Hotkeys
  switch (lowerKey) {
    case 'p':
    case 'P':
      std::static_pointer_cast<PresetComponent>(preset_ctrl_)->cyclePreset();
      return true;
    case 'r':
    case 'R':
      std::static_pointer_cast<ModeComponent>(mode_ctrl_)->cycleMode();
      return true;
    case 'f':
    case 'F':
      audio_guard_([this](){
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.fxEnabled = !tape.fxEnabled;
      });
      return true;
    case '1':
      audio_guard_([this](){
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.speed = 0; // 0.5x
        mini_acid_.tapeLooper.setSpeed(tape.speed);
      });
      return true;
    case '2':
      audio_guard_([this](){
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.speed = 1; // 1.0x
        mini_acid_.tapeLooper.setSpeed(tape.speed);
      });
      return true;
    case '3':
      audio_guard_([this](){
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.speed = 2; // 2.0x
        mini_acid_.tapeLooper.setSpeed(tape.speed);
      });
      return true;
    case '\n': // Enter = stutter (TODO: hold detection)
      audio_guard_([this](){
        mini_acid_.tapeLooper.setStutter(true);
      });
      return true;
    case '\b': // Backspace/Del = eject
    case 0x7F:
      audio_guard_([this](){
        mini_acid_.tapeLooper.eject();
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.mode = TapeMode::Stop;
        tape.fxEnabled = false;
      });
      return true;
    case ' ':
      audio_guard_([this](){
        mini_acid_.tapeLooper.clear();
      });
      return true;
  }

  return Container::handleEvent(ui_event);
}

const std::string& TapePage::getTitle() const { return title_; }
