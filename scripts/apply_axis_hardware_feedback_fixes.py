#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path('.')


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding='utf-8')
    if old not in text:
        raise RuntimeError(f'missing expected text in {path}: {old[:100]!r}')
    path.write_text(text.replace(old, new, 1), encoding='utf-8')


def regex_once(path: Path, pattern: str, replacement: str) -> None:
    text = path.read_text(encoding='utf-8')
    text, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f'pattern did not match exactly once in {path}: {pattern[:100]!r}')
    path.write_text(text, encoding='utf-8')


# Shared hold acceleration for arrow-driven values.
ui_input = ROOT / 'src/ui/ui_input.h'
ui_text = ui_input.read_text(encoding='utf-8')
if 'class HoldAccelerator' not in ui_text:
    marker = '// Global navigation keys are reserved at the app level.\n'
    helper = '''class HoldAccelerator {
 public:
  int multiplier(int direction, bool forcedFast = false) {
    const uint32_t now = millis();
    if (direction == last_direction_ &&
        static_cast<uint32_t>(now - last_event_ms_) <= 160u) {
      if (streak_ < 32) ++streak_;
    } else {
      streak_ = 0;
    }
    last_direction_ = direction;
    last_event_ms_ = now;

    if (forcedFast) return 5;
    if (streak_ >= 10) return 5;
    if (streak_ >= 4) return 3;
    return 1;
  }

  void reset() {
    last_direction_ = 0;
    last_event_ms_ = 0;
    streak_ = 0;
  }

 private:
  int last_direction_ = 0;
  uint32_t last_event_ms_ = 0;
  uint8_t streak_ = 0;
};

'''
    if marker not in ui_text:
        raise RuntimeError('ui_input insertion marker missing')
    ui_input.write_text(ui_text.replace(marker, helper + marker, 1), encoding='utf-8')


feel_h = r'''#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "../ui_core.h"
#include "../ui_input.h"
#include "src/dsp/miniacid_engine.h"
#include "src/state/scene_revision.h"

class FeelPage : public IPage {
 public:
  FeelPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override { return title_; }
  void setVisualStyle(VisualStyle style) override { style_ = style; }

 private:
  enum class FocusRow : uint8_t {
    Swing = 0,
    TimingHumanize,
    VelocityHumanize,
    Preset,
  };

  void moveFocus(int delta);
  void adjustFocused(int delta, bool fast);
  void applyPreset(int index);

  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
    GroovePuterState::markSceneMutated();
  }

  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  VisualStyle style_ = VisualStyle::MINIMAL;
  FocusRow focus_ = FocusRow::Swing;
  int preset_index_ = 1;
  UIInput::HoldAccelerator hold_accel_;
  std::string title_ = "FEEL";
};
'''

feel_cpp = r'''#include "feel_page.h"

#include <algorithm>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"

namespace {
constexpr const char* kPresetNames[3] = {"TIGHT", "HUMAN", "LOOSE"};

int wrapIndex(int value, int count) {
  if (count <= 0) return 0;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return value;
}

int percent(float value) {
  return static_cast<int>(value * 100.0f + 0.5f);
}
}  // namespace

FeelPage::FeelPage(IGfx& gfx,
                   MiniAcid& mini_acid,
                   AudioGuard& audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
}

void FeelPage::moveFocus(int delta) {
  int value = static_cast<int>(focus_) + delta;
  value = wrapIndex(value, 4);
  focus_ = static_cast<FocusRow>(value);
  hold_accel_.reset();
}

void FeelPage::adjustFocused(int delta, bool fast) {
  if (focus_ == FocusRow::Preset) {
    preset_index_ = wrapIndex(preset_index_ + delta, 3);
    hold_accel_.reset();
    return;
  }

  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int multiplier = hold_accel_.multiplier(delta, fast);

  withAudioGuard([&]() {
    switch (focus_) {
      case FocusRow::Swing: {
        const int value = static_cast<int>(scene.feel.swingPct) +
                          delta * multiplier;
        scene.feel.swingPct = static_cast<uint8_t>(
            std::clamp(value, 50, 75));
        mini_acid_.applyFeelTimingFromScene_();
        break;
      }
      case FocusRow::TimingHumanize: {
        const float step = 0.01f * static_cast<float>(multiplier);
        scene.generatorParams.microTimingAmount = std::clamp(
            scene.generatorParams.microTimingAmount + delta * step,
            0.0f, 1.0f);
        break;
      }
      case FocusRow::VelocityHumanize: {
        const float step = 0.01f * static_cast<float>(multiplier);
        scene.generatorParams.velocityRange = std::clamp(
            scene.generatorParams.velocityRange + delta * step,
            0.0f, 1.0f);
        break;
      }
      case FocusRow::Preset:
        break;
    }
  });
}

void FeelPage::applyPreset(int index) {
  index = wrapIndex(index, 3);
  preset_index_ = index;
  Scene& scene = mini_acid_.sceneManager().currentScene();

  withAudioGuard([&]() {
    switch (index) {
      case 0:
        scene.feel.swingPct = 50;
        scene.generatorParams.microTimingAmount = 0.02f;
        scene.generatorParams.velocityRange = 0.10f;
        break;
      case 1:
        scene.feel.swingPct = 58;
        scene.generatorParams.microTimingAmount = 0.12f;
        scene.generatorParams.velocityRange = 0.30f;
        break;
      case 2:
        scene.feel.swingPct = 64;
        scene.generatorParams.microTimingAmount = 0.22f;
        scene.generatorParams.velocityRange = 0.45f;
        break;
    }
    mini_acid_.applyFeelTimingFromScene_();
  });

  char toast[72];
  std::snprintf(toast, sizeof(toast), "FEEL %s SW %u TIME %d VEL %d",
                kPresetNames[index],
                static_cast<unsigned>(scene.feel.swingPct),
                percent(scene.generatorParams.microTimingAmount),
                percent(scene.generatorParams.velocityRange));
  UI::showToast(toast, 1600);
}

void FeelPage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.feel;
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const GeneratorParams& params = scene.generatorParams;
  const GrooveRecipe recipe = mini_acid_.genreManager().getGrooveRecipe();

  UI::drawStandardHeader(gfx, mini_acid_, "FEEL");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "FEEL 2/4", "TIMING / VELOCITY",
                      axisColor, palette);

  char value[48];
  std::snprintf(value, sizeof(value), "%u%%",
                static_cast<unsigned>(scene.feel.swingPct));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(1), width,
                       "SWING OFFBEAT", value,
                       focus_ == FocusRow::Swing,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(1) + 2,
                    84, static_cast<int>(scene.feel.swingPct) - 50,
                    25, axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%",
                percent(params.microTimingAmount));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "TIMING DRIFT", value,
                       focus_ == FocusRow::TimingHumanize,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(2) + 2,
                    84, percent(params.microTimingAmount),
                    100, axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%",
                percent(params.velocityRange));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "VELOCITY VAR", value,
                       focus_ == FocusRow::VelocityHumanize,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(3) + 2,
                    84, percent(params.velocityRange),
                    100, axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
                       "PRESET", kPresetNames[preset_index_],
                       focus_ == FocusRow::Preset,
                       axisColor, palette);

  std::snprintf(value, sizeof(value), "GRID %u/BAR  MASK %04X",
                static_cast<unsigned>(recipe.stepsPerBar),
                static_cast<unsigned>(scene.feel.swingMask));
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(5) + 1, value);

  const char* explanation = "LIVE: offbeat playback delay";
  switch (focus_) {
    case FocusRow::Swing:
      explanation = "LIVE: offbeat playback delay";
      break;
    case FocusRow::TimingHumanize:
      explanation = "NEXT GEN: note timing spread";
      break;
    case FocusRow::VelocityHumanize:
      explanation = "NEXT GEN: note velocity spread";
      break;
    case FocusRow::Preset:
      explanation = "ENTER: load all FEEL values";
      break;
  }
  gfx.setTextColor(palette.text);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1, explanation);
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1,
               "No note, pitch or texture changes");

  UI::drawStandardFooter(gfx,
                         "TAB/U/D:FIELD  L/R:CHANGE",
                         "HOLD L/R:ACCEL  ENTER:PRESET");
}

bool FeelPage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (UIInput::isTab(event)) {
    moveFocus(1);
    return true;
  }

  const int nav = UIInput::navCode(event);
  if (nav == GROOVEPUTER_UP) {
    moveFocus(-1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    moveFocus(1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    const int delta = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    adjustFocused(delta, event.shift || event.ctrl || event.alt);
    return true;
  }

  if ((event.key == '\n' || event.key == '\r' || event.key == ' ') &&
      focus_ == FocusRow::Preset) {
    applyPreset(preset_index_);
    return true;
  }

  return false;
}
'''

(ROOT / 'src/ui/pages/feel_page.h').write_text(feel_h, encoding='utf-8')
(ROOT / 'src/ui/pages/feel_page.cpp').write_text(feel_cpp, encoding='utf-8')


texture_h = r'''#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "../ui_core.h"
#include "../ui_input.h"
#include "src/dsp/miniacid_engine.h"
#include "src/state/scene_revision.h"

class TexturePage : public IPage {
 public:
  TexturePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override { return title_; }
  void setVisualStyle(VisualStyle style) override { style_ = style; }
  void onEnter(int context) override {
    (void)context;
    syncFromEngine();
  }

 private:
  enum class FocusRow : uint8_t {
    Mode = 0,
    Amount,
    FlavorLink,
    Apply,
  };

  void syncFromEngine();
  void moveFocus(int delta);
  void shiftTexture(int delta);
  void adjustAmount(int delta, bool fast);
  void toggleFlavorLink();
  void applyTexture(bool announce);
  std::array<uint8_t, 7> macroView() const;

  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
    GroovePuterState::markSceneMutated();
  }

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  VisualStyle style_ = VisualStyle::MINIMAL;
  FocusRow focus_ = FocusRow::Mode;
  int texture_index_ = 0;
  int texture_amount_ = 70;
  UIInput::HoldAccelerator hold_accel_;
  std::string title_ = "TEXTURE";
};
'''

texture_cpp = r'''#include "texture_page.h"

#include <algorithm>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"

namespace {
int wrapIndex(int value, int count) {
  if (count <= 0) return 0;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return value;
}

int clampPct(int value) {
  return std::clamp(value, 0, 100);
}

uint8_t toMacro127(int percent, int amount) {
  percent = clampPct(percent);
  amount = clampPct(amount);
  return static_cast<uint8_t>((percent * amount * 127 + 5000) / 10000);
}

void drawMacroStrip(IGfx& gfx,
                    int x,
                    int y,
                    const std::array<uint8_t, 7>& values,
                    IGfxColor color,
                    const AxisUI::Palette& palette) {
  static constexpr const char* kLabels[7] = {
      "DI", "AG", "SP", "WD", "IN", "GR", "DK",
  };
  constexpr int kCellW = 31;
  for (int i = 0; i < 7; ++i) {
    const int cellX = x + i * (kCellW + 2);
    gfx.setTextColor(palette.muted);
    gfx.drawText(cellX, y, kLabels[i]);
    gfx.drawRect(cellX, y + 8, kCellW, 7, palette.border);
    const int fill = (static_cast<int>(values[i]) * (kCellW - 2)) / 127;
    if (fill > 0) gfx.fillRect(cellX + 1, y + 9, fill, 5, color);
  }
}
}  // namespace

TexturePage::TexturePage(IGfx& gfx,
                         MiniAcid& mini_acid,
                         AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
  syncFromEngine();
}

void TexturePage::syncFromEngine() {
  texture_index_ = static_cast<int>(mini_acid_.genreManager().textureMode());
  texture_amount_ = static_cast<int>(
      mini_acid_.sceneManager().currentScene().genre.textureAmount);
}

void TexturePage::moveFocus(int delta) {
  int value = static_cast<int>(focus_) + delta;
  value = wrapIndex(value, 4);
  focus_ = static_cast<FocusRow>(value);
  hold_accel_.reset();
}

void TexturePage::shiftTexture(int delta) {
  texture_index_ = wrapIndex(texture_index_ + delta, kTextureModeCount);
  applyTexture(false);
}

void TexturePage::adjustAmount(int delta, bool fast) {
  const int multiplier = hold_accel_.multiplier(delta, fast);
  texture_amount_ = std::clamp(
      texture_amount_ + delta * 2 * multiplier, 0, 100);
  applyTexture(false);
}

void TexturePage::toggleFlavorLink() {
  auto& enabled =
      mini_acid_.sceneManager().currentScene().genre.applySoundMacros;
  enabled = !enabled;
  GroovePuterState::markSceneMutated();
  UI::showToast(enabled ? "Flavor -> Texture: LINKED"
                        : "Flavor -> Texture: INDEPENDENT",
                1300);
}

void TexturePage::applyTexture(bool announce) {
  withAudioGuard([&]() {
    auto& manager = mini_acid_.genreManager();
    manager.setTextureMode(static_cast<TextureMode>(texture_index_));

    auto& settings = mini_acid_.sceneManager().currentScene().genre;
    settings.textureMode = static_cast<uint8_t>(texture_index_);
    settings.textureAmount = static_cast<uint8_t>(texture_amount_);

    manager.applyTexture(mini_acid_);
  });

  if (!announce) return;

  const TextureParams& params =
      kTexturePresets[wrapIndex(texture_index_, kTextureModeCount)];
  const bool tapeOn = texture_index_ != static_cast<int>(TextureMode::Clean) &&
                      texture_amount_ > 0;
  const bool delayOn = params.delayEnabled && texture_amount_ > 0;
  char toast[96];
  std::snprintf(toast, sizeof(toast), "TEXTURE %s %d%% TAPE:%s DLY:%s",
                GenreManager::textureModeName(
                    static_cast<TextureMode>(texture_index_)),
                texture_amount_, tapeOn ? "ON" : "OFF",
                delayOn ? "ON" : "OFF");
  UI::showToast(toast, 1700);
}

std::array<uint8_t, 7> TexturePage::macroView() const {
  const TextureParams& params =
      kTexturePresets[wrapIndex(texture_index_, kTextureModeCount)];
  const TapeMacro& tape = params.tapeMacro;

  const int dirt = std::max(
      static_cast<int>(tape.sat),
      static_cast<int>(tape.crush) * 25);
  const int age = tape.age;
  const int space = std::max(
      static_cast<int>(params.delayMix * 100.0f + 0.5f),
      static_cast<int>(params.delayFeedback * 75.0f + 0.5f));

  int width = params.delayEnabled ? 58 : 25;
  if (texture_index_ == static_cast<int>(TextureMode::Psychedelic)) width = 90;
  if (texture_index_ == static_cast<int>(TextureMode::Dub)) width = 68;

  const int instability = tape.wow;
  const int aggression = std::max(
      static_cast<int>(tape.sat),
      static_cast<int>(std::max(0.0f, params.filterResonanceBias) * 500.0f));
  const int darkness = std::max(
      100 - static_cast<int>(tape.tone),
      static_cast<int>(std::max(0.0f, -params.trebleBoostDB) * 16.0f));

  return {
      toMacro127(dirt, texture_amount_),
      toMacro127(age, texture_amount_),
      toMacro127(space, texture_amount_),
      toMacro127(width, texture_amount_),
      toMacro127(instability, texture_amount_),
      toMacro127(aggression, texture_amount_),
      toMacro127(darkness, texture_amount_),
  };
}

void TexturePage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.texture;
  const auto selected = static_cast<TextureMode>(texture_index_);
  const auto active = mini_acid_.genreManager().textureMode();
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const bool link = scene.genre.applySoundMacros;
  const TextureParams& params =
      kTexturePresets[wrapIndex(texture_index_, kTextureModeCount)];
  const bool tapeOn = texture_index_ != static_cast<int>(TextureMode::Clean) &&
                      texture_amount_ > 0;
  const bool delayOn = params.delayEnabled && texture_amount_ > 0;

  UI::drawStandardHeader(gfx, mini_acid_, "TEXTURE");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "TEXTURE 4/4", "LIVE SOUND SURFACE",
                      axisColor, palette);

  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(1), width, "MODE",
      GenreManager::textureModeName(selected),
      focus_ == FocusRow::Mode, axisColor, palette);

  char value[80];
  std::snprintf(value, sizeof(value), "%d%%", texture_amount_);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "AMOUNT", value,
                       focus_ == FocusRow::Amount,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(2) + 2,
                    84, texture_amount_, 100, axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "FLAVOR LINK", link ? "LINKED" : "INDEPENDENT",
                       focus_ == FocusRow::FlavorLink,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
                       "APPLY", "LIVE / ENTER REAPPLY",
                       focus_ == FocusRow::Apply,
                       axisColor, palette);

  std::snprintf(value, sizeof(value), "AUDIBLE TAPE %s  DELAY %s",
                tapeOn ? "ON" : "OFF", delayOn ? "ON" : "OFF");
  gfx.setTextColor(palette.text);
  gfx.drawText(x + 2, LayoutManager::lineY(5), value);

  const std::array<uint8_t, 7> macros = macroView();
  drawMacroStrip(gfx, x + 2, LayoutManager::lineY(6),
                 macros, axisColor, palette);

  std::snprintf(value, sizeof(value), "ACTIVE %s %u%%",
                GenreManager::textureModeName(active),
                static_cast<unsigned>(scene.genre.textureAmount));
  gfx.setTextColor(active == selected ? axisColor : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  UI::drawStandardFooter(gfx,
                         "TAB/U/D:FIELD  L/R:LIVE",
                         "HOLD L/R:ACCEL  ENTER:REAPPLY");
}

bool TexturePage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (UIInput::isTab(event)) {
    moveFocus(1);
    return true;
  }

  const int nav = UIInput::navCode(event);
  if (nav == GROOVEPUTER_UP) {
    moveFocus(-1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    moveFocus(1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    const int delta = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    switch (focus_) {
      case FocusRow::Mode:
        shiftTexture(delta);
        break;
      case FocusRow::Amount:
        adjustAmount(delta, event.shift || event.ctrl || event.alt);
        break;
      case FocusRow::FlavorLink:
        toggleFlavorLink();
        break;
      case FocusRow::Apply:
        applyTexture(true);
        break;
    }
    return true;
  }

  if (event.key == '\n' || event.key == '\r' || event.key == ' ') {
    if (focus_ == FocusRow::FlavorLink) toggleFlavorLink();
    else applyTexture(true);
    return true;
  }

  return false;
}
'''

(ROOT / 'src/ui/pages/texture_page.h').write_text(texture_h, encoding='utf-8')
(ROOT / 'src/ui/pages/texture_page.cpp').write_text(texture_cpp, encoding='utf-8')


# Texture now owns enabling its audible processing path.
replace_once(
    ROOT / 'src/dsp/genre_manager.cpp',
    '    tape.macro = macro;\n    // FEEL page owns Tape ON/OFF. Genre texture adjusts macro only.\n',
    '    tape.macro = macro;\n'
    '    const bool tapeOn = textureMode_ != TextureMode::Clean && amount > 0.01f;\n'
    '    tape.fxEnabled = tapeOn;\n'
    '    engine.sceneManager().currentScene().feel.tapeEnabled = tapeOn;\n',
)


# Song materialization must use the active genre+recipe, including Atlas variants,
# and must not inherit unrelated legacy flavor state.
song = ROOT / 'src/ui/pages/song_page.cpp'
replace_once(
    song,
    '#include "../ui_theme.h"\n',
    '#include "../ui_theme.h"\n#include "../../dsp/atlas_runtime.h"\n',
)
replace_once(
    song,
    '    request.modeTag = static_cast<uint8_t>(gen_mode_);\n',
    '    const uint8_t genreTag = static_cast<uint8_t>(\n'
    '        mini_acid_.genreManager().generativeMode());\n'
    '    const uint8_t recipeTag = static_cast<uint8_t>(\n'
    '        mini_acid_.genreManager().recipe());\n'
    '    request.modeTag = static_cast<uint8_t>(\n'
    '        genreTag * 17u + recipeTag * 5u +\n'
    '        static_cast<uint8_t>(gen_mode_));\n',
)
regex_once(
    song,
    r'    Scene& scene = mini_acid_\.sceneManager\(\)\.currentScene\(\);\n'
    r'    const GenerativeParams& params =.*?\n'
    r'    auto commit =',
    '''    Scene& scene = mini_acid_.sceneManager().currentScene();
    auto& genreManager = mini_acid_.genreManager();
    const GenerativeMode activeGenre = genreManager.generativeMode();
    const GenreRecipeId activeRecipe = genreManager.recipe();
    const GenerativeParams& params =
        genreManager.getCompiledGenerativeParams();
    const GenreBehavior behavior = genreManager.getBehavior();
    const GrooveboxMode mappedMode = GenreManager::grooveboxModeForRecipe(
        activeRecipe, activeGenre);

    SynthPattern atlasA{};
    SynthPattern atlasB{};
    DrumPatternSet atlasDrums{};
    const uint8_t variationCount = AtlasRuntime::variationCount(activeRecipe);
    const uint8_t variation = variationCount == 0
        ? 0
        : static_cast<uint8_t>(std::min(
              static_cast<int>(variationCount) - 1,
              static_cast<int>(gen_mode_)));
    const bool atlasReady = AtlasRuntime::hasRecipe(activeRecipe) &&
        AtlasRuntime::applyRecipe(activeRecipe, variation,
                                  atlasA, atlasB, atlasDrums, nullptr);

    auto generateTrack = [&](SongTrack track,
                             uint32_t seed,
                             SynthPattern& synth,
                             DrumPatternSet& drums) {
        if (atlasReady) {
            switch (track) {
                case SongTrack::SynthA:
                    synth = atlasA;
                    return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(synth);
                case SongTrack::SynthB:
                    synth = atlasB;
                    return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(synth);
                case SongTrack::Drums:
                    drums = atlasDrums;
                    return !SongPatternMaterializer::drumPatternSetIsStrictlyEmpty(drums);
                case SongTrack::Voice:
                    return false;
            }
        }

        GrooveboxModeManager generator(mini_acid_);
        generator.setModeLocal(mappedMode);
        generator.setFlavorLocal(0);
        generator.setGenerationSeed(seed);

        switch (track) {
            case SongTrack::SynthA:
                generator.generatePattern(
                    synth, mini_acid_.bpm(), params, behavior, 0);
                return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
                    synth);
            case SongTrack::SynthB:
                generator.generatePattern(
                    synth, mini_acid_.bpm(), params, behavior, 1);
                return !SongPatternMaterializer::synthPatternIsStrictlyEmpty(
                    synth);
            case SongTrack::Drums:
                generator.generateDrumPattern(drums, params, behavior);
                return !SongPatternMaterializer::drumPatternSetIsStrictlyEmpty(
                    drums);
            case SongTrack::Voice:
                return false;
        }
        return false;
    };

    auto commit =''',
)
replace_once(
    song,
    '    char message[48];\n    std::snprintf(\n        message, sizeof(message), "GEN %s -> %s", trackLabel, patternLabel);\n',
    '    char message[96];\n'
    '    std::snprintf(\n'
    '        message, sizeof(message), "GEN %s %s/%s -> %s",\n'
    '        trackLabel,\n'
    '        GenreManager::generativeModeName(\n'
    '            mini_acid_.genreManager().generativeMode()),\n'
    '        GenreManager::recipeName(mini_acid_.genreManager().recipe()),\n'
    '        patternLabel);\n',
)
replace_once(
    song,
    '    char message[32];\n    std::snprintf(message, sizeof(message), "GENERATED ROW %d", row + 1);\n',
    '    char message[96];\n'
    '    std::snprintf(message, sizeof(message), "ROW %d %s/%s",\n'
    '                  row + 1,\n'
    '                  GenreManager::generativeModeName(\n'
    '                      mini_acid_.genreManager().generativeMode()),\n'
    '                  GenreManager::recipeName(\n'
    '                      mini_acid_.genreManager().recipe()));\n',
)


generation_h = r'''#pragma once

#include <cstdint>
#include <string>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"

class GenerationPage : public IPage {
 public:
  GenerationPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override { return title_; }
  void setVisualStyle(VisualStyle style) override { style_ = style; }

 private:
  static constexpr uint8_t kMaterializeBars = 1;

  void materializeCurrentBar();

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  VisualStyle style_ = VisualStyle::MINIMAL;
  bool last_attempted_ = false;
  bool last_success_ = false;
  int last_row_ = -1;
  int last_pattern_ = -1;
  std::string last_status_ = "READY";
  std::string title_ = "GENERATION";
};
'''

generation_cpp = r'''#include "generation_page.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../../dsp/atlas_runtime.h"
#include "../../dsp/phrase_generator.h"
#include "src/state/scene_revision.h"

namespace {
uint8_t atlasVariationForRole(PhraseGenerator::PhraseBarRole role) {
  switch (role) {
    case PhraseGenerator::PhraseBarRole::Base:
    case PhraseGenerator::PhraseBarRole::Return:
      return 0;
    case PhraseGenerator::PhraseBarRole::MicroVariation:
    case PhraseGenerator::PhraseBarRole::Development:
    case PhraseGenerator::PhraseBarRole::Build:
      return 1;
    case PhraseGenerator::PhraseBarRole::Breakdown:
    case PhraseGenerator::PhraseBarRole::Fill:
    case PhraseGenerator::PhraseBarRole::EndingFill:
      return 2;
  }
  return 0;
}

void formatPatternLabel(int globalPattern, char* out, int outSize) {
  if (!out || outSize <= 0 || globalPattern < 0) return;
  std::snprintf(out, outSize, "%d%c%d",
                songPatternPage(globalPattern) + 1,
                static_cast<char>('A' + songPatternBank(globalPattern)),
                songPatternIndexInBank(globalPattern) + 1);
}
}  // namespace

GenerationPage::GenerationPage(IGfx& gfx,
                               MiniAcid& mini_acid,
                               AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
}

void GenerationPage::materializeCurrentBar() {
  const bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying) mini_acid_.stop();

  PhraseGenerator::PhraseResult result{};
  const int targetRow = std::max(0, mini_acid_.currentSongPosition());
  const auto generate = [&]() {
    PhraseGenerator::PhraseRequest request{};
    request.bars = kMaterializeBars;
    request.songStart = targetRow;
    request.pageIndex = mini_acid_.currentPageIndex();
    request.seed = static_cast<uint32_t>(rand());

    Scene& scene = mini_acid_.sceneManager().currentScene();
    auto& genreManager = mini_acid_.genreManager();
    const GenerativeMode activeGenre = genreManager.generativeMode();
    const GenreRecipeId recipe = genreManager.recipe();
    const bool atlasPhrase = AtlasRuntime::hasRecipe(recipe) &&
                             AtlasRuntime::variationCount(recipe) >= 3;

    if (atlasPhrase) {
      result = PhraseGenerator::generateBarsToSong(
          scene, request,
          [&](PhraseGenerator::PhraseBar& bar,
              PhraseGenerator::PhraseBarRole role,
              int barIndex) {
            (void)barIndex;
            return AtlasRuntime::applyRecipe(
                recipe, atlasVariationForRole(role),
                bar.synthA, bar.synthB, bar.drums, nullptr);
          });
    } else {
      result = PhraseGenerator::generateToSong(
          scene, request, [&](PhraseGenerator::PhraseBar& base) {
            const GenerativeParams& params =
                genreManager.getCompiledGenerativeParams();
            const GenreBehavior behavior = genreManager.getBehavior();
            GrooveboxModeManager generator(mini_acid_);
            generator.setModeLocal(GenreManager::grooveboxModeForRecipe(
                recipe, activeGenre));
            generator.setFlavorLocal(0);
            generator.setGenerationSeed(request.seed);
            generator.generatePattern(
                base.synthA, mini_acid_.bpm(), params, behavior, 0);
            generator.generatePattern(
                base.synthB, mini_acid_.bpm(), params, behavior, 1);
            generator.generateDrumPattern(base.drums, params, behavior);
          });
    }

    if (result) {
      mini_acid_.setSongMode(true);
      mini_acid_.setSongPlaybackSlot(scene.activeSongSlot);
      mini_acid_.setSongPosition(result.songStart);
    }
  };

  if (audio_guard_) audio_guard_(generate);
  else generate();

  last_attempted_ = true;
  last_success_ = static_cast<bool>(result);
  last_row_ = targetRow;
  last_pattern_ = result.firstGlobalPattern;

  if (result) {
    GroovePuterState::markSceneMutated();
    char patternLabel[16] = "---";
    formatPatternLabel(result.firstGlobalPattern,
                       patternLabel, sizeof(patternLabel));
    char status[64];
    std::snprintf(status, sizeof(status), "OK ROW %d -> %s",
                  result.songStart + 1, patternLabel);
    last_status_ = status;

    char toast[112];
    std::snprintf(toast, sizeof(toast), "GEN OK %s/%s ROW %d -> %s",
                  GenreManager::generativeModeName(
                      mini_acid_.genreManager().generativeMode()),
                  GenreManager::recipeName(
                      mini_acid_.genreManager().recipe()),
                  result.songStart + 1, patternLabel);
    UI::showToast(toast, 2000);
  } else {
    last_status_ = PhraseGenerator::errorText(result.error);
    char toast[96];
    std::snprintf(toast, sizeof(toast), "GEN BLOCKED ROW %d: %s",
                  targetRow + 1, last_status_.c_str());
    UI::showToast(toast, 2200);
  }

  if (wasPlaying) mini_acid_.start();
}

void GenerationPage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.generation;
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const GenerativeParams& params =
      mini_acid_.genreManager().getCompiledGenerativeParams();

  UI::drawStandardHeader(gfx, mini_acid_, "GENERATION");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "GEN 3/4", "WRITE ONE SONG BAR",
                      axisColor, palette);

  char value[112];
  std::snprintf(value, sizeof(value), "%s / %s",
                GenreManager::generativeModeName(
                    mini_acid_.genreManager().generativeMode()),
                GenreManager::recipeName(mini_acid_.genreManager().recipe()));
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(1) + 1, value);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "SCOPE", "CURRENT EMPTY SONG ROW", false,
                       axisColor, palette);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "PLAN", "SINGLE BAR / BASE", false,
                       axisColor, palette);

  std::snprintf(value, sizeof(value), "SONG %c ROW %d",
                static_cast<char>('A' + std::clamp(scene.activeSongSlot, 0, 1)),
                std::max(0, mini_acid_.currentSongPosition()) + 1);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
                       "TARGET", value, false,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(5), width,
                       "WRITE", "ENTER / G", true,
                       axisColor, palette);

  std::snprintf(value, sizeof(value),
                "A %.0f%%  S %.0f%%  FILL %.0f%%",
                params.accentProbability * 100.0f,
                params.slideProbability * 100.0f,
                params.fillProbability * 100.0f);
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1, value);

  std::snprintf(value, sizeof(value), "LAST %s",
                last_attempted_ ? last_status_.c_str() : "READY");
  gfx.setTextColor(!last_attempted_ || last_success_
                       ? axisColor
                       : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  UI::drawStandardFooter(gfx,
                         "ENTER/G:WRITE 1 BAR",
                         "ROW OCCUPIED:BLOCK  LEN:PHRASE");
}

bool GenerationPage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (event.key == '\n' || event.key == '\r') {
    materializeCurrentBar();
    return true;
  }

  if ((event.key == 'g' || event.key == 'G') &&
      !event.ctrl && !event.alt && !event.meta) {
    materializeCurrentBar();
    return true;
  }

  return false;
}
'''

(ROOT / 'src/ui/pages/generation_page.h').write_text(generation_h, encoding='utf-8')
(ROOT / 'src/ui/pages/generation_page.cpp').write_text(generation_cpp, encoding='utf-8')


test = r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

song = read("src/ui/pages/song_page.cpp")
feel = read("src/ui/pages/feel_page.cpp")
texture = read("src/ui/pages/texture_page.cpp")
generation = read("src/ui/pages/generation_page.cpp")
genre_manager = read("src/dsp/genre_manager.cpp")
ui_input = read("src/ui/ui_input.h")

for token in (
    "AtlasRuntime::hasRecipe(activeRecipe)",
    "AtlasRuntime::applyRecipe(activeRecipe",
    "GenreManager::grooveboxModeForRecipe",
    "generator.setFlavorLocal(0)",
    "genreTag * 17u + recipeTag * 5u",
):
    assert token in song, f"Song genre materialization contract missing: {token}"

for token in (
    "tape.fxEnabled = tapeOn;",
    "currentScene().feel.tapeEnabled = tapeOn;",
):
    assert token in genre_manager, f"Texture audible path missing: {token}"

for token in (
    "applyTexture(false);",
    "LIVE / ENTER REAPPLY",
    "AUDIBLE TAPE",
    "HOLD L/R:ACCEL",
):
    assert token in texture, f"Texture feedback contract missing: {token}"

for token in (
    "LIVE: offbeat playback delay",
    "NEXT GEN: note timing spread",
    "NEXT GEN: note velocity spread",
    "HOLD L/R:ACCEL",
):
    assert token in feel, f"FEEL causality contract missing: {token}"

for token in (
    "class HoldAccelerator",
    "streak_ >= 10",
    "streak_ >= 4",
):
    assert token in ui_input, f"Hold acceleration missing: {token}"

for token in (
    "GEN BLOCKED ROW",
    "LAST %s",
    "CURRENT EMPTY SONG ROW",
    "generator.setFlavorLocal(0)",
):
    assert token in generation, f"Generation feedback contract missing: {token}"

print("Axis hardware feedback source regressions: PASS")
'''
(ROOT / 'tests/test_axis_hardware_feedback_source_regressions.py').write_text(
    test, encoding='utf-8')

run_tests = ROOT / 'tests/run_host_tests.sh'
run_text = run_tests.read_text(encoding='utf-8')
line = 'python3 "${ROOT_DIR}/tests/test_axis_hardware_feedback_source_regressions.py"\n'
if line not in run_text:
    anchor = 'python3 "${ROOT_DIR}/tests/test_song_generation_source_regressions.py"\n'
    if anchor not in run_text:
        raise RuntimeError('run_host_tests insertion anchor missing')
    run_tests.write_text(run_text.replace(anchor, anchor + line, 1), encoding='utf-8')

runbook = ROOT / 'docs/stages/FOUR_AXIS_GENERATE_UI.md'
runbook_text = runbook.read_text(encoding='utf-8')
section = '''

## Hardware feedback pass: Song / FEEL / GENERATION / TEXTURE

- Song `G` uses the active GENRE and VARIANT directly. Atlas-backed variants are materialized without visiting GENRE first; the toast names the active genre/variant.
- FEEL `SWING OFFBEAT` changes playback immediately. `TIMING DRIFT` and `VELOCITY VAR` affect the next generated material. Hold Left/Right to accelerate.
- GENERATION writes one bar only to the current empty Song row. A persistent `LAST` line shows success or the blocking reason.
- TEXTURE applies MODE and AMOUNT live. Non-CLEAN textures enable the tape FX path; the screen shows whether TAPE and DELAY are audible. Hold Left/Right to accelerate.

Acceptance:

- [ ] Song-generated A/B/DR material changes when GENRE/VARIANT changes.
- [ ] FEEL screen distinguishes LIVE from NEXT GEN controls.
- [ ] Held Left/Right accelerates FEEL and TEXTURE values.
- [ ] GENERATION shows `LAST OK` or a persistent blocked reason.
- [ ] DUB/LOFI/INDUSTRIAL/PSYCHEDELIC produce an audible change and show TAPE/DELAY state.
'''
if '## Hardware feedback pass: Song / FEEL / GENERATION / TEXTURE' not in runbook_text:
    runbook.write_text(runbook_text.rstrip() + section + '\n', encoding='utf-8')
