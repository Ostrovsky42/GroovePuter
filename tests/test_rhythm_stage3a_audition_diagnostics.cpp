#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/audition/rhythm_audition_catalog.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

uint64_t signature(const RhythmPhrasePlan& plan) {
  uint64_t hash = 1469598103934665603ull;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      hash ^= plan.bars[bar].roles[role].structural;
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

uint8_t distinctCount(const uint64_t* values, uint8_t count) {
  uint8_t distinct = 0;
  for (uint8_t i = 0; i < count; ++i) {
    bool seen = false;
    for (uint8_t j = 0; j < i; ++j) {
      if (values[j] == values[i]) {
        seen = true;
        break;
      }
    }
    if (!seen) ++distinct;
  }
  return distinct;
}

}  // namespace

int main() {
  assert(validateRhythmCatalog(Audition::catalog()));
  for (uint8_t i = 0; i < Audition::definitionCount(); ++i) {
    const Audition::Definition& definition = Audition::definition(i);
    uint64_t signatures[32]{};
    uint8_t valid = 0;
    for (uint32_t seed = 1; seed <= 32; ++seed) {
      RhythmRealizationRequest request{};
      request.catalog = &Audition::catalog();
      request.archetypeId = definition.archetypeId;
      request.phraseBars = 1;
      request.level = RealizationLevel::P1Canonical;
      request.generation.projectSeed = seed;
      const RhythmRealizationResult result = realizeRhythmPhrase(request);
      if (result.status != RealizationStatus::InvalidConstraintSet) {
        signatures[valid++] = signature(result.plan);
      }
    }
    std::printf("STAGE3A-DIAG %s valid=%u distinct=%u\n",
                definition.name,
                static_cast<unsigned>(valid),
                static_cast<unsigned>(distinctCount(signatures, valid)));
  }
  return 0;
}
