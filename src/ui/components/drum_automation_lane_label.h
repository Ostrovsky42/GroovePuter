#pragma once

#include <string>

#include "../ui_core.h"

class DrumAutomationLaneLabel : public Component {
 public:
  DrumAutomationLaneLabel(MiniAcid& mini_acid, DrumAutomationParamId param_id,
                          std::string text);

  void setText(std::string text);
  const std::string& text() const { return text_; }
  bool handleEvent(UIEvent& ui_event) override;

  void draw(IGfx& gfx) override;

 private:
  Rect squareRect(const Rect& bounds) const;

  MiniAcid& mini_acid_;
  DrumAutomationParamId param_id_;
  std::string text_;
};
