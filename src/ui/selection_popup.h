#pragma once

#include <algorithm>
#include <cstdint>

#include "screen_geometry.h"
#include "ui_core.h"
#include "ui_theme.h"
#include "ui_widgets.h"

namespace UI {

enum class SelectionPopupResult : uint8_t {
  None = 0,
  Confirmed,
  Cancelled,
};

// Small modal selector shared by pages that already expose a value through
// left/right stepping. It owns navigation only; the page remains responsible
// for applying the selected value to its domain model.
class SelectionPopup final {
 public:
  bool open(const char* title, uint8_t itemCount, uint8_t selectedIndex) {
    if (title == nullptr || itemCount == 0 || selectedIndex >= itemCount) {
      close();
      return false;
    }
    title_ = title;
    itemCount_ = itemCount;
    selectedIndex_ = selectedIndex;
    open_ = true;
    return true;
  }

  void close() {
    open_ = false;
    itemCount_ = 0;
    selectedIndex_ = 0;
    title_ = nullptr;
  }

  bool isOpen() const { return open_; }
  uint8_t selectedIndex() const { return selectedIndex_; }
  uint8_t itemCount() const { return itemCount_; }

  SelectionPopupResult handleEvent(const UIEvent& event) {
    if (!open_ || event.event_type != GROOVEPUTER_KEY_DOWN) {
      return SelectionPopupResult::None;
    }
    const int nav = (event.scancode == GROOVEPUTER_UP ||
                     event.scancode == GROOVEPUTER_DOWN ||
                     event.scancode == GROOVEPUTER_LEFT ||
                     event.scancode == GROOVEPUTER_RIGHT)
                        ? event.scancode
                        : ((event.key == GROOVEPUTER_UP ||
                            event.key == GROOVEPUTER_DOWN ||
                            event.key == GROOVEPUTER_LEFT ||
                            event.key == GROOVEPUTER_RIGHT)
                               ? event.key : 0);
    if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
      const int delta = nav == GROOVEPUTER_DOWN ? 1 : -1;
      int next = static_cast<int>(selectedIndex_) + delta;
      if (next < 0) next = itemCount_ - 1;
      if (next >= itemCount_) next = 0;
      selectedIndex_ = static_cast<uint8_t>(next);
      return SelectionPopupResult::None;
    }
    if (event.key == '\n' || event.key == '\r') {
      open_ = false;
      return SelectionPopupResult::Confirmed;
    }
    if (event.key == 0x1B || event.key == 0x08) {
      open_ = false;
      return SelectionPopupResult::Cancelled;
    }
    return SelectionPopupResult::None;
  }

  void draw(IGfx& gfx, const char* const* items) const {
    if (!open_ || items == nullptr) return;

    const UI::ThemePalette palette = UI::themePalette();
    constexpr int kPanelX = 8;
    constexpr int kPanelY = Layout::CONTENT.y + 2;
    constexpr int kPanelW = Layout::CONTENT.w - 16;
    constexpr int kPanelH = Layout::CONTENT.h - 4;
    constexpr int kFirstRowY = kPanelY + 14;
    constexpr int kVisibleRows = 5;

    gfx.fillRect(kPanelX, kPanelY, kPanelW, kPanelH, palette.panel);
    gfx.drawRect(kPanelX, kPanelY, kPanelW, kPanelH, palette.accent);
    gfx.setTextColor(palette.accent);
    gfx.drawText(kPanelX + 6, kPanelY + 2, title_ == nullptr ? "SELECT" : title_);

    const int first = selectedIndex_ >= kVisibleRows
        ? static_cast<int>(selectedIndex_) - kVisibleRows + 1 : 0;
    const int visible = std::min(kVisibleRows,
                                 static_cast<int>(itemCount_) - first);
    for (int row = 0; row < visible; ++row) {
      const int index = first + row;
      Widgets::drawListRow(gfx, kPanelX + 4, kFirstRowY + row * Layout::LINE_HEIGHT,
                           kPanelW - 8, items[index],
                           index == static_cast<int>(selectedIndex_), true);
    }
    gfx.setTextColor(palette.secondary);
    gfx.drawText(kPanelX + 6, kPanelY + kPanelH - 10,
                 "U/D SELECT  ENTER OK  ESC BACK");
  }

 private:
  const char* title_{nullptr};
  uint8_t itemCount_{0};
  uint8_t selectedIndex_{0};
  bool open_{false};
};

}  // namespace UI
