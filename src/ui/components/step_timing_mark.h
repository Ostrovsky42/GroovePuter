#pragma once

#include "../ui_core.h"
#include "../../state/step_setting_marks.h"

// Micro-timing mark on a step cell (synth STEPS grid and DRUMS grid): a 2x2
// square in the top corner of the cell, left for an early step (TIMING < 0) and
// right for a late one (TIMING > 0); nothing at TIMING 0. Synth and drum TIMING
// are the same unit (ticks, -23..23, + = later). The caller picks a colour that
// reads on its own cell fill and draws the mark after the fill, before the
// cursor / playhead borders. No allocation, no state.
namespace StepTimingMark {

constexpr int kSize = 2;

inline void draw(IGfx& gfx, int cellX, int cellY, int cellW, int inset, int timing,
                 IGfxColor color) {
  const int side = GroovePuterUndo::StepSettingMarks::timingMarkSide(timing);
  if (side == 0 || cellW < 2 * (inset + kSize)) return;
  const int x = side < 0 ? cellX + inset : cellX + cellW - inset - kSize;
  gfx.fillRect(x, cellY + inset, kSize, kSize, color);
}

}  // namespace StepTimingMark
