#include "settings_page.h"

#include <algorithm>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"

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

SettingsPage::SettingsPage(IGfx& gfx,
                           MiniAcid& mini_acid,
                           AudioGuard& audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
}

void SettingsPage::moveFocus(int delta) {
  int value = static_cast<int>(focus_) + delta;
  value = wrapIndex(value, 4);
  focus_ = static_cast<FocusRow>(value);
}

void SettingsPage::adjustFocused(int delta, bool fast) {
  if (focus_ == FocusRow::Preset) {
    preset_index_ = wrapIndex(preset_index_ + delta, 3);
    return;
  }

  Scene& scene = mini_acid_.sceneManager().currentScene();
  const int scalar = fast ? 5 : 1;

  withAudioGuard([&]() {
    switch (focus_) {
      case FocusRow::Swing: {
        int value = static_cast<int>(scene.feel.swingPct) + delta * scalar;
        scene.feel.swingPct = static_cast<uint8_t>(std::clamp(value, 50, 75));
        mini_acid_.applyFeelTimingFromScene_();
        break;
      }
      case FocusRow::TimingHumanize: {
        const float step = fast ? 0.05f : 0.01f;
        scene.generatorParams.microTimingAmount = std::clamp(
            scene.generatorParams.microTimingAmount + delta * step,
            0.0f, 1.0f);
        break;
      }
      case FocusRow::VelocityHumanize: {
        const float step = fast ? 0.05f : 0.01f;
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

void SettingsPage::applyPreset(int index) {
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

  char toast[64];
  std::snprintf(toast, sizeof(toast), "FEEL %s: SW %u MT %d VL %d",
                kPresetNames[index],
                static_cast<unsigned>(scene.feel.swingPct),
                percent(scene.generatorParams.microTimingAmount),
                percent(scene.generatorParams.velocityRange));
  UI::showToast(toast, 1500);
}

void SettingsPage::draw(IGfx& gfx) {
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
                       "SWING", value, focus_ == FocusRow::Swing,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(1) + 2,
                    84, static_cast<int>(scene.feel.swingPct) - 50,
                    25, axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%",
                percent(params.microTimingAmount));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "TIME HUMAN", value,
                       focus_ == FocusRow::TimingHumanize,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(2) + 2,
                    84, percent(params.microTimingAmount),
                    100, axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%",
                percent(params.velocityRange));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "VEL HUMAN", value,
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

  gfx.setTextColor(palette.text);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1,
               "Moves timing/velocity only");
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1,
               "No notes, roles or sound changes");

  UI::drawStandardFooter(gfx,
                         "TAB/U/D:FIELD  L/R:CHANGE",
                         "FAST:SHIFT/CTRL  ENTER:PRESET");
}

bool SettingsPage::handleEvent(UIEvent& event) {
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
