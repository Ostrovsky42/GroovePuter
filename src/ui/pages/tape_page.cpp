#include "tape_page.h"
#include "../../dsp/tape_presets.h"
#include "../ui_common.h"
#ifdef USE_RETRO_THEME
#include "../retro_ui_theme.h"
#include "../retro_widgets.h"
#endif
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
inline constexpr uint8_t kWashSpace = 62;
inline constexpr uint8_t kWashMovement = 55;
inline constexpr uint8_t kWashGroove = 58;

TapeMode keyToTapeMode(char lowerKey) {
  switch (lowerKey) {
    case 'z': return TapeMode::Stop;
    case 'c': return TapeMode::Dub;
    case 'v': return TapeMode::Play;
    default: return TapeMode::Stop;
  }
}

bool isDirectModeKey(char lowerKey) {
  return lowerKey == 'z' || lowerKey == 'c' || lowerKey == 'v';
}
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

    if (UI::currentStyle == VisualStyle::RETRO_CLASSIC) {
#ifdef USE_RETRO_THEME
      // Cyber meter: segmented bar instead of plain slider.
      gfx.setTextColor(isFocused() ? IGfxColor(RetroTheme::NEON_CYAN) : IGfxColor(RetroTheme::TEXT_SECONDARY));
      gfx.drawText(bounds.x, bounds.y, label_);

      int labelW = gfx.textWidth(label_);
      int meterX = bounds.x + labelW + 6;
      int meterW = bounds.w - labelW - 34;
      int meterY = bounds.y + 2;
      int meterH = 6;
      int segments = 12;
      int gap = 1;
      int segW = (meterW - (segments - 1) * gap) / segments;
      if (segW < 2) segW = 2;
      int lit = (value_ * segments) / maxValue_;
      if (value_ > 0 && lit == 0) lit = 1;

      for (int i = 0; i < segments; ++i) {
        int sx = meterX + i * (segW + gap);
        bool on = i < lit;
        IGfxColor c = on ? (isFocused() ? IGfxColor(RetroTheme::NEON_CYAN) : IGfxColor(RetroTheme::NEON_MAGENTA))
                         : IGfxColor(RetroTheme::GRID_DIM);
        gfx.fillRect(sx, meterY, segW, meterH, c);
      }
      gfx.drawRect(meterX - 1, meterY - 1, meterW + 2, meterH + 2, IGfxColor(RetroTheme::GRID_MEDIUM));

      char buf[12];
      if (maxValue_ == 100) std::snprintf(buf, sizeof(buf), "%d%%", value_);
      else std::snprintf(buf, sizeof(buf), "%d", value_);
      gfx.setTextColor(IGfxColor(RetroTheme::TEXT_PRIMARY));
      gfx.drawText(meterX + meterW + 4, bounds.y, buf);

      if (isFocused()) {
        RetroWidgets::drawGlowBorder(gfx, bounds.x - 2, bounds.y - 1, bounds.w + 4, bounds.h + 2,
                                     IGfxColor(RetroTheme::NEON_CYAN), 1);
      }
#else
      gfx.setTextColor(isFocused() ? COLOR_KNOB_1 : COLOR_LABEL);
      gfx.drawText(bounds.x, bounds.y, label_);
#endif
      return;
    }

    // Default minimal/amber rendering
    gfx.setTextColor(isFocused() ? COLOR_KNOB_1 : COLOR_LABEL);
    gfx.drawText(bounds.x, bounds.y, label_);
    int labelW = gfx.textWidth(label_);
    int barX = bounds.x + labelW + 6;
    int barW = bounds.w - labelW - 35;
    int barY = bounds.y + 3;
    int barH = 4;
    gfx.fillRect(barX, barY, barW, barH, COLOR_BLACK);
    int fillW = (barW * value_) / maxValue_;
    gfx.fillRect(barX, barY, fillW, barH, isFocused() ? COLOR_KNOB_1 : COLOR_KNOB_2);
    char buf[12];
    if (maxValue_ == 100) std::snprintf(buf, sizeof(buf), "%d%%", value_);
    else std::snprintf(buf, sizeof(buf), "%d", value_);
    gfx.drawText(barX + barW + 4, bounds.y, buf);
    if (isFocused()) gfx.drawRect(bounds.x - 2, bounds.y - 1, bounds.w + 4, bounds.h + 2, kFocusColor);
  }

 private:
  const char* label_;
  int value_;
  int maxValue_;
  std::function<void(int)> change_fn_;
};

class TapePage::ModeComponent : public FocusableComponent {
 public:
  ModeComponent(MiniAcid& synth, AudioGuard guard) : synth_(synth), guard_(guard) {}

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
      synth_.tapeLooper->setMode(tape.mode);
    });
  }

 private:
  MiniAcid& synth_;
  AudioGuard guard_;
};

class TapePage::PresetComponent : public FocusableComponent {
 public:
  PresetComponent(MiniAcid& synth, AudioGuard guard) : synth_(synth), guard_(guard) {}

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
      synth_.tapeFX->applyMacro(tape.macro);
    });
  }

 private:
  MiniAcid& synth_;
  AudioGuard guard_;
};

TapePage::TapePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard), waveform_(gfx) {}

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
      mini_acid_.tapeFX->applyMacro(macro);
    });
  };

  wow_slider_ = std::make_shared<SliderComponent>("WOW", 12, 100, [=](int v){ updateMacro(0, v); });
  age_slider_ = std::make_shared<SliderComponent>("AGE", 20, 100, [=](int v){ updateMacro(1, v); });
  sat_slider_ = std::make_shared<SliderComponent>("SAT", 35, 100, [=](int v){ updateMacro(2, v); });
  tone_slider_ = std::make_shared<SliderComponent>("TONE", 60, 100, [=](int v){ updateMacro(3, v); });
  crush_slider_ = std::make_shared<SliderComponent>("CRUSH", 0, 3, [=](int v){ updateMacro(4, v); });
  looper_slider_ = std::make_shared<SliderComponent>("LOOP", 55, 100, [this](int v){
    audio_guard_([this, v]() {
      TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
      tape.looperVolume = static_cast<float>(v) / 100.0f;
      mini_acid_.tapeLooper->setVolume(tape.looperVolume);
    });
  });

  mode_ctrl_ = std::make_shared<ModeComponent>(mini_acid_, audio_guard_);
  preset_ctrl_ = std::make_shared<PresetComponent>(mini_acid_, audio_guard_);

  addChild(wow_slider_);
  addChild(age_slider_);
  addChild(sat_slider_);
  addChild(tone_slider_);
  addChild(crush_slider_);
  addChild(looper_slider_);
  addChild(mode_ctrl_);
  addChild(preset_ctrl_);

  int x = dx() + 5;
  int y = dy() + 5;
  int lh = 10;
  int sliderW = getBoundaries().w - 10;

  // Layout
  mode_ctrl_->setBoundaries(Rect(x, y, 80, lh)); 
  preset_ctrl_->setBoundaries(Rect(x + 85, y, 80, lh)); 
  y += lh + 2;

  wow_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 1;
  age_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 1;
  sat_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 1;
  tone_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 1;
  crush_slider_->setBoundaries(Rect(x, y, sliderW, lh)); y += lh + 1;
  looper_slider_->setBoundaries(Rect(x, y, sliderW, lh));

  initialized_ = true;
}

void TapePage::syncFromState() {
  const TapeMacro& macro = mini_acid_.sceneManager().currentScene().tape.macro;
  wow_slider_->setValue(macro.wow);
  age_slider_->setValue(macro.age);
  sat_slider_->setValue(macro.sat);
  tone_slider_->setValue(macro.tone);
  crush_slider_->setValue(macro.crush);
  looper_slider_->setValue(static_cast<int>(mini_acid_.sceneManager().currentScene().tape.looperVolume * 100.0f + 0.5f));
}

void TapePage::draw(IGfx& gfx) {
  if (!initialized_) initComponents();
#ifdef USE_RETRO_THEME
  if (UI::currentStyle == VisualStyle::RETRO_CLASSIC) {
    const Rect area = getBoundaries();
    gfx.fillRect(area.x, area.y, area.w, area.h, IGfxColor(RetroTheme::BG_DEEP_BLACK));
    for (int gy = area.y; gy < area.y + area.h; gy += 8) {
      gfx.drawLine(area.x, gy, area.x + area.w - 1, gy, IGfxColor(RetroTheme::SCANLINE_COLOR));
    }
    gfx.drawRect(area.x + 2, area.y + 2, area.w - 4, area.h - 4, IGfxColor(RetroTheme::GRID_MEDIUM));
  }
#endif
  syncFromState();
  updateAnimations();
  drawCassette(gfx);
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

  char volBuf[12];
  std::snprintf(volBuf, sizeof(volBuf), "LVL:%d%%", static_cast<int>(tape.looperVolume * 100.0f + 0.5f));
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x + 52, y, volBuf);

  // Mode indicator (explicit so it is visible even when mode row is unfocused)
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x + 96, y, "MD:");
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x + 116, y, tapeModeName(tape.mode));
  
  // Recorder/loop status
  if (tape.mode == TapeMode::Rec) {
    char buf[16];
    gfx.setTextColor(IGfxColor(0xFF3030));
    gfx.drawText(x + 146, y, "REC:");
    if (mini_acid_.tapeLooper->isFirstRecordPass()) {
      std::snprintf(buf, sizeof(buf), "%.1fs", mini_acid_.tapeLooper->recordElapsedSeconds());
    } else if (mini_acid_.tapeLooper->hasLoop()) {
      std::snprintf(buf, sizeof(buf), "OVR");
    } else {
      std::snprintf(buf, sizeof(buf), "ARM");
    }
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 174, y, buf);
  } else if (mini_acid_.tapeLooper->hasLoop()) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1fs", mini_acid_.tapeLooper->loopLengthSeconds());
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x + 146, y, "LEN:");
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 174, y, buf);
  }

  const bool safeDub = mini_acid_.tapeLooper->dubAutoExit();
  UI::drawStandardFooter(
      gfx,
      safeDub ? "X SMART SAFE:DUB1  A CAP  S THK" : "X SMART  A CAP  S THK  D WSH  G MUTE",
      "Z STOP C DUB V PLAY 1/2/3 SPD F FX");
}

bool TapePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return Container::handleEvent(ui_event);

  bool shift = ui_event.shift;

  switch (ui_event.scancode) {
    case GROOVEPUTER_UP: focusPrev(); return true;
    case GROOVEPUTER_DOWN: focusNext(); return true;
    case GROOVEPUTER_LEFT:
    case GROOVEPUTER_RIGHT: {
      int dir = (ui_event.scancode == GROOVEPUTER_RIGHT) ? 1 : -1;
      // Adjust focused slider
      if (wow_slider_->isFocused()) wow_slider_->adjust(dir, shift);
      else if (age_slider_->isFocused()) age_slider_->adjust(dir, shift);
      else if (sat_slider_->isFocused()) sat_slider_->adjust(dir, shift);
      else if (tone_slider_->isFocused()) tone_slider_->adjust(dir, shift);
      else if (crush_slider_->isFocused()) crush_slider_->adjust(dir, shift);
      else if (looper_slider_->isFocused()) looper_slider_->adjust(dir, shift);
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

  auto setTapeMode = [this](TapeMode mode) {
    audio_guard_([this, mode]() {
      TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
      tape.mode = mode;
      mini_acid_.tapeLooper->setDubAutoExit(false);
      mini_acid_.tapeLooper->setMode(mode);
    });
  };
  
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
  if (lowerKey == 'x' && !ui_event.ctrl && !ui_event.alt && !ui_event.shift && !ui_event.meta) {
    audio_guard_([this]() {
      TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
      const bool hasLoop = mini_acid_.tapeLooper->hasLoop();
      const bool isFirstRec = mini_acid_.tapeLooper->isFirstRecordPass();

      if (!hasLoop) {
        // No loop yet: X arms/starts REC, second X closes take into PLAY.
        tape.mode = (tape.mode == TapeMode::Rec && isFirstRec) ? TapeMode::Play : TapeMode::Rec;
        mini_acid_.tapeLooper->setDubAutoExit(false);
      } else {
        // Existing loop: X toggles PLAY <-> DUB as performance workflow.
        if (tape.mode == TapeMode::Dub) {
          tape.mode = TapeMode::Play;
          mini_acid_.tapeLooper->setDubAutoExit(false);
        } else {
          tape.mode = TapeMode::Dub;
          mini_acid_.tapeLooper->setDubAutoExit(true);  // safety: one cycle only
        }
      }
      mini_acid_.tapeLooper->setMode(tape.mode);
    });
    return true;
  }

  if (isDirectModeKey(lowerKey)) {
    setTapeMode(keyToTapeMode(lowerKey));
    return true;
  }

  // Performance actions for small-screen live workflow.
  if (lowerKey == 'a' && !ui_event.ctrl && !ui_event.alt && !ui_event.shift && !ui_event.meta) {
    audio_guard_([this]() {
      TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
      mini_acid_.tapeLooper->clear();
      mini_acid_.tapeLooper->setDubAutoExit(false);
      tape.mode = TapeMode::Rec;
      tape.fxEnabled = true;
      mini_acid_.tapeLooper->setMode(tape.mode);
    });
    UI::showToast("CAPTURE: REC", 1000);
    return true;
  }
  if (lowerKey == 's' && !ui_event.ctrl && !ui_event.alt && !ui_event.shift && !ui_event.meta) {
    audio_guard_([this]() {
      TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
      if (!mini_acid_.tapeLooper->hasLoop()) return;
      tape.mode = TapeMode::Dub;
      mini_acid_.tapeLooper->setDubAutoExit(true);  // one cycle safety
      mini_acid_.tapeLooper->setMode(tape.mode);
    });
    if (mini_acid_.tapeLooper->hasLoop()) UI::showToast("THICKEN: DUB x1", 900);
    else UI::showToast("THICKEN: NO LOOP", 900);
    return true;
  }
  if (lowerKey == 'd' && !ui_event.ctrl && !ui_event.alt && !ui_event.shift && !ui_event.meta) {
    audio_guard_([this]() {
      TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
      if (!perf_wash_active_) {
        perf_prev_space_ = tape.space;
        perf_prev_movement_ = tape.movement;
        perf_prev_groove_ = tape.groove;
        // Wash preset: reverby, moving, groove-heavy.
        tape.space = kWashSpace;
        tape.movement = kWashMovement;
        tape.groove = kWashGroove;
        tape.fxEnabled = true;
        perf_wash_active_ = true;
      } else {
        tape.space = perf_prev_space_;
        tape.movement = perf_prev_movement_;
        tape.groove = perf_prev_groove_;
        perf_wash_active_ = false;
      }
      mini_acid_.tapeFX->applyMinimalParams(tape.space, tape.movement, tape.groove);
    });
    UI::showToast(perf_wash_active_ ? "WASH: ON" : "WASH: OFF", 900);
    return true;
  }
  if (lowerKey == 'g' && !ui_event.ctrl && !ui_event.alt && !ui_event.shift && !ui_event.meta) {
    audio_guard_([this]() {
      TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
      if (!perf_loop_muted_) {
        perf_prev_loop_volume_ = tape.looperVolume;
        tape.looperVolume = 0.0f;
        perf_loop_muted_ = true;
      } else {
        tape.looperVolume = perf_prev_loop_volume_;
        perf_loop_muted_ = false;
      }
      mini_acid_.tapeLooper->setVolume(tape.looperVolume);
    });
    UI::showToast(perf_loop_muted_ ? "LOOP: MUTED" : "LOOP: UNMUTED", 900);
    return true;
  }

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
      UI::showToast(mini_acid_.sceneManager().currentScene().tape.fxEnabled ? "FX: ON" : "FX: OFF", 900);
      return true;
    case '1':
      audio_guard_([this](){
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.speed = 0; // 0.5x
        mini_acid_.tapeLooper->setSpeed(tape.speed);
      });
      return true;
    case '2':
      audio_guard_([this](){
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.speed = 1; // 1.0x
        mini_acid_.tapeLooper->setSpeed(tape.speed);
      });
      return true;
    case '3':
      audio_guard_([this](){
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.speed = 2; // 2.0x
        mini_acid_.tapeLooper->setSpeed(tape.speed);
      });
      return true;
    case '\n': // Enter = stutter toggle
      audio_guard_([this](){
        const bool active = mini_acid_.tapeLooper->stutterActive();
        mini_acid_.tapeLooper->setStutter(!active);
      });
      return true;
    case '\b': // Backspace/Del = eject
    case 0x7F:
      audio_guard_([this](){
        mini_acid_.tapeLooper->eject();
        TapeState& tape = mini_acid_.sceneManager().currentScene().tape;
        tape.mode = TapeMode::Stop;
        tape.fxEnabled = false;
      });
      return true;
    case ' ':
      audio_guard_([this](){
        mini_acid_.tapeLooper->clear();
      });
      return true;
  }

  return Container::handleEvent(ui_event);
}

const std::string& TapePage::getTitle() const { return title_; }

void TapePage::updateAnimations() {
  if (millis() - last_frame_time_ >= kFrameDelay) {
    TapeMode mode = mini_acid_.sceneManager().currentScene().tape.mode;
    if (mode == TapeMode::Play || mode == TapeMode::Rec || mode == TapeMode::Dub) {
      reel_rotation_ += 0.3f;
      if (reel_rotation_ >= 6.28318f) reel_rotation_ -= 6.28318f;
    }
    
    // Update waveform data from audio engine
    const auto& buffer = mini_acid_.getWaveformBuffer();
    waveform_.setWaveData(buffer.data, buffer.count);
    
    last_frame_time_ = millis();
  }
}

void TapePage::drawCassette(IGfx& gfx) {
  const Rect area = getBoundaries();
  // Compact cassette layout for Cardputer (240x135)
  // Positioned in the remaining space below sliders
  int cx = area.x + area.w / 2 - 80;
  int cy = area.y + 78;
  int cw = 160;
  int ch = 42;
  
  // Body background
  IGfxColor bodyColor(0x333333);
  gfx.fillRect(cx, cy, cw, ch, bodyColor);
  gfx.drawRect(cx, cy, cw, ch, IGfxColor::Gray());
  
  // Sticker area
  int sx = cx + 35;
  int sy = cy + 4;
  int sw = 90;
  int sh = 28;
  gfx.fillRect(sx, sy, sw, sh, IGfxColor(0x1a1a1a));
  
  // Waveform visualization on sticker
  waveform_.drawWaveformInRegion(Rect(sx, sy, sw, sh), IGfxColor::Green());
  
  // Reels
  drawReel(gfx, cx + 18, cy + 20, 14, reel_rotation_);
  drawReel(gfx, cx + cw - 18, cy + 20, 14, -reel_rotation_);
}

void TapePage::drawReel(IGfx& gfx, int x, int y, int radius, float rotation) {
  gfx.drawCircle(x, y, radius, IGfxColor::White());
  gfx.fillCircle(x, y, 4, IGfxColor::White());
  
  // Mechanical hub spokes
  for (int i = 0; i < 3; i++) {
    float angle = rotation + i * (2.09439f); // 2*PI/3
    int x1 = x + cos(angle) * (radius - 2);
    int y1 = y + sin(angle) * (radius - 2);
    gfx.drawLine(x, y, x1, y1, IGfxColor::White());
  }
}

void TapePage::drawTape(IGfx& gfx) {
    // Placeholder for tape path if needed later
}
