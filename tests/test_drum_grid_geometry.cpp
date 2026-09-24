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
  // All styles put pattern 1..8 and bank A/B in one selector row directly
  // below the shell header; the grid starts at y=28.
  const int gridY = 28;
  assert(gridBottom(gridY, Layout::SCREEN_H - gridY) <= kShellBottom);

  // The shared selector row buys every lane 9px: 8px labels get a 1px gap.
  assert(DrumGridGeometry::laneHeight(gridY, Layout::SCREEN_H - gridY,
                                      kStepHeader, kLanes) >= 9);

  // Bounds that already end above the shell band are left untouched.
  assert(DrumGridGeometry::laneHeight(20, 48, kStepHeader, kLanes) == 5);

  // Degenerate bounds never produce a zero-height lane.
  assert(DrumGridGeometry::laneHeight(100, 4, kStepHeader, kLanes) == 1);

  return 0;
}
