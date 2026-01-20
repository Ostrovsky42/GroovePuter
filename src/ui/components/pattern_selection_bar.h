#pragma once

#include <functional>
#include <string>

#include "../ui_core.h"

class PatternSelectionBarComponent : public FocusableComponent {
 public:
 struct State {
    int pattern_count = 8;
    int selected_index = -1;
    int cursor_index = 0;
    bool show_cursor = false;
    bool song_mode = false;
    int columns = 8;
  };

  struct Callbacks {
    std::function<void(int index)> onSelect;
    std::function<void(int index)> onCursorMove;
  };

  explicit PatternSelectionBarComponent(std::string label);

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
    int pattern_size = 0;
    int pattern_height = 0;
    int spacing = 4;
    int columns = 8;
    int rows = 1;
    int row_spacing = 0;
    int bar_height = 0;
    int row_y = 0;
  };

  bool computeLayout(IGfx& gfx, Layout& layout) const;

  std::string label_;
  State state_{};
  Callbacks callbacks_{};
  mutable Layout last_layout_{};
  mutable bool last_layout_valid_ = false;
};
