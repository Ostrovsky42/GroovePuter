// Every GenerativeMode must have a non-empty drum template: kick anchors,
// a snare or clap, and hats. Rows were silently zero for modes 9..15.
#include <cstdio>
#include <cstdint>

#include "src/dsp/drum_genre_templates.h"

namespace {
int popcount16(uint16_t v) {
  int n = 0;
  for (; v; v >>= 1) n += v & 1;
  return n;
}
}  // namespace

int main() {
  int failures = 0;
  for (int m = 0; m < kGenerativeModeCount; ++m) {
    const DrumGenreTemplate& t = kDrumTemplates[m];
    const int kick = popcount16(t.kickMask);
    const int snare = popcount16(t.snareMask);
    const int hats = popcount16(t.hatMask);
    // Reggae is a one-drop groove: a single kick on beat 3 is its identity.
    const int minKick = (m == static_cast<int>(GenerativeMode::Reggae)) ? 1 : 2;
    const bool ok = kick >= minKick && (snare >= 1 || t.useClap) && hats >= 2;
    std::printf("mode %2d kick=%d snare=%d hats=%d open=%d %s\n", m, kick, snare,
                hats, popcount16(t.openHatMask), ok ? "ok" : "FAIL");
    if (!ok) ++failures;
  }
  if (failures) {
    std::fprintf(stderr, "FAIL: %d modes with incomplete drum template\n", failures);
    return 1;
  }
  std::printf("drum genre template coverage: PASS\n");
  return 0;
}
