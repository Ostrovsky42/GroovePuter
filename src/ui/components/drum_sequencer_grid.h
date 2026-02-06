#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../../dsp/grooveputer_engine.h"
#include <functional>

/**
 * Reusable Drum Sequencer Grid Component.
 * 
 * Displays a step grid with 8 drum voices and accent row.
 * Supports mouse interaction and visual highlighting.
 */
class DrumSequencerGridComponent : public Component {
 public:
  struct Callbacks {
    std::function<void(int step, int voice)> onToggle;
    std::function<void(int step)> onToggleAccent;
    std::function<int()> cursorStep;
    std::function<int()> cursorVoice;
    std::function<bool()> gridFocused;
    std::function<int()> currentStep;
  };

  DrumSequencerGridComponent(GroovePuter& mini_acid, Callbacks callbacks);

  bool handleEvent(UIEvent& ui_event) override;
  void draw(IGfx& gfx) override;

 private:
  struct GridLayout {
    int bounds_x = 0;
    int bounds_y = 0;
    int bounds_w = 0;
    int bounds_h = 0;
    int grid_x = 0;
    int grid_y = 0;
    int grid_w = 0;
    int grid_h = 0;
    int grid_right = 0;
    int grid_bottom = 0;
    int cell_w = 0;
    int stripe_h = 0;
    int accent_y = 0;
    int accent_h = 0;
    int accent_bottom = 0;
    int accent_gap = 0;
  };

  bool computeLayout(GridLayout& layout) const;

  GroovePuter& mini_acid_;
  Callbacks callbacks_;
};
