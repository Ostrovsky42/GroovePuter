#pragma once

#include <functional>
#include <string>

#include "../ui_core.h"

class BankSelectionBarComponent : public FocusableComponent {
 public:
  struct State {
    int bank_count = 4;
    int selected_index = 0;
    int cursor_index = 0;
    bool show_cursor = false;
    bool song_mode = false;
  };

  struct Callbacks {
    std::function<void(int index)> onSelect;
    std::function<void(int index)> onCursorMove;
  };

  BankSelectionBarComponent(std::string label, std::string letters);

  void setState(const State& state);
  void setCallbacks(Callbacks callbacks);
  int barHeight(IGfx& gfx) const;

  bool handleEvent(UIEvent& ui_event) override;
  void draw(IGfx& gfx) override;

 private:
  struct Layout {
    int bounds_x = 0;
    int bounds_y = 0;
    int bounds_w = 0;
    int label_y = 0;
    int label_h = 0;
    int label_w = 0;
    int box_size = 0;
    int spacing = 2;
    int bank_x = 0;
    int bank_y = 0;
  };

  bool computeLayout(IGfx& gfx, Layout& layout) const;
  char bankLetter(int index) const;

  std::string label_;
  std::string letters_;
  State state_{};
  Callbacks callbacks_{};
  mutable Layout last_layout_{};
  mutable bool last_layout_valid_ = false;
};
