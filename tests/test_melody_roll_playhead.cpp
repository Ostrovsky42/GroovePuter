// The Melody roll draws its playback strip through the same helper as the
// Melody overview; the overview's original formula is restated here.
#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/ui/melody_playhead.h"

namespace {
int overviewX(uint16_t phase, int rollX, int rollW) {
  const uint16_t phaseInBar =
      static_cast<uint16_t>(phase % PhraseRuntime::kTicksPerBar);
  return rollX + static_cast<int>(
      static_cast<uint32_t>(phaseInBar) * static_cast<uint32_t>(rollW - 1) /
      PhraseRuntime::kTicksPerBar);
}
}  // namespace

int main() {
  const int widths[] = {32, 100, 212};
  for (int w : widths) {
    const int left = 24;
    int last = left;
    for (uint32_t tick = 0; tick < 4u * PhraseRuntime::kTicksPerBar; ++tick) {
      const int x = MelodyPlayhead::x(static_cast<uint16_t>(tick), left, w);
      assert(x == overviewX(static_cast<uint16_t>(tick), left, w));
      assert(x >= left && x <= left + w - 1);  // inside the plane
      if (tick % PhraseRuntime::kTicksPerBar == 0) assert(x == left);  // bar start
      else assert(x >= last);  // never moves back inside a bar
      last = x;
    }
  }
  // A: start 37, duration 50 (audible end cut by B at 70). B: start 70.
  for (uint32_t t = 37; t < 70; ++t) assert(MelodyPlayhead::isSounding(t, 37, 70));
  assert(!MelodyPlayhead::isSounding(70, 37, 70));
  assert(MelodyPlayhead::isSounding(70, 70, 120));
  assert(!MelodyPlayhead::isSounding(36, 37, 70));
  assert(!MelodyPlayhead::isSounding(36, 70, 120));
  // A chord: two notes sharing a start both sound.
  assert(MelodyPlayhead::isSounding(40, 37, 70) && MelodyPlayhead::isSounding(40, 37, 70));
  assert(MelodyPlayhead::isSounding(37, 37, 87) && MelodyPlayhead::isSounding(37, 37, 70));

  std::puts("melody roll playhead PASS");
  return 0;
}
