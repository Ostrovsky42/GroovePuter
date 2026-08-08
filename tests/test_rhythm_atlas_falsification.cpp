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
// Pattern Atlas v2.6 compiler. The test deliberately does NOT promote Atlas
// physical synth targets 8/9 into a global VoiceRole rule: Synth A/B != role.
// A small per-recipe projection represents the manual-curation boundary that
// the architecture requires between Atlas extraction and runtime vocabulary.

struct SynthRoleProjection {
  RhythmRole synthA;
  RhythmRole synthB;
};

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

uint8_t popcount16(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

struct ExtractedAtlasRhythm {
  StepMask masks[kRhythmRoleCount]{};
  uint8_t heldIntentCount = 0;
};

template <std::size_t N>
ExtractedAtlasRhythm extractRoleMasks(
    const AtlasGenerated::Event (&events)[N],
    const SynthRoleProjection& projection) {
  ExtractedAtlasRhythm out{};
  for (const AtlasGenerated::Event& event : events) {
    assert(event.step < kStepsPerBar);
    RhythmRole role{};
    assert(rhythmRoleForAtlasTarget(event.target, projection, role));
    assert(static_cast<uint8_t>(role) < kRhythmRoleCount);
    const uint8_t index = static_cast<uint8_t>(role);
    out.masks[index] = static_cast<StepMask>(
        out.masks[index] | stepBit(event.step));

    if ((event.flags & AtlasGenerated::kSustain) != 0) {
      RhythmEventIntent intent{};
      intent.step = event.step;
      intent.gate = GateClass::Held;
      intent.importance = EventImportance::Structural;
      assert(intent.gate == GateClass::Held);
      ++out.heldIntentCount;
    }
  }
  return out;
}

struct EncodedAtlasPattern {
  BarTrajectory trajectory{};
  LaneGrammar lanes[kRhythmRoleCount]{};
  LaneRelationship relationship{};
  TrajectoryRef trajectoryRef{};
  RhythmArchetype archetype{};
  RhythmCatalogView catalog{};
  PhraseRhythmIdentity identity{};
};

void encodeObservedMasks(EncodedAtlasPattern& out,
                         RhythmArchetypeId archetypeId,
                         RhythmFamily family,
                         const StepMask (&observed)[kRhythmRoleCount],
                         const LaneRelationship& relationship) {
  out.trajectory.id = 1;
  out.trajectory.barCount = 1;
  out.trajectory.bars[0] = BarFunction::Statement;

  uint8_t laneCount = 0;
  uint16_t totalStructural = 0;
  RhythmRoleMask activeRoles = 0;
  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const StepMask mask = observed[roleIndex];
    if (!mask) continue;
    LaneGrammar& lane = out.lanes[laneCount++];
    lane.role = static_cast<RhythmRole>(roleIndex);
    lane.canonicalAnchors = mask;
    lane.structuralMin = popcount16(mask);
    lane.structuralMax = lane.structuralMin;
    totalStructural += lane.structuralMin;
    activeRoles = static_cast<RhythmRoleMask>(
        activeRoles | rhythmRoleBit(lane.role));
  }

  out.relationship = relationship;
  out.trajectoryRef.id = out.trajectory.id;
  out.trajectoryRef.weight = 100;
  out.trajectoryRef.allowedLevels = kAllRealizationLevels;

  out.archetype.id = archetypeId;
  out.archetype.family = family;
  out.archetype.allowedPhraseBars = phraseBarsBit(1);
  out.archetype.activeRoles = activeRoles;
  out.archetype.lanes = out.lanes;
  out.archetype.laneCount = laneCount;
  out.archetype.relationships = &out.relationship;
  out.archetype.relationshipCount = 1;
  out.archetype.trajectories = &out.trajectoryRef;
  out.archetype.trajectoryCount = 1;
  out.archetype.density = DensityContract{
      static_cast<uint8_t>(totalStructural),
      static_cast<uint8_t>(totalStructural),
      static_cast<uint8_t>(totalStructural),
      0};

  out.catalog.archetypes = &out.archetype;
  out.catalog.archetypeCount = 1;
  out.catalog.trajectories = &out.trajectory;
  out.catalog.trajectoryCount = 1;

  out.identity.archetypeId = archetypeId;
  out.identity.phraseBars = 1;
  out.identity.trajectoryId = out.trajectory.id;
  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    out.identity.structuralCore[0][roleIndex] = observed[roleIndex];
    out.identity.canonicalCore[0][roleIndex] = observed[roleIndex];
  }
}

void assertMasks(const StepMask (&actual)[kRhythmRoleCount],
                 StepMask kick,
                 StepMask backbeat,
                 StepMask closedHat,
                 StepMask openHat,
                 StepMask percussion,
                 StepMask bass,
                 StepMask chord,
                 StepMask melodic) {
  assert(actual[static_cast<uint8_t>(RhythmRole::Kick)] == kick);
  assert(actual[static_cast<uint8_t>(RhythmRole::Backbeat)] == backbeat);
  assert(actual[static_cast<uint8_t>(RhythmRole::ClosedHat)] == closedHat);
  assert(actual[static_cast<uint8_t>(RhythmRole::OpenHat)] == openHat);
  assert(actual[static_cast<uint8_t>(RhythmRole::Percussion)] == percussion);
  assert(actual[static_cast<uint8_t>(RhythmRole::BassRhythm)] == bass);
  assert(actual[static_cast<uint8_t>(RhythmRole::ChordRhythm)] == chord);
  assert(actual[static_cast<uint8_t>(RhythmRole::MelodicRhythm)] == melodic);
}

void testRollingAcidP1IsRepresentable() {
  // Manual curation: current Synth A material is the acid/bass line; Synth B
  // is treated as melodic support here. This is fixture knowledge, not a
  // global mapping from physical track to role.
  const SynthRoleProjection projection{
      RhythmRole::BassRhythm, RhythmRole::MelodicRhythm};
  const ExtractedAtlasRhythm extracted = extractRoleMasks(
      AtlasGenerated::kEvents_PAT_ED_ACID_ROLLING_P1, projection);
  assert(extracted.heldIntentCount == 0);
  assertMasks(extracted.masks, 0x8888, 0x0808, 0x2222, 0x0202,
              0x1111, 0xF7F7, 0x0000, 0x2323);

  LaneRelationship relation{};
  relation.source = RhythmRole::Kick;
  relation.target = RhythmRole::ClosedHat;
  relation.op = RelationshipOp::Offset;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 2;

  EncodedAtlasPattern encoded{};
  encodeObservedMasks(encoded, 101, RhythmFamily::FourFloor,
                      extracted.masks, relation);
  assert(validateRhythmCatalog(encoded.catalog));
}

void testClassicTwoStepP1IsRepresentable() {
  const SynthRoleProjection projection{
      RhythmRole::BassRhythm, RhythmRole::MelodicRhythm};
  const ExtractedAtlasRhythm extracted = extractRoleMasks(
      AtlasGenerated::kEvents_PAT_ED_UKG_CLASSIC_2STEP_P1, projection);
  assert(extracted.heldIntentCount == 3);
  assertMasks(extracted.masks, 0x8220, 0x0808, 0x2525, 0x0101,
              0x1456, 0x8442, 0x0000, 0x3232);

  LaneRelationship relation{};
  relation.source = RhythmRole::Backbeat;
  relation.target = RhythmRole::Kick;
  relation.op = RelationshipOp::Exclude;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;

  EncodedAtlasPattern encoded{};
  encodeObservedMasks(encoded, 102, RhythmFamily::UkTwoStep,
                      extracted.masks, relation);
  assert(validateRhythmCatalog(encoded.catalog));
}

void testDeepChordP1IsRepresentable() {
  // The same physical Synth B target is curated differently here because the
  // recipe's musical function is chordal. This is exactly the separation the
  // Stage 1 contract must preserve.
  const SynthRoleProjection projection{
      RhythmRole::BassRhythm, RhythmRole::ChordRhythm};
  const ExtractedAtlasRhythm extracted = extractRoleMasks(
      AtlasGenerated::kEvents_PAT_ED_DUB_DEEP_CHORD_P1, projection);
  assert(extracted.heldIntentCount == 4);
  assertMasks(extracted.masks, 0x8888, 0x0808, 0x2222, 0x0202,
              0x1111, 0x9090, 0x2323, 0x0000);

  LaneRelationship relation{};
  relation.source = RhythmRole::Kick;
  relation.target = RhythmRole::ChordRhythm;
  relation.op = RelationshipOp::Respond;
  relation.strength = ConstraintStrength::Soft;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 3;
  relation.weight = 60;

  EncodedAtlasPattern encoded{};
  encodeObservedMasks(encoded, 103, RhythmFamily::DubPulse,
                      extracted.masks, relation);
  assert(validateRhythmCatalog(encoded.catalog));
}

}  // namespace

int main() {
  testRollingAcidP1IsRepresentable();
  testClassicTwoStepP1IsRepresentable();
  testDeepChordP1IsRepresentable();
  return 0;
}
