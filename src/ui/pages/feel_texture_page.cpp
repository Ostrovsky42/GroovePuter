#include "feel_texture_page.h"

#include <algorithm>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"

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
      "DI", "AG", "SP", "WD", "IN", "AT", "DK",
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

FeelTexturePage::FeelTexturePage(IGfx& gfx,
                                 MiniAcid& mini_acid,
                                 AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
  syncFromEngine();
}

void FeelTexturePage::syncFromEngine() {
  texture_index_ = static_cast<int>(mini_acid_.genreManager().textureMode());
  texture_amount_ = static_cast<int>(
      mini_acid_.sceneManager().currentScene().genre.textureAmount);
}

void FeelTexturePage::moveFocus(int delta) {
  int value = static_cast<int>(focus_) + delta;
  value = wrapIndex(value, 4);
  focus_ = static_cast<FocusRow>(value);
}

void FeelTexturePage::shiftTexture(int delta) {
  texture_index_ = wrapIndex(texture_index_ + delta, kTextureModeCount);
}

void FeelTexturePage::adjustAmount(int delta, bool fast) {
  texture_amount_ = std::clamp(
      texture_amount_ + delta * (fast ? 10 : 2), 0, 100);
}

void FeelTexturePage::toggleFlavorLink() {
  auto& enabled =
      mini_acid_.sceneManager().currentScene().genre.applySoundMacros;
  enabled = !enabled;
  GroovePuterState::markSceneMutated();
  UI::showToast(enabled ? "Flavor -> Sound: ON" : "Flavor -> Sound: OFF",
                1100);
}

void FeelTexturePage::applyTexture() {
  withAudioGuard([&]() {
    auto& manager = mini_acid_.genreManager();
    manager.setTextureMode(static_cast<TextureMode>(texture_index_));

    auto& settings = mini_acid_.sceneManager().currentScene().genre;
    settings.textureMode = static_cast<uint8_t>(texture_index_);
    settings.textureAmount = static_cast<uint8_t>(texture_amount_);

    manager.applyTexture(mini_acid_);
  });

  char toast[72];
  std::snprintf(toast, sizeof(toast), "TEXTURE %s %d%%",
                GenreManager::textureModeName(
                    static_cast<TextureMode>(texture_index_)),
                texture_amount_);
  UI::showToast(toast, 1400);
}

std::array<uint8_t, 7> FeelTexturePage::macroView() const {
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

void FeelTexturePage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.texture;
  const auto selected = static_cast<TextureMode>(texture_index_);
  const auto active = mini_acid_.genreManager().textureMode();
  const bool link =
      mini_acid_.sceneManager().currentScene().genre.applySoundMacros;

  UI::drawStandardHeader(gfx, mini_acid_, "TEXTURE");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "TEXTURE 4/4", "SOUND SURFACE",
                      axisColor, palette);

  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(1), width, "MODE",
      GenreManager::textureModeName(selected),
      focus_ == FocusRow::Mode, axisColor, palette);

  char value[64];
  std::snprintf(value, sizeof(value), "%d%%", texture_amount_);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "AMOUNT", value,
                       focus_ == FocusRow::Amount,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(2) + 2,
                    84, texture_amount_, 100, axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "FLAVOR LINK", link ? "ON (CROSS-AXIS)" : "OFF",
                       focus_ == FocusRow::FlavorLink,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
                       "APPLY", "ENTER / SPACE",
                       focus_ == FocusRow::Apply,
                       axisColor, palette);

  const std::array<uint8_t, 7> macros = macroView();
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(5),
               "MACRO VIEW 0..127 (READ ONLY)");
  drawMacroStrip(gfx, x + 2, LayoutManager::lineY(6),
                 macros, axisColor, palette);

  std::snprintf(value, sizeof(value), "ACTIVE %s",
                GenreManager::textureModeName(active));
  gfx.setTextColor(active == selected ? axisColor : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  UI::drawStandardFooter(gfx,
                         "TAB/U/D:FIELD  L/R:CHANGE",
                         "ENTER/SPACE:APPLY TEXTURE");
}

bool FeelTexturePage::handleEvent(UIEvent& event) {
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
        break;
    }
    return true;
  }

  if (event.key == '\n' || event.key == '\r' || event.key == ' ') {
    if (focus_ == FocusRow::Mode) shiftTexture(1);
    else if (focus_ == FocusRow::FlavorLink) toggleFlavorLink();
    else if (focus_ == FocusRow::Apply) applyTexture();
    return true;
  }

  return false;
}
