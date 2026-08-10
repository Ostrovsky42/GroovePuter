#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

uint8_t popcount16(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    mask = static_cast<StepMask>(mask & (mask - 1u));
    ++count;
  }
  return count;
}

uint16_t countSecondary(const RhythmPhrasePlan& plan) {
  uint16_t count = 0;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      count = static_cast<uint16_t>(
          count + popcount16(plan.bars[bar].roles[role].secondary));
    }
  }
  return count;
}

uint16_t countGhosts(const RhythmPhrasePlan& plan) {
  uint16_t count = 0;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      count = static_cast<uint16_t>(
          count + popcount16(plan.bars[bar].roles[role].ghosts));
    }
  }
  return count;
}

bool sameIdentity(const PhraseRhythmIdentity& a,
                  const PhraseRhythmIdentity& b) {
  if (a.archetypeId != b.archetypeId ||
      a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) {
    return false;
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    if (a.protectedSpaces[i].steps != b.protectedSpaces[i].steps ||
        a.protectedSpaces[i].affectedRoles !=
            b.protectedSpaces[i].affectedRoles) {
      return false;
    }
  }
  return true;
}

RhythmRealizationResult realize(uint32_t seed,
                                RealizationLevel level,
                                const PhraseRhythmIdentity* reuse = nullptr) {
  RhythmRealizationRequest request{};
  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = 713;  // funk_house_bridge: broad audited candidate space
  request.phraseBars = 1;
  request.level = level;
  request.generation.projectSeed = seed;
  request.generation.phraseOrdinal = static_cast<uint16_t>(seed & 0xFFFFu);
  request.reuseIdentity = reuse;
  return realizeRhythmPhrase(request);
}

bool valid(const RhythmRealizationResult& result) {
  return result.status == RealizationStatus::Ok ||
         result.status == RealizationStatus::ValidButSparse;
}

void testPLevelsAreCumulativeAndIdentityStable() {
  uint32_t observedCases = 0;
  uint32_t observedP3Secondary = 0;
  uint32_t observedP3Ghosts = 0;
  uint32_t totalP2Surface = 0;
  uint32_t totalP3Surface = 0;

  for (uint32_t seed = 1; seed <= 128; ++seed) {
    const RhythmRealizationResult p1 =
        realize(seed, RealizationLevel::P1Canonical);
    assert(valid(p1));
    assert(countSecondary(p1.plan) == 0);
    assert(countGhosts(p1.plan) == 0);

    const RhythmRealizationResult p2 =
        realize(seed, RealizationLevel::P2Variation, &p1.identity);
    const RhythmRealizationResult p3 =
        realize(seed, RealizationLevel::P3Transformation, &p1.identity);
    assert(valid(p2));
    assert(valid(p3));
    assert(sameIdentity(p1.identity, p2.identity));
    assert(sameIdentity(p1.identity, p3.identity));

    const uint16_t p2Secondary = countSecondary(p2.plan);
    const uint16_t p2Ghosts = countGhosts(p2.plan);
    const uint16_t p3Secondary = countSecondary(p3.plan);
    const uint16_t p3Ghosts = countGhosts(p3.plan);

    // ReferenceVocabulary remains backward-compatible: P2 is the existing
    // subtle ghost layer. P3 adds its structural surface while inheriting the
    // P2 ghost layer instead of replacing it.
    assert(p2Secondary == 0);
    assert(p2Ghosts <= 2);
    assert(p3Secondary <= 3);
    assert(p3Ghosts <= 2);

    totalP2Surface += p2Secondary + p2Ghosts;
    totalP3Surface += p3Secondary + p3Ghosts;
    if (p3Secondary != 0) ++observedP3Secondary;
    if (p3Ghosts != 0) ++observedP3Ghosts;
    ++observedCases;
  }

  assert(observedCases == 128);
  assert(observedP3Secondary != 0);
  assert(observedP3Ghosts != 0);
  assert(totalP3Surface > totalP2Surface);
}

void testExtendedBudgetsPreserveLegacyAggregateLayout() {
  // Six-field aggregate initialization is used by older tests/catalog data.
  // The appended Stage 14.1 fields must default to zero without shifting any
  // existing field.
  constexpr MutationBudget legacy{
      2, 1, 3, 4, AllowGhostConversion, kP2TransformationIntents};
  static_assert(legacy.maxAdds == 2, "legacy maxAdds shifted");
  static_assert(legacy.maxDrops == 1, "legacy maxDrops shifted");
  static_assert(legacy.maxDisplacements == 3,
                "legacy maxDisplacements shifted");
  static_assert(legacy.maxAccentChanges == 4,
                "legacy maxAccentChanges shifted");
  static_assert(legacy.flags == AllowGhostConversion, "legacy flags shifted");
  static_assert(legacy.allowedIntents == kP2TransformationIntents,
                "legacy intents shifted");
  static_assert(legacy.maxSecondaryAdds == 0,
                "new secondary budget must default to zero");
  static_assert(legacy.maxGhostAdds == 0,
                "new ghost budget must default to zero");
}

}  // namespace

int main() {
  testPLevelsAreCumulativeAndIdentityStable();
  testExtendedBudgetsPreserveLegacyAggregateLayout();
  return 0;
}
