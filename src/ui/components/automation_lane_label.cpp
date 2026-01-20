#include "automation_lane_label.h"

#include "../ui_colors.h"

AutomationLaneLabel::AutomationLaneLabel(MiniAcid& mini_acid,
                                         TB303ParamId param_id,
                                         int voice_index, std::string text)
    : mini_acid_(mini_acid),
      param_id_(param_id),
      voice_index_(voice_index),
      text_(std::move(text)) {}

void AutomationLaneLabel::setText(std::string text) {
  text_ = std::move(text);
}

void AutomationLaneLabel::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  if (bounds.w <= 0 || bounds.h <= 0) return;

  int text_y = bounds.y + (bounds.h - gfx.fontHeight()) / 2;
  gfx.drawText(bounds.x, text_y, text_.c_str());

  const AutomationLane* lane = mini_acid_.automationLane303(param_id_, voice_index_);
  if (!lane || !lane->hasNodes()) return;

  Rect square = squareRect(bounds);
  if (square.w <= 1 || square.h <= 1) return;
  IGfxColor square_color = IGfxColor::Yellow();
  if (lane->enabled) {
    gfx.fillRect(square.x, square.y, square.w, square.h, square_color);
  } else {
    gfx.drawRect(square.x, square.y, square.w, square.h, square_color);
  }
}

bool AutomationLaneLabel::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == MINIACID_MOUSE_DOWN) {
    const Rect& bounds = getBoundaries();
    if (!bounds.contains(Point{ui_event.x, ui_event.y})) return false;
    const AutomationLane* lane = mini_acid_.automationLane303(param_id_, voice_index_);
    if (!lane || !lane->hasNodes()) return false;
    Rect square = squareRect(bounds);
    if (square.contains(Point{ui_event.x, ui_event.y})) {
      mini_acid_.toggleAutomationLaneEnabled303(param_id_, voice_index_);
      return true;
    }
    return false;
  }
  if (ui_event.event_type == MINIACID_KEY_DOWN) {
    if (ui_event.key == '\n' || ui_event.key == '\r') {
      mini_acid_.toggleAutomationLaneEnabled303(param_id_, voice_index_);
      return true;
    }
  }
  return false;
}

Rect AutomationLaneLabel::squareRect(const Rect& bounds) const {
  int size = 5;
  if (size > bounds.h - 2) size = bounds.h - 2;
  if (size < 2) return Rect(bounds.x, bounds.y, 0, 0);

  int pad = 2;
  int square_x = bounds.x + bounds.w - size - pad;
  if (square_x < bounds.x) square_x = bounds.x;
  int square_y = bounds.y + (bounds.h - size) / 2;
  return Rect(square_x, square_y, size, size);
}
