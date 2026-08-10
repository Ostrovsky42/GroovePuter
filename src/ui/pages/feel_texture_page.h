#pragma once

#include "feel_page.h"

#include <string>
#include <utility>

class FeelTexturePage : public IPage {
 public:
  FeelTexturePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;

  const std::string& getTitle() const override;
  void setVisualStyle(VisualStyle style) override;

 private:
  AudioGuard audio_guard_;
  FeelPage feel_page_;
};
