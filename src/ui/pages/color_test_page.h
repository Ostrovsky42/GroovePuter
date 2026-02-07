#pragma once

#include "../ui_core.h"
#include <string>

class ColorTestPage : public IPage {
public:
  ColorTestPage(IGfx& gfx, MiniAcid& mini_acid);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;

private:
  IGfx& gfx_;
  MiniAcid& mini_acid_;
  std::string title_ = "COLOR TEST";
};
