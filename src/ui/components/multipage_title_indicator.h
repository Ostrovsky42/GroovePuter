#pragma once

#include <functional>
#include <utility>

#include "../ui_colors.h"
#include "../ui_core.h"
#include "../ui_utils.h"

class MultiPageTitleIndicator : public Component {
 public:
  MultiPageTitleIndicator(std::function<void()> up_callback,
                          std::function<void()> down_callback)
      : up_callback_(std::move(up_callback)),
        down_callback_(std::move(down_callback)) {}

  void setVisible(bool visible) { visible_ = visible; }

  void draw(IGfx& gfx) override {
    if (!visible_) return;
    gfx.fillRect(dx(), dy(), width(), height(), COLOR_WHITE);
    gfx.setTextColor(COLOR_BLACK);
    
    int mid_y = dy() + height() / 2;
    int center_x = dx() + width() / 2;
    
    // Upper half: arrow pointing up (two lines forming ^)
    int upper_y = dy() + height() / 4 - 1;
    gfx.drawLine(center_x - 3, upper_y + 2, center_x, upper_y - 1);
    gfx.drawLine(center_x, upper_y - 1, center_x + 3, upper_y + 2);
    
    // Lower half: arrow pointing down (two lines forming v)
    int lower_y = mid_y + height() / 4 + 1;
    gfx.drawLine(center_x - 3, lower_y - 2, center_x, lower_y + 1);
    gfx.drawLine(center_x, lower_y + 1, center_x + 3, lower_y - 2);
    
    gfx.setTextColor(COLOR_WHITE);
  }

  bool handleEvent(UIEvent& ui_event) override {
    if (!visible_) return false;
    if (ui_event.event_type == MINIACID_MOUSE_DOWN &&
        ui_event.button == MOUSE_BUTTON_LEFT &&
        contains(ui_event.x, ui_event.y)) {
      int midpoint = dy() + height() / 2;
      if (ui_event.y < midpoint) {
        if (up_callback_) up_callback_();
      } else {
        if (down_callback_) down_callback_();
      }
      return true;
    }
    return false;
  }

  bool isFocusable() const override { return false; }

 private:
  std::function<void()> up_callback_;
  std::function<void()> down_callback_;
  bool visible_ = false;
};
