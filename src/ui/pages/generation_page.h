#pragma once

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
