#pragma once

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
    Profile = 0,
    Swing,
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
  FocusRow focus_ = FocusRow::Profile;
  int preset_index_ = 1;
  UIInput::HoldAccelerator hold_accel_;
  std::string title_ = "FEEL";
};
