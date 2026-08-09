#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>

#include "src/generation/audition_stage7/stage7a_catalog.h"
#include "src/generation/audition_stage7/stage7a_session.h"
#include "src/generation/rhythm/relationship_resolver.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

bool identityEqual(const PhraseRhythmIdentity& a,
                   const PhraseRhythmIdentity& b) {
  if (a.archetypeId != b.archetypeId || a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) return false;
  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    if (a.protectedSpaces[i].steps != b.protectedSpaces[i].steps ||
        a.protectedSpaces[i].affectedRoles != b.protectedSpaces[i].affectedRoles) return false;
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) return false;
    }
  }
  return true;
}

uint64_t signature(const RhythmPhrasePlan& plan) {
  uint64_t hash = 1469598103934665603ull;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    const RoleRhythmPlan& value = plan.bars[0].roles[role];
    hash ^= value.structural; hash *= 1099511628211ull;
    hash ^= value.secondary; hash *= 1099511628211ull;
    hash ^= value.ghosts; hash *= 1099511628211ull;
  }
  return hash;
}

const RhythmArchetype& archetypeFor(RhythmArchetypeId id) {
  const RhythmCatalogView& view = Stage7AAudition::catalog();
  for (uint16_t i = 0; i < view.archetypeCount; ++i) {
    if (view.archetypes[i].id == id) return view.archetypes[i];
  }
  assert(false && "missing Stage 7B archetype");
  return view.archetypes[0];
}

RhythmRealizationResult realize(const Stage7AAudition::Definition& definition,
                                uint32_t seed,
                                RealizationLevel level,
                                const PhraseRhythmIdentity* identity = nullptr) {
  RhythmRealizationRequest request{};
  request.catalog = &Stage7AAudition::catalog();
  request.archetypeId = definition.archetypeId;
  request.phraseBars = 1;
  request.level = level;
  request.generation.projectSeed = seed;
  request.generation.phraseOrdinal = 0;
  request.reuseIdentity = identity;
  return realizeRhythmPhrase(request);
}

void assertPlanLegal(const RhythmArchetype& archetype,
                     const RhythmRealizationResult& result) {
  assert(result.status != RealizationStatus::InvalidConstraintSet);
  assert(result.plan.barCount == 1);
  assert(result.plan.trajectoryId == kNoTrajectoryId);
  assert(result.plan.intent == TransformationIntent::Auto);
  assert(result.plan.bars[0].function == BarFunction::Statement);
  assert(planRespectsProtectedSpace(archetype, result.plan));
  assert(planRespectsLaneBounds(archetype, result.plan));
  assert(hardRelationshipsSatisfied(archetype, structuralOccupancy(result.plan)));
  const PhraseOccupancy occupancy = structuralOccupancy(result.plan);
  assert(occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::BassRhythm)] == 0);
  assert(occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::ChordRhythm)] == 0);
  assert(occupancy.roleMasks[0][static_cast<uint8_t>(RhythmRole::MelodicRhythm)] == 0);
}

bool synthEqual(const SynthPattern& a, const SynthPattern& b) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& x = a.steps[step];
    const SynthStep& y = b.steps[step];
    if (x.note != y.note || x.slide != y.slide || x.accent != y.accent ||
        x.ghost != y.ghost || x.velocity != y.velocity || x.timing != y.timing ||
        x.fx != y.fx || x.fxParam != y.fxParam || x.probability != y.probability) return false;
  }
  return true;
}

bool drumsEqual(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& x = a.voices[voice].steps[step];
      const DrumStep& y = b.voices[voice].steps[step];
      if (x.hit != y.hit || x.accent != y.accent || x.velocity != y.velocity ||
          x.timing != y.timing || x.fx != y.fx || x.fxParam != y.fxParam ||
          x.probability != y.probability) return false;
    }
  }
  return a.groove.swing == b.groove.swing && a.groove.humanize == b.groove.humanize;
}

void testCatalogAndSeedCorpus() {
  assert(Stage7AAudition::definitionCount() == 4);
  assert(validateRhythmCatalog(Stage7AAudition::catalog()));
  for (uint8_t index = 0; index < Stage7AAudition::definitionCount(); ++index) {
    const auto& definition = Stage7AAudition::definition(index);
    const RhythmArchetype& archetype = archetypeFor(definition.archetypeId);
    assert(definition.archetypeId >= 711 && definition.archetypeId <= 714);
    assert(definition.evidence == Stage7AAudition::EvidenceClass::SingleRootChallenger);
    assert(archetype.allowedPhraseBars == phraseBarsBit(1));

    std::map<uint64_t, uint32_t> buckets;
    uint32_t p2Changed = 0;
    uint32_t p3Changed = 0;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
      const RhythmRealizationResult p1 = realize(definition, seed, RealizationLevel::P1Canonical);
      assertPlanLegal(archetype, p1);
      const uint64_t p1Signature = signature(p1.plan);
      buckets[p1Signature] += 1;

      const RhythmRealizationResult p2 = realize(definition, seed, RealizationLevel::P2Variation, &p1.identity);
      assertPlanLegal(archetype, p2);
      assert(identityEqual(p1.identity, p2.identity));
      if (signature(p2.plan) != p1Signature) ++p2Changed;

      const RhythmRealizationResult p3 = realize(definition, seed, RealizationLevel::P3Transformation, &p1.identity);
      assertPlanLegal(archetype, p3);
      assert(identityEqual(p1.identity, p3.identity));
      if (signature(p3.plan) != p1Signature) ++p3Changed;

      const RhythmRealizationResult repeat = realize(definition, seed, RealizationLevel::P1Canonical);
      assert(signature(repeat.plan) == p1Signature);
      assert(identityEqual(repeat.identity, p1.identity));
    }

    uint32_t maxBucket = 0;
    for (const auto& item : buckets) maxBucket = std::max(maxBucket, item.second);
    std::fprintf(stderr,
                 "STAGE7B_VARIATION %s distinct=%zu max_bucket=%u ratio=%.6f p2_changed=%u p3_changed=%u evidence=%s\n",
                 definition.name, buckets.size(), static_cast<unsigned>(maxBucket),
                 static_cast<double>(maxBucket) / 64.0,
                 static_cast<unsigned>(p2Changed), static_cast<unsigned>(p3Changed),
                 Stage7AAudition::evidenceName(definition.evidence));
    assert(buckets.size() >= 2);
    assert(p2Changed > 0);
    assert(p3Changed > 0);
  }
}

void testSessionIsTransactionalAndRestoresExactly() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  drums.voices[0].steps[1].hit = true;
  synthA.steps[2].note = 49;
  synthB.steps[5].note = 62;
  const DrumPatternSet originalDrums = drums;
  const SynthPattern originalA = synthA;
  const SynthPattern originalB = synthB;

  Stage7AAudition::Session session;
  assert(session.activate(drums, synthA, synthB));
  assert(session.selectCandidate(1, drums, synthA, synthB));
  const PhraseRhythmIdentity identityAtP1 = session.identity();
  assert(session.cycleLevel(drums, synthA, synthB));
  assert(identityEqual(identityAtP1, session.identity()));
  assert(session.cycleLevel(drums, synthA, synthB));
  assert(identityEqual(identityAtP1, session.identity()));
  assert(session.shiftSeed(3, drums, synthA, synthB));
  assert(session.seed() == 4);
  assert(session.rerender(drums, synthA, synthB));
  session.deactivate(drums, synthA, synthB);
  assert(drumsEqual(drums, originalDrums));
  assert(synthEqual(synthA, originalA));
  assert(synthEqual(synthB, originalB));
}

void testStatusIncludesEvidenceClass() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  Stage7AAudition::Session session;
  assert(session.activate(drums, synthA, synthB));
  char status[96]{};
  session.formatStatus(status, sizeof(status));
  assert(std::strstr(status, "stacked_quarters") != nullptr);
  assert(std::strstr(status, "S1") != nullptr);
  assert(std::strstr(status, "P1") != nullptr);
  assert(std::strstr(status, "CHAL") != nullptr);
}

}  // namespace

int main() {
  testCatalogAndSeedCorpus();
  testSessionIsTransactionalAndRestoresExactly();
  testStatusIncludesEvidenceClass();
  return 0;
}
