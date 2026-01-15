#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class HelpPage : public IPage {
 public:
  explicit HelpPage();
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;

 private:
  void drawTransportSection(IGfx& gfx, int x, int y, int w, int h);
  
  int scroll_y_ = 0;
  int total_content_h_ = 0;
};
