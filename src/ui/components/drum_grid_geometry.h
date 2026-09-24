#pragma once

#include <algorithm>

#include "../screen_geometry.h"

namespace DrumGridGeometry {

// Height of one drum lane for grid bounds that start at boundsY. Pages hand
// the grid bounds that may run to the bottom of the screen, but the shell
// paints its performance HUD and footer over everything below CONTENT after
// the page draws, so lanes may only use the space above that band.
inline int laneHeight(int boundsY,
                      int boundsH,
                      int stepHeaderHeight,
                      int laneCount,
                      int shellContentBottom =
                          Layout::CONTENT.y + Layout::CONTENT.h) {
  const int usableBottom = std::min(boundsY + boundsH, shellContentBottom);
  const int available = std::max(1, usableBottom - boundsY - stepHeaderHeight);
  return std::max(1, available / std::max(1, laneCount));
}

}  // namespace DrumGridGeometry
