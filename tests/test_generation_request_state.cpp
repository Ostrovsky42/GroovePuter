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

  assert(std::strcmp(generationLevelCode(RealizationLevel::P1Canonical),
                     "P1") == 0);
  assert(std::strcmp(generationLevelCode(RealizationLevel::P2Variation),
                     "P2") == 0);
  assert(std::strcmp(generationLevelCode(RealizationLevel::P3Transformation),
                     "P3") == 0);
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

  // GA-01 / GA-03: attemptOrdinal belongs to the accepted generation tuple and
  // is allocated monotonically when that tuple is accepted, not at publication.
  resetGenerationAttemptState();
  assert(generationAttemptCapacity() == 64);
  auto attempt = allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, 42);
  assert(attempt.ok() && attempt.ordinal == 0);
  attempt = allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, 42);
  assert(attempt.ok() && attempt.ordinal == 1);
  attempt = allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, 42);
  assert(attempt.ok() && attempt.ordinal == 2);

  // Every tuple axis is identity-bearing. Switching one axis starts its own
  // ordinal sequence and does not reset the original tuple.
  assert(allocateGenerationAttempt(2, 6,
      RealizationLevel::P2Variation, 42).ordinal == 0);
  assert(allocateGenerationAttempt(1, 7,
      RealizationLevel::P2Variation, 42).ordinal == 0);
  assert(allocateGenerationAttempt(1, 6,
      RealizationLevel::P1Canonical, 42).ordinal == 0);
  assert(allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, 43).ordinal == 0);
  attempt = allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, 42);
  assert(attempt.ok() && attempt.ordinal == 3);

  // Invalid requests are not accepted and therefore consume no ordinal.
  resetGenerationAttemptState();
  assert(allocateGenerationAttempt(16, 6,
      RealizationLevel::P2Variation, 42).status ==
      GenerationAttemptStatus::InvalidTuple);
  assert(allocateGenerationAttempt(1, 6,
      RealizationLevel::Count, 42).status ==
      GenerationAttemptStatus::InvalidTuple);
  assert(allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, -1).status ==
      GenerationAttemptStatus::InvalidTuple);
  attempt = allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, 42);
  assert(attempt.ok() && attempt.ordinal == 0);

  // Capacity is a history-comfort bound, not a generation-availability bound.
  // Fill the exact table, then accept another full table of distinct tuples.
  // Every new tuple must still start at attempt 0. After a complete eviction
  // cycle, an old tuple also restarts at 0 while a non-evicted recent tuple
  // continues monotonically.
  resetGenerationAttemptState();
  for (int address = 0; address < 64; ++address) {
    const auto filled = allocateGenerationAttempt(
        1, 6, RealizationLevel::P2Variation, address);
    assert(filled.ok() && filled.ordinal == 0);
  }
  attempt = allocateGenerationAttempt(
      1, 6, RealizationLevel::P2Variation, 0);
  assert(attempt.ok() && attempt.ordinal == 1);

  for (int address = 64; address < 128; ++address) {
    const auto rerouted = allocateGenerationAttempt(
        1, 6, RealizationLevel::P2Variation, address);
    assert(rerouted.ok() && rerouted.ordinal == 0);
  }

  attempt = allocateGenerationAttempt(
      1, 6, RealizationLevel::P2Variation, 0);
  assert(attempt.ok() && attempt.ordinal == 0);
  attempt = allocateGenerationAttempt(
      1, 6, RealizationLevel::P2Variation, 127);
  assert(attempt.ok() && attempt.ordinal == 1);

  // Session reset is explicit and non-persistent, including eviction order.
  resetGenerationAttemptState();
  attempt = allocateGenerationAttempt(1, 6,
      RealizationLevel::P2Variation, 42);
  assert(attempt.ok() && attempt.ordinal == 0);

  // Restore the compatibility default so this test is order-independent if it
  // is ever embedded in a larger host runner.
  setGenerationLevel(RealizationLevel::P2Variation);
  resetGenerationAttemptState();
  return 0;
}
