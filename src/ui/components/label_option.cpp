#include "label_option.h"

#include "../ui_colors.h"
#include "../ui_input.h"

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
} // namespace

LabelOptionComponent::LabelOptionComponent(const char* label,
                                           IGfxColor label_color,
                                           IGfxColor value_color)
    : label_(label ? label : ""),
      label_color_(label_color),
      value_color_(value_color) {}

void LabelOptionComponent::setOptions(std::vector<std::string> options) {
  options_ = std::move(options);
  if (option_index_ < 0) option_index_ = 0;
  if (!options_.empty() && option_index_ >= static_cast<int>(options_.size())) {
    option_index_ = 0;
  }
}

void LabelOptionComponent::setOptionIndex(int index) {
  option_index_ = index;
  if (option_index_ < 0) option_index_ = 0;
  if (!options_.empty() && option_index_ >= static_cast<int>(options_.size())) {
    option_index_ = static_cast<int>(options_.size()) - 1;
  }
}

bool LabelOptionComponent::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;
  if (!isFocused()) return false;
  if (options_.empty()) return false;

  int nav = UIInput::navCode(ui_event);
  switch (nav) {
    case MINIACID_UP:
    case MINIACID_RIGHT:
      option_index_ = (option_index_ + 1) % options_.size();
      return true;
    case MINIACID_DOWN:
    case MINIACID_LEFT:
      option_index_ = (option_index_ - 1 + options_.size()) % options_.size();
      return true;
    default:
      break;
  }
  return false;
}

void LabelOptionComponent::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  gfx.setTextColor(label_color_);
  gfx.drawText(bounds.x, bounds.y, label_.c_str());

  int label_w = textWidth(gfx, label_.c_str());
  gfx.setTextColor(value_color_);
  gfx.drawText(bounds.x + label_w + 3, bounds.y, currentOption());

  if (isFocused()) {
    int pad = 2;
    gfx.drawRect(bounds.x - pad, bounds.y - pad,
                 bounds.w + pad * 2, bounds.h + pad * 2, kFocusColor);
  }
}

const char* LabelOptionComponent::currentOption() const {
  if (options_.empty()) return "";
  if (option_index_ < 0) return options_[0].c_str();
  if (option_index_ >= static_cast<int>(options_.size())) {
    return options_[0].c_str();
  }
  return options_[option_index_].c_str();
}
