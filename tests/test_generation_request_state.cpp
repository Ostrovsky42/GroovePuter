#include <cassert>
#include <cstring>

#include "src/state/generation_request_state.h"

using GroovePuterRhythm::RealizationLevel;

int main() {
  using namespace GroovePuterState;

  // Host builds have no NVS backend. The compatibility/default behavior must
  // therefore exactly match the pre-selector production level.
  assert(currentGenerationLevel() == RealizationLevel::P2Variation);
  assert(sanitizeGenerationLevel(255) == RealizationLevel::P2Variation);

  assert(nextGenerationLevel(RealizationLevel::P1Canonical) ==
         RealizationLevel::P2Variation);
  assert(nextGenerationLevel(RealizationLevel::P2Variation) ==
         RealizationLevel::P3Transformation);
  assert(nextGenerationLevel(RealizationLevel::P3Transformation) ==
         RealizationLevel::P1Canonical);
  assert(nextGenerationLevel(RealizationLevel::P1Canonical, -1) ==
         RealizationLevel::P3Transformation);

  assert(std::strcmp(generationLevelShortName(RealizationLevel::P1Canonical),
                     "P1 CANON") == 0);
  assert(std::strcmp(generationLevelShortName(RealizationLevel::P2Variation),
                     "P2 VAR") == 0);
  assert(std::strcmp(
             generationLevelShortName(RealizationLevel::P3Transformation),
             "P3 TRANS") == 0);

  assert(setGenerationLevel(RealizationLevel::P1Canonical));
  assert(currentGenerationLevel() == RealizationLevel::P1Canonical);
  assert(cycleGenerationLevel() == RealizationLevel::P2Variation);
  assert(cycleGenerationLevel() == RealizationLevel::P3Transformation);
  assert(cycleGenerationLevel() == RealizationLevel::P1Canonical);

  // Restore the compatibility default so this test is order-independent if it
  // is ever embedded in a larger host runner.
  setGenerationLevel(RealizationLevel::P2Variation);
  return 0;
}
