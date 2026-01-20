#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../ui_core.h"

class ComboBoxComponent : public FocusableComponent {
 public:
  explicit ComboBoxComponent(std::vector<std::string> options);
  explicit ComboBoxComponent(std::vector<std::shared_ptr<Component>> options);

  void setOptions(std::vector<std::string> options);
  void setOptionComponents(std::vector<std::shared_ptr<Component>> options);
  void setSelectedIndex(int index);
  int selectedIndex() const { return selected_index_; }
  int optionCount() const { return static_cast<int>(options_.size()); }

  bool handleEvent(UIEvent& ui_event) override;
  void draw(IGfx& gfx) override;

 private:
  int clampIndex(int index) const;

  std::vector<std::shared_ptr<Component>> options_;
  int selected_index_ = 0;
};
