#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "../ui_core.h"
#include "../ui_input.h"
#include "../ui_view_continuity.h"
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
    Repeats,
    Preset,
  };

  void captureViewContinuity(UI::UiViewContinuityState& state) const override {
    state.feelFocus = static_cast<uint8_t>(focus_);
    state.feelPreset = static_cast<uint8_t>(preset_index_);
  }

  void restoreViewContinuity(const UI::UiViewContinuityState& state) override {
    uint8_t focus = state.feelFocus;
    if (focus > static_cast<uint8_t>(FocusRow::Preset)) focus = 0;
    focus_ = static_cast<FocusRow>(focus);
    preset_index_ = state.feelPreset < 3 ? state.feelPreset : 1;
    hold_accel_.reset();
  }

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
