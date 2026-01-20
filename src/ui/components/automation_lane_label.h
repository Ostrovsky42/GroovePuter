#pragma once

#include <string>

#include "../ui_core.h"

class AutomationLaneLabel : public Component {
 public:
  AutomationLaneLabel(MiniAcid& mini_acid, TB303ParamId param_id,
                      int voice_index, std::string text);

  void setText(std::string text);
  const std::string& text() const { return text_; }
  bool handleEvent(UIEvent& ui_event) override;

 void draw(IGfx& gfx) override;

 private:
  Rect squareRect(const Rect& bounds) const;

  MiniAcid& mini_acid_;
  TB303ParamId param_id_;
  int voice_index_;
  std::string text_;
};
