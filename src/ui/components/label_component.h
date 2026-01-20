#pragma once

#include <string>

#include "../ui_core.h"

enum LabelJustification {
  JUSTIFY_LEFT = 0,
  JUSTIFY_CENTER,
};

class LabelComponent : public Component {
 public:
  explicit LabelComponent(std::string text);

  void setJustification(LabelJustification justification);
  void setText(std::string text);
  void setTextColor(IGfxColor color) override;
  const std::string& text() const { return text_; }

  void draw(IGfx& gfx) override;

 private:
  std::string text_;
  LabelJustification justification_;
  IGfxColor text_color_;
};
