#include "../src/dsp/song_cycle_boundary.h"

#include <cassert>

namespace {

void verifyCycle(int bars) {
  SongCycleBoundary boundary{-1, false};

  boundary = nextSongCycleBoundary(boundary.barIndex, bars);
  assert(boundary.barIndex == 0);
  assert(!boundary.advanceRow);

  for (int bar = 1; bar < bars; ++bar) {
    boundary = nextSongCycleBoundary(boundary.barIndex, bars);
    assert(boundary.barIndex == bar);
    assert(!boundary.advanceRow);
  }

  boundary = nextSongCycleBoundary(boundary.barIndex, bars);
  assert(boundary.barIndex == 0);
  assert(boundary.advanceRow);
}

}  // namespace

int main() {
  verifyCycle(1);
  verifyCycle(2);
  verifyCycle(4);
  verifyCycle(8);

  SongCycleBoundary restarted{-1, false};
  restarted = nextSongCycleBoundary(restarted.barIndex, 2);
  assert(restarted.barIndex == 0);
  assert(!restarted.advanceRow);

  SongCycleBoundary invalid{-1, false};
  invalid = nextSongCycleBoundary(invalid.barIndex, 3);
  assert(invalid.barIndex == 0);
  assert(!invalid.advanceRow);
  invalid = nextSongCycleBoundary(invalid.barIndex, 3);
  assert(invalid.barIndex == 0);
  assert(invalid.advanceRow);
}
