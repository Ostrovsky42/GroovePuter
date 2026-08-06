#pragma once

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
