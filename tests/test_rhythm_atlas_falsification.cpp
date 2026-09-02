#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/generation/rhythm/rhythm_catalog.h"
#include "src/generated/rec_acid_rolling.generated.h"
#include "src/generated/rec_dub_deep_chord.generated.h"
#include "src/generated/rec_ukg_classic_2step.generated.h"

using namespace GroovePuterRhythm;

namespace {

// These fixtures are generated runtime outputs from the hash-gated SEQTRAK
// Pattern Atlas v2.6 compiler. The purpose is model falsification, not preset
// import: each reviewed Atlas pattern must be a legal member of a manually
// curated grammar that is broader than that one concrete bitmap.
//
// Physical Atlas targets 8/9 are never promoted into a global VoiceRole rule.
// The per-recipe projection below is the explicit manual-curation boundary:
// Synth A/B != musical role.
struct SynthRoleProjection {
  RhythmRole synthA;
  RhythmRole synthB;
};

struct ExtractedAtlasRhythm {
  StepMask masks[kRhythmRoleCount]{};
  StepMask heldMasks[kRhythmRoleCount]{};
  uint8_t heldIntentCount = 0;
};

struct GrammarFixture {
  BarTrajectory trajectory{};
  LaneGrammar lanes[kRhythmRoleCount]{};
  ProtectedSpace protectedSpaces[2]{};
  LaneRelationship relationships[2]{};
  TrajectoryRef trajectoryRef{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};
  PhraseRhythmIdentity identity{};
  uint8_t laneCount = 0;
  uint16_t aggregateMin = 0;
  uint16_t aggregateMax = 0;
};

uint8_t popcount16(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

bool rhythmRoleForAtlasTarget(uint8_t target,
                              const SynthRoleProjection& projection,
                              RhythmRole& role) {
  switch (target) {
    case 0: role = RhythmRole::Kick; return true;
    case 1:
    case 7: role = RhythmRole::Backbeat; return true;
    case 2: role = RhythmRole::ClosedHat; return true;
    case 3: role = RhythmRole::OpenHat; return true;
    case 4:
    case 5:
    case 6: role = RhythmRole::Percussion; return true;
    case 8: role = projection.synthA; return true;
    case 9: role = projection.synthB; return true;
    default: return false;
  }
}

template <std::size_t N>
ExtractedAtlasRhythm extractRoleMasks(
    const AtlasGenerated::Event (&events)[N],
    const SynthRoleProjection& projection) {
  ExtractedAtlasRhythm out{};
  for (const AtlasGenerated::Event& event : events) {
    assert(event.step < kStepsPerBar);
    RhythmRole role{};
    assert(rhythmRoleForAtlasTarget(event.target, projection, role));
    const uint8_t roleIndex = static_cast<uint8_t>(role);
    assert(roleIndex < kRhythmRoleCount);
    out.masks[roleIndex] = static_cast<StepMask>(
        out.masks[roleIndex] | stepBit(event.step));

    if ((event.flags & AtlasGenerated::kSustain) != 0) {
      RhythmEventIntent intent{};
      intent.step = event.step;
      intent.gate = GateClass::Held;
      intent.importance = EventImportance::Structural;
      assert(intent.gate == GateClass::Held);
      out.heldMasks[roleIndex] = static_cast<StepMask>(
          out.heldMasks[roleIndex] | stepBit(event.step));
      ++out.heldIntentCount;
    }
  }
  return out;
}

void beginGrammar(GrammarFixture& fixture,
                  RhythmArchetypeId id,
                  RhythmFamily family) {
  fixture.trajectory.id = 1;
  fixture.trajectory.barCount = 1;
  fixture.trajectory.bars[0] = BarFunction::Statement;
  fixture.trajectoryRef.id = fixture.trajectory.id;
  fixture.trajectoryRef.weight = 100;
  fixture.trajectoryRef.allowedLevels = kAllRealizationLevels;

  fixture.archetype.id = id;
  fixture.archetype.family = family;
  fixture.archetype.allowedPhraseBars = phraseBarsBit(1);
  fixture.archetype.trajectories = &fixture.trajectoryRef;
  fixture.archetype.trajectoryCount = 1;

  fixture.catalog.archetypes = &fixture.archetype;
  fixture.catalog.archetypeCount = 1;
  fixture.catalog.trajectories = &fixture.trajectory;
  fixture.catalog.trajectoryCount = 1;

  fixture.identity.archetypeId = id;
  fixture.identity.phraseBars = 1;
  fixture.identity.trajectoryId = fixture.trajectory.id;
}

void addLane(GrammarFixture& fixture,
             RhythmRole role,
             StepMask canonical,
             StepMask preferred,
             StepMask optional,
             uint8_t structuralMin,
             uint8_t structuralMax,
             StepMask heldGate = 0) {
  assert(fixture.laneCount < kRhythmRoleCount);
  LaneGrammar& lane = fixture.lanes[fixture.laneCount++];
  lane.role = role;
  lane.canonicalAnchors = canonical;
  lane.preferred = preferred;
  lane.optional = optional;
  lane.structuralMin = structuralMin;
  lane.structuralMax = structuralMax;
  lane.heldGate = heldGate;

  fixture.archetype.activeRoles = static_cast<RhythmRoleMask>(
      fixture.archetype.activeRoles | rhythmRoleBit(role));
  fixture.aggregateMin += structuralMin;
  fixture.aggregateMax += structuralMax;

  const uint8_t roleIndex = static_cast<uint8_t>(role);
  fixture.identity.structuralCore[0][roleIndex] = canonical;
  fixture.identity.canonicalCore[0][roleIndex] = canonical;
}

void addProtectedSpace(GrammarFixture& fixture,
                       StepMask steps,
                       RhythmRoleMask affectedRoles) {
  assert(fixture.archetype.protectedSpaceCount < 2);
  ProtectedSpace& space =
      fixture.protectedSpaces[fixture.archetype.protectedSpaceCount++];
  space.steps = steps;
  space.affectedRoles = affectedRoles;
  fixture.archetype.protectedSpaces = fixture.protectedSpaces;
}

void addRelationship(GrammarFixture& fixture,
                     const LaneRelationship& relationship) {
  assert(fixture.archetype.relationshipCount < 2);
  fixture.relationships[fixture.archetype.relationshipCount++] = relationship;
  fixture.archetype.relationships = fixture.relationships;
}

void finishGrammar(GrammarFixture& fixture) {
  fixture.archetype.lanes = fixture.lanes;
  fixture.archetype.laneCount = fixture.laneCount;
  fixture.archetype.density = DensityContract{
      static_cast<uint8_t>(fixture.aggregateMin),
      static_cast<uint8_t>(fixture.aggregateMin),
      static_cast<uint8_t>(fixture.aggregateMax),
      8};
}

const LaneGrammar& laneFor(const GrammarFixture& fixture, RhythmRole role) {
  for (uint8_t i = 0; i < fixture.laneCount; ++i) {
    if (fixture.lanes[i].role == role) return fixture.lanes[i];
  }
  assert(false && "missing curated lane");
  return fixture.lanes[0];
}

StepMask protectedFor(const GrammarFixture& fixture, RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask bit = rhythmRoleBit(role);
  for (uint8_t i = 0; i < fixture.archetype.protectedSpaceCount; ++i) {
    if (fixture.protectedSpaces[i].affectedRoles & bit) {
      result = static_cast<StepMask>(
          result | fixture.protectedSpaces[i].steps);
    }
  }
  return result;
}

void assertObservedFitsLane(const GrammarFixture& fixture,
                            RhythmRole role,
                            StepMask observed) {
  const LaneGrammar& lane = laneFor(fixture, role);
  const StepMask anchors = static_cast<StepMask>(
      lane.immutableAnchors | lane.canonicalAnchors);
  const StepMask legal = static_cast<StepMask>(
      anchors | lane.preferred | lane.optional);
  assert((anchors & ~observed) == 0);
  assert((observed & ~legal) == 0);
  assert((observed & lane.forbidden) == 0);
  assert((observed & protectedFor(fixture, role)) == 0);
  const uint8_t count = popcount16(observed);
  assert(count >= lane.structuralMin);
  assert(count <= lane.structuralMax);
}

bool hasSourceAtOffset(StepMask source,
                       uint8_t targetStep,
                       int8_t minOffset,
                       int8_t maxOffset) {
  for (int sourceStep = 0; sourceStep < kStepsPerBar; ++sourceStep) {
    if (!(source & stepBit(static_cast<uint8_t>(sourceStep)))) continue;
    const int delta = static_cast<int>(targetStep) - sourceStep;
    if (delta >= minOffset && delta <= maxOffset) return true;
  }
  return false;
}

void assertObservedSatisfiesRelationship(
    const ExtractedAtlasRhythm& extracted,
    const LaneRelationship& relation) {
  const StepMask source =
      extracted.masks[static_cast<uint8_t>(relation.source)];
  const StepMask target =
      extracted.masks[static_cast<uint8_t>(relation.target)];

  if (relation.op == RelationshipOp::Exclude) {
    assert((source & target & relation.zoneMask) == 0);
    return;
  }

  if (relation.op == RelationshipOp::Offset) {
    for (uint8_t targetStep = 0; targetStep < kStepsPerBar; ++targetStep) {
      if (!(target & relation.zoneMask & stepBit(targetStep))) continue;
      assert(hasSourceAtOffset(source,
                               targetStep,
                               relation.minOffset,
                               relation.maxOffset));
    }
    return;
  }

  if (relation.op == RelationshipOp::Respond) {
    for (uint8_t sourceStep = 0; sourceStep < kStepsPerBar; ++sourceStep) {
      if (!(source & relation.zoneMask & stepBit(sourceStep))) continue;
      uint8_t responses = 0;
      for (uint8_t targetStep = 0; targetStep < kStepsPerBar; ++targetStep) {
        if (!(target & stepBit(targetStep))) continue;
        const int delta = static_cast<int>(targetStep) - sourceStep;
        if (delta >= relation.minOffset && delta <= relation.maxOffset) {
          ++responses;
        }
      }
      assert(responses >= relation.minResponsesPerWindow);
      if (relation.maxResponsesPerWindow) {
        assert(responses <= relation.maxResponsesPerWindow);
      }
    }
    return;
  }

  assert(false && "fixture relationship predicate not implemented");
}

void assertObservedIsLegalMember(const GrammarFixture& fixture,
                                 const ExtractedAtlasRhythm& extracted) {
  assert(validateRhythmCatalog(fixture.catalog));

  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const RhythmRole role = static_cast<RhythmRole>(roleIndex);
    if (!(fixture.archetype.activeRoles & rhythmRoleBit(role))) {
      assert(extracted.masks[roleIndex] == 0);
      assert(extracted.heldMasks[roleIndex] == 0);
    }
  }

  uint8_t variantCapableLanes = 0;
  uint8_t observedNonCanonicalLanes = 0;
  for (uint8_t i = 0; i < fixture.laneCount; ++i) {
    const LaneGrammar& lane = fixture.lanes[i];
    const StepMask observed =
        extracted.masks[static_cast<uint8_t>(lane.role)];
    assertObservedFitsLane(fixture, lane.role, observed);
    const StepMask observedHeld =
        extracted.heldMasks[static_cast<uint8_t>(lane.role)];
    assert(observedHeld == static_cast<StepMask>(
        observed & lane.heldGate));
    assert((observed & lane.shortGate) == 0);
    assert((observed & lane.tieGate) == 0);

    if (lane.preferred || lane.optional ||
        lane.structuralMin != lane.structuralMax) {
      ++variantCapableLanes;
    }
    if (observed & static_cast<StepMask>(lane.preferred | lane.optional)) {
      ++observedNonCanonicalLanes;
    }
  }

  // Two separate anti-cheat checks: the grammar must contain actual variation
  // space, and the observed Atlas member must genuinely exercise non-canonical
  // decisions rather than being copied wholesale into canonicalAnchors.
  assert(variantCapableLanes >= 3);
  assert(observedNonCanonicalLanes >= 3);

  for (uint8_t i = 0; i < fixture.archetype.relationshipCount; ++i) {
    assertObservedSatisfiesRelationship(extracted,
                                        fixture.relationships[i]);
  }
}

void testRollingAcidP1FitsGeneralizedGrammar() {
  const SynthRoleProjection projection{
      RhythmRole::BassRhythm, RhythmRole::MelodicRhythm};
  const ExtractedAtlasRhythm extracted = extractRoleMasks(
      AtlasGenerated::kEvents_PAT_ED_ACID_ROLLING_P1, projection);
  assert(extracted.heldIntentCount == 0);

  GrammarFixture fixture{};
  beginGrammar(fixture, 101, RhythmFamily::FourFloor);
  addLane(fixture, RhythmRole::Kick, 0x8888, 0, 0, 4, 4);
  addLane(fixture, RhythmRole::Backbeat, 0x0808, 0, 0, 2, 2);
  addLane(fixture, RhythmRole::ClosedHat, 0x2020, 0x0202, 0, 2, 4);
  addLane(fixture, RhythmRole::OpenHat, 0, 0x0202, 0x0101, 0, 2);
  addLane(fixture, RhythmRole::Percussion, 0, 0x1111, 0x4444, 1, 4);
  addLane(fixture, RhythmRole::BassRhythm, 0x8080, 0x7777, 0x0808, 4, 14);
  addLane(fixture, RhythmRole::MelodicRhythm, 0x2020, 0x0303, 0x0808, 2, 6);

  LaneRelationship relation{};
  relation.source = RhythmRole::Kick;
  relation.target = RhythmRole::ClosedHat;
  relation.op = RelationshipOp::Offset;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 2;
  addRelationship(fixture, relation);
  finishGrammar(fixture);
  assertObservedIsLegalMember(fixture, extracted);
}

void testClassicTwoStepP1FitsGeneralizedGrammar() {
  const SynthRoleProjection projection{
      RhythmRole::BassRhythm, RhythmRole::MelodicRhythm};
  const ExtractedAtlasRhythm extracted = extractRoleMasks(
      AtlasGenerated::kEvents_PAT_ED_UKG_CLASSIC_2STEP_P1, projection);
  assert(extracted.heldIntentCount == 3);

  GrammarFixture fixture{};
  beginGrammar(fixture, 102, RhythmFamily::UkTwoStep);
  addLane(fixture, RhythmRole::Kick, 0x8000, 0x0220, 0x0040, 2, 4);
  addLane(fixture, RhythmRole::Backbeat, 0x0808, 0, 0, 2, 2);
  addLane(fixture, RhythmRole::ClosedHat, 0x2020, 0x0505, 0x0202, 2, 6);
  addLane(fixture, RhythmRole::OpenHat, 0, 0x0101, 0x0202, 0, 2);
  addLane(fixture, RhythmRole::Percussion, 0x1000, 0x0456, 0x2020, 2, 6);
  addLane(fixture, RhythmRole::BassRhythm, 0x8000, 0x0442, 0x2020,
          2, 5, 0x8440);
  addLane(fixture, RhythmRole::MelodicRhythm, 0x2020, 0x1212, 0x0404, 2, 6);

  addProtectedSpace(
      fixture,
      0x0808,
      static_cast<RhythmRoleMask>(
          rhythmRoleBit(RhythmRole::Kick) |
          rhythmRoleBit(RhythmRole::BassRhythm)));

  LaneRelationship relation{};
  relation.source = RhythmRole::Backbeat;
  relation.target = RhythmRole::Kick;
  relation.op = RelationshipOp::Exclude;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  addRelationship(fixture, relation);
  finishGrammar(fixture);
  assertObservedIsLegalMember(fixture, extracted);
}

void testDeepChordP1FitsGeneralizedGrammar() {
  const SynthRoleProjection projection{
      RhythmRole::BassRhythm, RhythmRole::ChordRhythm};
  const ExtractedAtlasRhythm extracted = extractRoleMasks(
      AtlasGenerated::kEvents_PAT_ED_DUB_DEEP_CHORD_P1, projection);
  assert(extracted.heldIntentCount == 4);

  GrammarFixture fixture{};
  beginGrammar(fixture, 103, RhythmFamily::DubPulse);
  addLane(fixture, RhythmRole::Kick, 0x8888, 0, 0, 4, 4);
  addLane(fixture, RhythmRole::Backbeat, 0x0808, 0, 0, 2, 2);
  addLane(fixture, RhythmRole::ClosedHat, 0x2020, 0x0202, 0x0101, 2, 5);
  addLane(fixture, RhythmRole::OpenHat, 0, 0x0202, 0x0101, 0, 2);
  addLane(fixture, RhythmRole::Percussion, 0, 0x1111, 0x2222, 1, 5);
  addLane(fixture, RhythmRole::BassRhythm, 0x8080, 0x1010, 0x0202,
          2, 4, 0x9090);
  addLane(fixture, RhythmRole::ChordRhythm, 0x2020, 0x0303, 0x0808, 2, 6);

  addProtectedSpace(
      fixture,
      0x4444,
      static_cast<RhythmRoleMask>(
          rhythmRoleBit(RhythmRole::Kick) |
          rhythmRoleBit(RhythmRole::BassRhythm) |
          rhythmRoleBit(RhythmRole::ChordRhythm)));

  LaneRelationship relation{};
  relation.source = RhythmRole::Kick;
  relation.target = RhythmRole::ChordRhythm;
  relation.op = RelationshipOp::Respond;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 3;
  relation.minResponsesPerWindow = 1;
  relation.maxResponsesPerWindow = 2;
  addRelationship(fixture, relation);
  finishGrammar(fixture);
  assertObservedIsLegalMember(fixture, extracted);
}

}  // namespace

int main() {
  testRollingAcidP1FitsGeneralizedGrammar();
  testClassicTwoStepP1FitsGeneralizedGrammar();
  testDeepChordP1FitsGeneralizedGrammar();
  return 0;
}
