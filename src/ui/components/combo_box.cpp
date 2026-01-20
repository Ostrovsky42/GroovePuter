#include "combo_box.h"

#include "label_component.h"
#include "../ui_colors.h"

namespace {
std::vector<std::shared_ptr<Component>> makeLabelOptions(
    std::vector<std::string> options) {
  std::vector<std::shared_ptr<Component>> components;
  components.reserve(options.size());
  for (auto& option : options) {
    components.push_back(std::make_shared<LabelComponent>(std::move(option)));
  }
  return components;
}
} // namespace

ComboBoxComponent::ComboBoxComponent(std::vector<std::string> options)
    : ComboBoxComponent(makeLabelOptions(std::move(options))) {}

ComboBoxComponent::ComboBoxComponent(
    std::vector<std::shared_ptr<Component>> options)
  : options_(std::move(options)) {
  if (selected_index_ < 0) selected_index_ = 0;
  if (!options_.empty() && selected_index_ >= static_cast<int>(options_.size())) {
    selected_index_ = static_cast<int>(options_.size()) - 1;
  }
}

void ComboBoxComponent::setOptions(std::vector<std::string> options) {
  setOptionComponents(makeLabelOptions(std::move(options)));
}

void ComboBoxComponent::setOptionComponents(
    std::vector<std::shared_ptr<Component>> options) {
  options_ = std::move(options);
  if (selected_index_ < 0) selected_index_ = 0;
  if (!options_.empty() && selected_index_ >= static_cast<int>(options_.size())) {
    selected_index_ = static_cast<int>(options_.size()) - 1;
  }
}

void ComboBoxComponent::setSelectedIndex(int index) {
  selected_index_ = clampIndex(index);
}

int ComboBoxComponent::clampIndex(int index) const {
  if (options_.empty()) return 0;
  int clamped = index;
  if (clamped < 0) clamped = 0;
  if (clamped >= static_cast<int>(options_.size())) {
    clamped = static_cast<int>(options_.size()) - 1;
  }
  return clamped;
}

bool ComboBoxComponent::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == MINIACID_MOUSE_DOWN) {
    const Rect& bounds = getBoundaries();
    if (!bounds.contains(Point{ui_event.x, ui_event.y})) return false;
    if (options_.empty()) return false;
    int row_h = bounds.h / static_cast<int>(options_.size());
    if (row_h <= 0) return false;
    int index = (ui_event.y - bounds.y) / row_h;
    selected_index_ = clampIndex(index);
    // now before returning, give a chance for the newly selected
    // component to handle the event
    if (options_[selected_index_]) {
      // we ignore the return value because we'll return true 
      // after this anyways
      options_[selected_index_]->handleEvent(ui_event);
    }
    return true;
  }
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;
  if (!isFocused()) return false;
  if (options_.empty()) return false;

  int delta = 0;
  switch (ui_event.scancode) {
    case MINIACID_UP:
      delta = -1;
      break;
    case MINIACID_DOWN:
      delta = 1;
      break;
    default:
      break;
  }

  // handle selection change based on key press
  int count = static_cast<int>(options_.size());
  int next = selected_index_;
  if (next < 0) next = 0;
  next = (next + delta) % count;
  if (next < 0) next += count;
  selected_index_ = next;

  // Pass events to the selected component to see if they need to handle it
  if (options_[selected_index_]) {
    if (options_[selected_index_]->handleEvent(ui_event)) {
      return true;
    }
  }
  
  return false;
}

void ComboBoxComponent::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  if (bounds.w <= 0 || bounds.h <= 0) return;

  int row_h = gfx.fontHeight() + 2;
  if (row_h <= 0) return;

  for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
    int row_y = bounds.y + i * row_h;
    if (row_y + row_h > bounds.y + bounds.h) break;

    bool selected = (i == selected_index_);
    if (selected) {
      gfx.fillRect(bounds.x, row_y, bounds.w, row_h, COLOR_LIGHT_GRAY);
      // gfx.drawRect(bounds.x, row_y, bounds.w, row_h, COLOR_STEP_SELECTED);
    }
    if (options_[i]) {
      if (auto label = dynamic_cast<LabelComponent*>(options_[i].get())) {
        label->setTextColor(selected ? COLOR_WHITE : COLOR_LABEL);
      }
      options_[i]->setBoundaries(Rect(bounds.x + 2, row_y, bounds.w - 2, row_h));
      options_[i]->draw(gfx);
    }
  }
  gfx.setTextColor(COLOR_WHITE);

  if (isFocused()) {
    gfx.drawRect(bounds.x, bounds.y, bounds.w, bounds.h, COLOR_STEP_SELECTED);
  }
}
