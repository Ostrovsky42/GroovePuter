#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(relative: str, old: str, new: str) -> None:
    path = ROOT / relative
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{relative}: expected one replacement, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/ui/pages/tape_page.cpp",
    '#include "../ui_common.h"\n#include "src/state/scene_revision.h"',
    '#include "../ui_common.h"\n#include "../ui_input.h"\n#include "src/state/scene_revision.h"',
)

replace_once(
    "src/ui/pages/tape_page.cpp",
    '''  bool shift = ui_event.shift;

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
''',
    '''  static UIInput::HoldAccelerator sliderAccelerator;
  const bool fine = ui_event.shift;
  const int nav = UIInput::navCode(ui_event);

  switch (nav) {
    case GROOVEPUTER_UP:
      sliderAccelerator.reset();
      focusPrev();
      return true;
    case GROOVEPUTER_DOWN:
      sliderAccelerator.reset();
      focusNext();
      return true;
    case GROOVEPUTER_LEFT:
    case GROOVEPUTER_RIGHT: {
      const int direction = nav == GROOVEPUTER_RIGHT ? 1 : -1;
      const bool continuousSlider =
          wow_slider_->isFocused() || age_slider_->isFocused() ||
          sat_slider_->isFocused() || tone_slider_->isFocused() ||
          looper_slider_->isFocused();
      const int multiplier = continuousSlider && !fine
          ? sliderAccelerator.multiplier(direction)
          : 1;
      if (!continuousSlider || fine) sliderAccelerator.reset();

      if (wow_slider_->isFocused()) wow_slider_->adjust(direction * multiplier, fine);
      else if (age_slider_->isFocused()) age_slider_->adjust(direction * multiplier, fine);
      else if (sat_slider_->isFocused()) sat_slider_->adjust(direction * multiplier, fine);
      else if (tone_slider_->isFocused()) tone_slider_->adjust(direction * multiplier, fine);
      else if (crush_slider_->isFocused()) crush_slider_->adjust(direction, fine);
      else if (looper_slider_->isFocused()) looper_slider_->adjust(direction * multiplier, fine);
      else if (mode_ctrl_->isFocused()) {
        std::static_pointer_cast<ModeComponent>(mode_ctrl_)->cycleMode();
      } else if (preset_ctrl_->isFocused()) {
        std::static_pointer_cast<PresetComponent>(preset_ctrl_)->cyclePreset();
      }
      return true;
    }
    default:
      break;
  }

  sliderAccelerator.reset();
  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)));
''',
)

replace_once(
    "src/ui/pages/drum_sequencer_page.cpp",
    '''bool GlobalDrumFeelPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN) {
    const int nav = UIInput::navCode(ui_event);
    if (nav == GROOVEPUTER_UP) {
      if (selected_row_ > 0) selected_row_--;
      return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
      if (selected_row_ < kTotalRows - 1) selected_row_++;
      return true;
    }
    if (selected_row_ > 0 && (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)) {
      adjustDrumFx(selected_row_ - 1, nav == GROOVEPUTER_LEFT ? -kDrumStep : kDrumStep);
      return true;
    }
  }

  if (selected_row_ != 0) return false;
''',
    '''bool GlobalDrumFeelPage::handleEvent(UIEvent& ui_event) {
  static UIInput::HoldAccelerator fxAccelerator;
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN) {
    const int nav = UIInput::navCode(ui_event);
    if (nav == GROOVEPUTER_UP) {
      fxAccelerator.reset();
      if (selected_row_ > 0) selected_row_--;
      return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
      fxAccelerator.reset();
      if (selected_row_ < kTotalRows - 1) selected_row_++;
      return true;
    }
    if (selected_row_ > 0 &&
        (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)) {
      const int direction = nav == GROOVEPUTER_RIGHT ? 1 : -1;
      const int multiplier = fxAccelerator.multiplier(direction);
      adjustDrumFx(selected_row_ - 1,
                   static_cast<float>(direction * multiplier) * kDrumStep);
      return true;
    }
    fxAccelerator.reset();
  }

  if (selected_row_ != 0) return false;
''',
)

replace_once(
    "src/ui/pages/project_page.cpp",
    '#include "../ui_common.h"\n#include <algorithm>',
    '#include "../ui_common.h"\n#include "../ui_input.h"\n#include <algorithm>',
)

replace_once(
    "src/ui/pages/project_page.cpp",
    '''bool ProjectPage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

''',
    '''bool ProjectPage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

    static UIInput::HoldAccelerator mainValueAccelerator;
    if (dialog_type_ != DialogType::None) mainValueAccelerator.reset();

''',
)

replace_once(
    "src/ui/pages/project_page.cpp",
    '''    if (key == '\\t') {
        int sectionIdx = static_cast<int>(section_);
''',
    '''    if (key == '\\t') {
        mainValueAccelerator.reset();
        int sectionIdx = static_cast<int>(section_);
''',
)

replace_once(
    "src/ui/pages/project_page.cpp",
    '''        case GROOVEPUTER_UP: {
            int sectionIdx = static_cast<int>(section_);
''',
    '''        case GROOVEPUTER_UP: {
            mainValueAccelerator.reset();
            int sectionIdx = static_cast<int>(section_);
''',
)

replace_once(
    "src/ui/pages/project_page.cpp",
    '''        case GROOVEPUTER_DOWN: {
            int sectionIdx = static_cast<int>(section_);
''',
    '''        case GROOVEPUTER_DOWN: {
            mainValueAccelerator.reset();
            int sectionIdx = static_cast<int>(section_);
''',
)

replace_once(
    "src/ui/pages/project_page.cpp",
    '''            const bool right = (ui_event.scancode == GROOVEPUTER_RIGHT);
            auto& led = mini_acid_.sceneManager().currentScene().led;
            if (main_focus_ == MainFocus::Volume) {
                mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, right ? 1 : -1);
                return true;
            }
            if (main_focus_ == MainFocus::VisualStyle) {
''',
    '''            const bool right = (ui_event.scancode == GROOVEPUTER_RIGHT);
            const int direction = right ? 1 : -1;
            auto& led = mini_acid_.sceneManager().currentScene().led;
            if (main_focus_ == MainFocus::Volume) {
                const int multiplier = mainValueAccelerator.multiplier(direction);
                mini_acid_.adjustParameter(MiniAcidParamId::MainVolume,
                                           direction * multiplier);
                return true;
            }
            mainValueAccelerator.reset();
            if (main_focus_ == MainFocus::VisualStyle) {
''',
)

replace_once(
    "src/ui/pages/sampler_page.cpp",
    '#include "../../dsp/miniacid_engine.h"\n#include <cstdio>',
    '#include "../../dsp/miniacid_engine.h"\n#include "../ui_input.h"\n#include <cstdio>',
)

replace_once(
    "src/ui/pages/sampler_page.cpp",
    '''  switch (ui_event.scancode) {
    case GROOVEPUTER_UP:
      focusPrev();
      return true;
    case GROOVEPUTER_DOWN:
      focusNext();
      return true;
    case GROOVEPUTER_RIGHT:
      adjustFocusedElement(1);
      return true;
    default:
      break;
  }

  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)));
''',
    '''  static UIInput::HoldAccelerator valueAccelerator;
  const int nav = UIInput::navCode(ui_event);
  switch (nav) {
    case GROOVEPUTER_UP:
      valueAccelerator.reset();
      focusPrev();
      return true;
    case GROOVEPUTER_DOWN:
      valueAccelerator.reset();
      focusNext();
      return true;
    case GROOVEPUTER_LEFT:
    case GROOVEPUTER_RIGHT: {
      const int direction = nav == GROOVEPUTER_RIGHT ? 1 : -1;
      const bool continuous =
          volume_ctrl_->isFocused() || pitch_ctrl_->isFocused() ||
          start_ctrl_->isFocused() || end_ctrl_->isFocused();
      const int multiplier = continuous
          ? valueAccelerator.multiplier(direction)
          : 1;
      if (!continuous) valueAccelerator.reset();
      adjustFocusedElement(direction * multiplier);
      return true;
    }
    default:
      break;
  }

  valueAccelerator.reset();
  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)));
''',
)

print("Applied slider hold acceleration coverage v2")
