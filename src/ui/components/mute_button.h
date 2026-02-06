#pragma once

#include <functional>
#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../ui_themes.h"

class MuteButton : public Component {
 public:
  MuteButton(const char* label, std::function<bool()> is_muted, std::function<void()> toggle_callback)
      : label_(label), is_muted_(is_muted), toggle_callback_(toggle_callback) {}

  void draw(IGfx& gfx) override {
    const auto& palette = getPalette(g_currentTheme);
    if (!is_muted_()) {
      gfx.fillRect(dx() + 1, dy() + 1, width() - 3, height() - 2, palette.bg);
    }
    // else {
    //   gfx.fillRect(dx() + 1, dy() + 1, width() - 3, height() - 2, palette.accent);
    // }
    gfx.drawRect(dx() + 1, dy() + 1, width() - 3, height() - 2, palette.panel);
    gfx.setTextColor(is_muted_() ? palette.muted : palette.ink);
    // Center the label
    int label_w = textWidth(gfx, label_);
    int label_x = dx() + (width() - label_w) / 2;
    gfx.drawText(label_x, dy() + 6, label_);
  }

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type == GROOVEPUTER_MOUSE_DOWN && ui_event.button == MOUSE_BUTTON_LEFT) {
      if (contains(ui_event.x, ui_event.y)) {
        toggle_callback_();
        return true;
      }
    }
    return false;
  }

  bool isFocusable() const override { return false; }

 private:
  const char* label_;
  std::function<bool()> is_muted_;
  std::function<void()> toggle_callback_;
};
