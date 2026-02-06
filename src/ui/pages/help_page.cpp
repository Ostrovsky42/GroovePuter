#include "help_page.h"

#include "../help_dialog_frames.h"

HelpPage::HelpPage() : scroll_y_(0), total_content_h_(0) {}

void HelpPage::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y - scroll_y_;
  int w = bounds.w;
  int h = bounds.h;

  int current_y = y;
  int section_spacing = 10;

  // Add all help sections
  drawHelpPageTransport(gfx, x, current_y, w, 200);
  current_y += 100; // rough estimate for now

  drawHelpPage303(gfx, x, current_y, w, 200);
  current_y += 100;

  drawHelpPage303PatternEdit(gfx, x, current_y, w, 200);
  current_y += 100;

  drawHelpPageDrumPatternEdit(gfx, x, current_y, w, 200);
  current_y += 80;

  drawHelpPageSong(gfx, x, current_y, w, 200);
  current_y += 80;

  drawHelpPageSongCont(gfx, x, current_y, w, 200);
  current_y += 80;

  drawHelpPageTape(gfx, x, current_y, w, 200);
  current_y += 100;

  total_content_h_ = (current_y + scroll_y_) - bounds.y;

  // Draw a simple scroll indicator if content is longer than screen
  if (total_content_h_ > h) {
    int bar_h = 10;
    int scroll_bar_y = bounds.y + (h - bar_h) * scroll_y_ / (total_content_h_ - h);
    gfx.fillRect(bounds.x + bounds.w - 2, scroll_bar_y, 2, bar_h, COLOR_ACCENT);
  }
}

bool HelpPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  
  const int scroll_step = 10;
  const Rect& bounds = getBoundaries();

  switch (ui_event.scancode) {
    case GROOVEPUTER_UP:
      scroll_y_ = std::max(0, scroll_y_ - scroll_step);
      return true;
    case GROOVEPUTER_DOWN:
      if (total_content_h_ > bounds.h) {
          scroll_y_ = std::min(total_content_h_ - bounds.h, scroll_y_ + scroll_step);
      }
      return true;
    default:
      break;
  }
  return false;
}

const std::string & HelpPage::getTitle() const {
  static std::string title = "HELP (UP/DN to scroll)";
  return title;
}
