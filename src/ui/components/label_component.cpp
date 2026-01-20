#include "label_component.h"

#include "../ui_colors.h"

LabelComponent::LabelComponent(std::string text) : text_(std::move(text)),
 justification_(JUSTIFY_LEFT),
 text_color_(COLOR_BLACK) {}

void LabelComponent::setText(std::string text) {
  text_ = std::move(text);
}

void LabelComponent::setJustification(LabelJustification justification) {
  justification_ = justification;
}

void LabelComponent::setTextColor(IGfxColor color) {
  text_color_ = color;
}

void LabelComponent::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  if (bounds.w <= 0 || bounds.h <= 0) return;

  gfx.setTextColor(text_color_);
  if (justification_ == JUSTIFY_CENTER) {
    int text_width = gfx.textWidth(text_.c_str());
    int text_x = bounds.x + (bounds.w - text_width) / 2;
    int text_y = bounds.y + (bounds.h - gfx.fontHeight()) / 2;
    gfx.drawText(text_x, text_y, text_.c_str());
  } else {
    int text_y = bounds.y + (bounds.h - gfx.fontHeight()) / 2;
    gfx.drawText(bounds.x, text_y, text_.c_str());
  }
  gfx.setTextColor(COLOR_WHITE);
}
