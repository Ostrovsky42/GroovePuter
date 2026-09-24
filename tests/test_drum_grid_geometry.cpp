#include <cassert>

#include "src/ui/components/drum_grid_geometry.h"

namespace {

constexpr int kStepHeader = 8;
constexpr int kLanes = 8;
constexpr int kShellBottom = Layout::CONTENT.y + Layout::CONTENT.h;  // 109

int gridBottom(int boundsY, int boundsH) {
  const int lane =
      DrumGridGeometry::laneHeight(boundsY, boundsH, kStepHeader, kLanes);
  return boundsY + kStepHeader + lane * kLanes;
}

}  // namespace

int main() {
  static_assert(kShellBottom == Layout::PERFORMANCE_HUD.y,
                "lanes must end where the shell performance HUD begins");

  // Pages hand the grid bounds that run to the bottom of the 135px screen.
  // Hardware showed the last lanes (9RIM/0CLP) under the HUD/footer.
  const int minimalY = 30;  // below the Minimal pattern/bank bars
  assert(gridBottom(minimalY, Layout::SCREEN_H - minimalY) <= kShellBottom);

  const int retroAmberY = 36;  // Retro/Amber: BK selector ends at y+36
  assert(gridBottom(retroAmberY, Layout::SCREEN_H - retroAmberY) <= kShellBottom);

  // An 8px lane keeps the 8px lane labels from overlapping each other.
  assert(DrumGridGeometry::laneHeight(retroAmberY, Layout::SCREEN_H - retroAmberY,
                                      kStepHeader, kLanes) >= 8);

  // Bounds that already end above the shell band are left untouched.
  assert(DrumGridGeometry::laneHeight(20, 48, kStepHeader, kLanes) == 5);

  // Degenerate bounds never produce a zero-height lane.
  assert(DrumGridGeometry::laneHeight(100, 4, kStepHeader, kLanes) == 1);

  return 0;
}
