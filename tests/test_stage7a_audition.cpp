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
  if (a.archetypeId != b.archetypeId ||
      a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) {
    return false;
  }
  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    if (a.protectedSpaces[i].steps != b.protectedSpaces[i].steps ||
        a.protectedSpaces[i].affectedRoles != b.protectedSpaces[i].affectedRoles) {
      return false;
    }
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  return true;
}

uint64_t signature(const RhythmPhrasePlan& plan) {
  uint64_t hash = 1469598103934665603ull;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    const RoleRhythmPlan& value = plan.bars[0].roles[role];
    hash ^= value.structural;
    hash *= 1099511628211ull;
    hash ^= value.secondary;
    hash *= 1099511628211ull;
    hash ^= value.ghosts;
    hash *= 1099511628211ull;
  }
  return hash;
}

const RhythmArchetype& archetypeFor(RhythmArchetypeId id) {
  const RhythmCatalogView& view = Stage7AAudition::catalog();
  for (uint16_t i = 0; i < view.archetypeCount; ++i) {
    if (view.archetypes[i].id == id) return view.archetypes[i];
  }
  assert(false && "missing Stage 7A archetype");
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
        x.ghost != y.ghost || x.velocity != y.velocity ||
        x.timing != y.timing || x.fx != y.fx || x.fxParam != y.fxParam ||
        x.probability != y.probability) {
      return false;
    }
  }
  return true;
}

bool synthEmpty(const SynthPattern& pattern) {
  const SynthPattern empty{};
  return synthEqual(pattern, empty);
}

bool drumsEqual(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& x = a.voices[voice].steps[step];
      const DrumStep& y = b.voices[voice].steps[step];
      if (x.hit != y.hit || x.accent != y.accent ||
          x.velocity != y.velocity || x.timing != y.timing ||
          x.fx != y.fx || x.fxParam != y.fxParam ||
          x.probability != y.probability) {
        return false;
      }
    }
  }
  return a.groove.swing == b.groove.swing &&
         a.groove.humanize == b.groove.humanize;
}

void seedOriginals(DrumPatternSet& drums,
                   SynthPattern& synthA,
                   SynthPattern& synthB) {
  drums.voices[0].steps[1].hit = true;
  drums.voices[0].steps[1].velocity = 77;
  drums.voices[1].steps[9].hit = true;
  drums.groove.swing = 0.61f;
  drums.groove.humanize = 0.22f;
  synthA.steps[2].note = 49;
  synthA.steps[2].slide = true;
  synthA.steps[2].velocity = 91;
  synthB.steps[5].note = 62;
  synthB.steps[5].accent = true;
  synthB.steps[5].velocity = 103;
}

void testCatalogAndSeedCorpus() {
  assert(Stage7AAudition::definitionCount() == 5);
  assert(validateRhythmCatalog(Stage7AAudition::catalog()));

  uint8_t evidenceCount = 0;
  uint8_t challengerCount = 0;
  uint8_t controlCount = 0;

  for (uint8_t index = 0; index < Stage7AAudition::definitionCount(); ++index) {
    const auto& definition = Stage7AAudition::definition(index);
    const RhythmArchetype& archetype = archetypeFor(definition.archetypeId);
    assert(definition.archetypeId >= 701 && definition.archetypeId <= 705);
    assert(archetype.allowedPhraseBars == phraseBarsBit(1));
    assert(archetype.trajectoryCount == 1);
    assert(archetype.trajectories[0].id == 1);

    if (definition.evidence == Stage7AAudition::EvidenceClass::MultiProvenanceReview) ++evidenceCount;
    if (definition.evidence == Stage7AAudition::EvidenceClass::SingleRootChallenger) ++challengerCount;
    if (definition.evidence == Stage7AAudition::EvidenceClass::SingleRootControl) ++controlCount;

    std::map<uint64_t, uint32_t> buckets;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
      const RhythmRealizationResult p1 = realize(
          definition, seed, RealizationLevel::P1Canonical);
      assertPlanLegal(archetype, p1);
      buckets[signature(p1.plan)] += 1;

      const RhythmRealizationResult p2 = realize(
          definition, seed, RealizationLevel::P2Variation, &p1.identity);
      assertPlanLegal(archetype, p2);
      assert(identityEqual(p1.identity, p2.identity));

      const RhythmRealizationResult p3 = realize(
          definition, seed, RealizationLevel::P3Transformation, &p1.identity);
      assertPlanLegal(archetype, p3);
      assert(identityEqual(p1.identity, p3.identity));

      const RhythmRealizationResult repeat = realize(
          definition, seed, RealizationLevel::P1Canonical);
      assert(signature(repeat.plan) == signature(p1.plan));
      assert(identityEqual(repeat.identity, p1.identity));
    }

    uint32_t maxBucket = 0;
    for (const auto& item : buckets) maxBucket = std::max(maxBucket, item.second);
    const double maxBucketRatio = static_cast<double>(maxBucket) / 64.0;
    std::fprintf(stderr,
                 "STAGE7A_VARIATION %s distinct=%zu max_bucket=%u ratio=%.6f evidence=%s\n",
                 definition.name,
                 buckets.size(),
                 static_cast<unsigned>(maxBucket),
                 maxBucketRatio,
                 Stage7AAudition::evidenceName(definition.evidence));
    std::fflush(stderr);

    // Evidence candidates and challengers must already demonstrate more than
    // one generated P1 idea before hardware listening. The explicit control is
    // allowed to collapse: SAME-AS-EXISTING / one fingerprint is a successful
    // positive-control falsification, not a reason to weaken the real gates.
    if (definition.evidence != Stage7AAudition::EvidenceClass::SingleRootControl) {
      assert(buckets.size() >= 2);
    }
  }

  assert(evidenceCount == 2);
  assert(challengerCount == 2);
  assert(controlCount == 1);
}

void testSessionIsTransactionalAndRestoresExactly() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  seedOriginals(drums, synthA, synthB);
  const DrumPatternSet originalDrums = drums;
  const SynthPattern originalA = synthA;
  const SynthPattern originalB = synthB;

  Stage7AAudition::Session session;
  assert(session.activate(drums, synthA, synthB));
  assert(session.active());
  assert(!drumsEqual(drums, originalDrums));
  assert(synthEmpty(synthA));
  assert(synthEmpty(synthB));

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
  assert(!session.active());
  assert(drumsEqual(drums, originalDrums));
  assert(synthEqual(synthA, originalA));
  assert(synthEqual(synthB, originalB));
}

void testInactiveCommandsAreRejected() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  Stage7AAudition::Session session;
  assert(!session.selectCandidate(1, drums, synthA, synthB));
  assert(!session.shiftSeed(1, drums, synthA, synthB));
  assert(!session.cycleLevel(drums, synthA, synthB));
  assert(!session.rerender(drums, synthA, synthB));
}

void testStatusIncludesEvidenceClass() {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  Stage7AAudition::Session session;
  assert(session.activate(drums, synthA, synthB));
  char status[96]{};
  session.formatStatus(status, sizeof(status));
  assert(std::strstr(status, "S7A staggered_machine") != nullptr);
  assert(std::strstr(status, "S1") != nullptr);
  assert(std::strstr(status, "P1") != nullptr);
  assert(std::strstr(status, "EVID") != nullptr);
}

}  // namespace

int main() {
  testCatalogAndSeedCorpus();
  testSessionIsTransactionalAndRestoresExactly();
  testInactiveCommandsAreRejected();
  testStatusIncludesEvidenceClass();
  return 0;
}
