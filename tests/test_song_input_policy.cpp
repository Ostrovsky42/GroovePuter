#include <cassert>

#include "src/ui/pages/song_input_policy.h"

int main() {
  using GroovePuterSongInput::AltVerticalRoute;
  using GroovePuterSongInput::classifyAltVerticalRoute;

  assert(classifyAltVerticalRoute(false, false, true, false) ==
         AltVerticalRoute::None);
  assert(classifyAltVerticalRoute(true, false, false, false) ==
         AltVerticalRoute::None);

  assert(classifyAltVerticalRoute(true, false, true, false) ==
         AltVerticalRoute::PatternAdjust);

  assert(classifyAltVerticalRoute(true, true, true, false) ==
         AltVerticalRoute::DelegateLegacy);
  assert(classifyAltVerticalRoute(true, true, true, true) ==
         AltVerticalRoute::DelegateLegacy);
  assert(classifyAltVerticalRoute(true, false, true, true) ==
         AltVerticalRoute::DelegateLegacy);

  return 0;
}
