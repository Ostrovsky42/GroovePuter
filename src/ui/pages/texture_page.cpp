#include "texture_page.h"

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
