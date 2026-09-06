#include "feel_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../../state/generation_request_state.h"
#include "../../state/scene_revision.h"

namespace {
constexpr const char* kPresetNames[3] = {"TIGHT", "HUMAN", "LOOSE"};
constexpr uint8_t kRepeatBars[4] = {1, 2, 4, 8};

int wrapIndex(int value, int count) {
  if (count <= 0) return 0;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return value;
}

int percent(float value) {
  return static_cast<int>(value * 100.0f + 0.5f);
}

uint8_t normalizedRepeatBars(uint8_t bars) {
  for (uint8_t value : kRepeatBars) {
    if (bars == value) return value;
  }
  return 1;
}

uint8_t shiftRepeatBars(uint8_t current, int delta) {
  current = normalizedRepeatBars(current);
  int index = 0;
  for (int i = 0; i < 4; ++i) {
    if (kRepeatBars[i] == current) {
      index = i;
      break;
    }
  }
  return kRepeatBars[wrapIndex(index + delta, 4)];
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
  value = wrapIndex(value, 6);
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
  bool changed = false;

  withAudioGuard([&]() {
    switch (focus_) {
      case FocusRow::Profile: {
        const int count = static_cast<int>(
            GroovePuterRhythm::FeelProfileId::Count);
        const uint8_t next = static_cast<uint8_t>(wrapIndex(
            static_cast<int>(scene.feel.timingProfile) + delta, count));
        if (next != scene.feel.timingProfile) {
          scene.feel.timingProfile = next;
          mini_acid_.applyFeelTimingFromScene_();
          changed = true;
        }
        break;
      }
      case FocusRow::Swing: {
        const int value = static_cast<int>(scene.feel.swingPct) +
                          delta * multiplier;
        const uint8_t next = static_cast<uint8_t>(
            std::clamp(value, 50, 75));
        if (next != scene.feel.swingPct) {
          scene.feel.swingPct = next;
          mini_acid_.applyFeelTimingFromScene_();
          (void)mini_acid_.rebuildPatternRuntimeEventBank();
          changed = true;
        }
        break;
      }
      case FocusRow::TimingHumanize: {
        // GF2-I2A: the musically useful range is roughly 0.15..1.0, so a 0.01
        // step made the control take about eighty presses to cross.
        const float step = 0.05f * static_cast<float>(multiplier);
        const float next = std::clamp(
            scene.generatorParams.microTimingAmount + delta * step,
            0.0f, 1.0f);
        if (next != scene.generatorParams.microTimingAmount) {
          scene.generatorParams.microTimingAmount = next;
          changed = true;
        }
        break;
      }
      case FocusRow::VelocityHumanize: {
        const float step = 0.01f * static_cast<float>(multiplier);
        const float next = std::clamp(
            scene.generatorParams.velocityRange + delta * step,
            0.0f, 1.0f);
        if (next != scene.generatorParams.velocityRange) {
          scene.generatorParams.velocityRange = next;
          changed = true;
        }
        break;
      }
      case FocusRow::Repeats: {
        const uint8_t next = shiftRepeatBars(scene.feel.patternBars, delta);
        if (next != scene.feel.patternBars) {
          scene.feel.patternBars = next;
          changed = true;
        }
        break;
      }
      case FocusRow::Preset:
        break;
    }
  });

  if (changed) GroovePuterState::markSceneMutated();
}

void FeelPage::applyPreset(int index) {
  index = wrapIndex(index, 3);
  preset_index_ = index;
  Scene& scene = mini_acid_.sceneManager().currentScene();

  uint8_t nextSwing = scene.feel.swingPct;
  float nextTiming = scene.generatorParams.microTimingAmount;
  float nextVelocity = scene.generatorParams.velocityRange;
  uint8_t nextProfile = scene.feel.timingProfile;
  switch (index) {
    case 0:
      nextProfile = static_cast<uint8_t>(
          GroovePuterRhythm::FeelProfileId::Straight);
      nextSwing = 50;
      nextTiming = 0.15f;
      nextVelocity = 0.10f;
      break;
    case 1:
      nextProfile = static_cast<uint8_t>(
          GroovePuterRhythm::FeelProfileId::SwingCompatible);
      nextSwing = 58;
      nextTiming = 0.50f;
      nextVelocity = 0.30f;
      break;
    case 2:
      nextProfile = static_cast<uint8_t>(
          GroovePuterRhythm::FeelProfileId::LaidBack);
      nextSwing = 64;
      nextTiming = 0.80f;
      nextVelocity = 0.45f;
      break;
  }

  const bool changed = scene.feel.timingProfile != nextProfile ||
                       scene.feel.swingPct != nextSwing ||
                       scene.generatorParams.microTimingAmount != nextTiming ||
                       scene.generatorParams.velocityRange != nextVelocity;
  if (changed) {
    withAudioGuard([&]() {
      scene.feel.swingPct = nextSwing;
      scene.feel.timingProfile = nextProfile;
      scene.generatorParams.microTimingAmount = nextTiming;
      scene.generatorParams.velocityRange = nextVelocity;
      mini_acid_.applyFeelTimingFromScene_();
      (void)mini_acid_.rebuildPatternRuntimeEventBank();
    });
    GroovePuterState::markSceneMutated();
  }

  char toast[72];
  std::snprintf(toast, sizeof(toast), "FEEL %s %s SW %u TIME %d VEL %d",
                kPresetNames[index],
                GroovePuterRhythm::feelProfileName(
                    static_cast<GroovePuterRhythm::FeelProfileId>(
                        scene.feel.timingProfile)),
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

  UI::drawStandardHeader(gfx, mini_acid_, "FEEL");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "FEEL 2/2", "TIMING / VELOCITY",
                      axisColor, palette);

  char value[48];
  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(1), width, "PROFILE",
      GroovePuterRhythm::feelProfileName(
          static_cast<GroovePuterRhythm::FeelProfileId>(
              scene.feel.timingProfile)),
      focus_ == FocusRow::Profile, axisColor, palette);

  std::snprintf(value, sizeof(value), "%u%%",
                static_cast<unsigned>(scene.feel.swingPct));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "SWING OFFBEAT", value,
                       focus_ == FocusRow::Swing,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(2) + 2,
                    84, static_cast<int>(scene.feel.swingPct) - 50,
                    25, axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%",
                percent(params.microTimingAmount));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "FEEL AMOUNT", value,
                       focus_ == FocusRow::TimingHumanize,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(3) + 2,
                    84, percent(params.microTimingAmount),
                    100, axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%",
                percent(params.velocityRange));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
                       "VELOCITY VAR", value,
                       focus_ == FocusRow::VelocityHumanize,
                       axisColor, palette);
  AxisUI::drawMeter(gfx, x + 142, LayoutManager::lineY(4) + 2,
                    84, percent(params.velocityRange),
                    100, axisColor, palette);

  std::snprintf(value, sizeof(value), "%u BAR%s",
                static_cast<unsigned>(normalizedRepeatBars(scene.feel.patternBars)),
                normalizedRepeatBars(scene.feel.patternBars) == 1 ? "" : "S");
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(5), width,
                       "FEEL CYCLE", value,
                       focus_ == FocusRow::Repeats,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(6), width,
                       "PRESET", kPresetNames[preset_index_],
                       focus_ == FocusRow::Preset, axisColor, palette);

  const char* explanation = "LIVE: offbeat playback delay";
  switch (focus_) {
    case FocusRow::Profile:
      explanation = "NEXT GEN: bounded role timing";
      break;
    case FocusRow::Swing:
      explanation = "LIVE: offbeat playback delay";
      break;
    case FocusRow::TimingHumanize:
      explanation = "NEXT GEN: profile intensity";
      break;
    case FocusRow::VelocityHumanize:
      explanation = "NEXT GEN: note velocity spread";
      break;
    case FocusRow::Repeats:
      explanation = "LOCAL CYCLE: repeat 1/2/4/8 bars";
      break;
    case FocusRow::Preset:
      explanation = "ENTER: load all FEEL values";
      break;
  }
  gfx.setTextColor(palette.text);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, explanation);

  UI::drawStandardFooter(gfx,
                         "U/D:FIELD L/R:CHANGE",
                         "HOLD L/R:ACCEL P:LEVEL");
}

bool FeelPage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

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

  const char lowerKey = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  const bool keyP = lowerKey == 'p' || event.scancode == GROOVEPUTER_P;
  if (!event.ctrl && !event.alt && !event.meta && keyP) {
    const auto level = GroovePuterState::cycleGenerationLevel();
    UI::showToast(GroovePuterState::generationLevelShortName(level), 1200);
    return true;
  }

  // The GENERATE workflow must not fall through to the old global I/O
  // GrooveboxModeManager shortcuts. P is now owned above by the single
  // P1/P2/P3 generation-request selector.
  if (!event.ctrl && !event.alt && !event.meta &&
      (lowerKey == 'i' || lowerKey == 'o')) {
    UI::showToast("LEGACY SYNTH GEN OFF", 1200);
    return true;
  }

  return false;
}