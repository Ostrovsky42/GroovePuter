#pragma once

#include <functional>
#include <cstdio>
#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class ModeButton : public Component {
 public:
  ModeButton(std::function<bool()> is_song_mode, std::function<void()> toggle_callback)
      : is_song_mode_(is_song_mode), toggle_callback_(toggle_callback) {}

  void draw(IGfx& gfx) override {
    bool songMode = is_song_mode_();
    IGfxColor modeColor = songMode ? IGfxColor::Green() : IGfxColor::Blue();
    
    gfx.fillRect(dx(), dy(), width(), height(), COLOR_PANEL);
    gfx.drawRect(dx(), dy(), width(), height(), modeColor);
    
    char modeLabel[32];
    snprintf(modeLabel, sizeof(modeLabel), "MODE:%s", songMode ? "SONG" : "PAT");
    int twMode = textWidth(gfx, modeLabel);
    int label_h = gfx.fontHeight();
    
    gfx.setTextColor(modeColor);
    gfx.drawText(dx() + (width() - twMode) / 2, dy() + height() / 2 - label_h / 2, modeLabel);
    gfx.setTextColor(COLOR_WHITE);
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
  std::function<bool()> is_song_mode_;
  std::function<void()> toggle_callback_;
};
