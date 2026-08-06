#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "src/state/scene_revision.h"

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

  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
    GroovePuterState::markSceneMutated();
  }

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  VisualStyle style_ = VisualStyle::MINIMAL;
  std::string title_ = "GENERATION";
};
