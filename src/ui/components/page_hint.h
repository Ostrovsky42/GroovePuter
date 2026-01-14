#pragma once

#include <functional>
#include <cstdio>
#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../ui_themes.h"

class PageHint : public Component {
 public:
  PageHint(std::function<int()> get_page_index, 
           std::function<int()> get_page_count,
           std::function<void()> prev_callback,
           std::function<void()> next_callback)
      : get_page_index_(get_page_index), 
        get_page_count_(get_page_count),
        prev_callback_(prev_callback),
        next_callback_(next_callback) {}

  void draw(IGfx& gfx) override {
    char buf[32];
    snprintf(buf, sizeof(buf), "[< %d/%d >]", get_page_index_() + 1, get_page_count_());
    const auto& palette = getPalette(g_currentTheme);
    gfx.setTextColor(palette.ink);
    gfx.drawText(dx(), dy(), buf);
    gfx.setTextColor(palette.ink);
  }

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type == MINIACID_MOUSE_DOWN && ui_event.button == MOUSE_BUTTON_LEFT) {
      if (contains(ui_event.x, ui_event.y)) {
        // Left half = previous, right half = next
        int midpoint = dx() + width() / 2;
        if (ui_event.x < midpoint) {
          prev_callback_();
        } else {
          next_callback_();
        }
        return true;
      }
    }
    return false;
  }

  bool isFocusable() const override { return false; }

 private:
  std::function<int()> get_page_index_;
  std::function<int()> get_page_count_;
  std::function<void()> prev_callback_;
  std::function<void()> next_callback_;
};
