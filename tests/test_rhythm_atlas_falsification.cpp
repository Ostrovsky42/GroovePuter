#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/generation/rhythm/rhythm_catalog.h"
#include "src/generated/rec_acid_rolling.generated.h"
#include "src/generated/rec_dub_deep_chord.generated.h"
#include "src/generated/rec_ukg_classic_2step.generated.h"

using namespace GroovePuterRhythm;

namespace {

// These fixtures are not hand-authored pattern presets. They are generated
// runtime outputs from the hash-gated SEQTRAK Pattern Atlas v2.6 compiler.
// tools/atlas/compile_atlas_runtime.py accepts only schema 2.6.0 with zero
// validation failures and source archive SHA-256
// 5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd.
//
// This test deliberately checks only semantics actually present in those
// generated Atlas events: role-level onset topology. GateClass support remains
// a Stage 1 type contract, but is not inferred from Atlas events unless the
// source data explicitly carries kSustain. Likewise accent/slide stay outside
// Rhythm Vocabulary topology ownership.

bool rhythmRoleForAtlasTarget(uint8_t target, RhythmRole& role) {
  switch (target) {
    case 0: role = RhythmRole::Kick; return true;
    case 1:
    case 7: role = RhythmRole::Backbeat; return true;
    case 2: role = RhythmRole::ClosedHat; return true;
    case 3: role = RhythmRole::OpenHat; return true;
    case 4:
    case 5:
    case 6: role = RhythmRole::Percussion; return true;
    case 8: role = RhythmRole::BassRhythm; return true;
    case 9: role = RhythmRole::ChordRhythm; return true;
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

template <std::size_t N>
void extractRoleMasks(const AtlasGenerated::Event (&events)[N],
                      StepMask (&masks)[kRhythmRoleCount]) {
  for (uint8_t i = 0; i < kRhythmRoleCount; ++i) masks[i] = 0;
  for (const AtlasGenerated::Event& event : events) {
    assert(event.step < kStepsPerBar);
    RhythmRole role{};
    assert(rhythmRoleForAtlasTarget(event.target, role));
    const uint8_t index = static_cast<uint8_t>(role);
    masks[index] = static_cast<StepMask>(masks[index] | stepBit(event.step));
  }
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
                 StepMask chord) {
  assert(actual[static_cast<uint8_t>(RhythmRole::Kick)] == kick);
  assert(actual[static_cast<uint8_t>(RhythmRole::Backbeat)] == backbeat);
  assert(actual[static_cast<uint8_t>(RhythmRole::ClosedHat)] == closedHat);
  assert(actual[static_cast<uint8_t>(RhythmRole::OpenHat)] == openHat);
  assert(actual[static_cast<uint8_t>(RhythmRole::Percussion)] == percussion);
  assert(actual[static_cast<uint8_t>(RhythmRole::BassRhythm)] == bass);
  assert(actual[static_cast<uint8_t>(RhythmRole::ChordRhythm)] == chord);
  assert(actual[static_cast<uint8_t>(RhythmRole::MelodicRhythm)] == 0);
}

void testRollingAcidP1IsRepresentable() {
  StepMask observed[kRhythmRoleCount]{};
  extractRoleMasks(AtlasGenerated::kEvents_PAT_ED_ACID_ROLLING_P1, observed);
  assertMasks(observed, 0x8888, 0x0808, 0x2222, 0x0202,
              0x1111, 0xF7F7, 0x2323);

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
                      observed, relation);
  assert(validateRhythmCatalog(encoded.catalog));
}

void testClassicTwoStepP1IsRepresentable() {
  StepMask observed[kRhythmRoleCount]{};
  extractRoleMasks(AtlasGenerated::kEvents_PAT_ED_UKG_CLASSIC_2STEP_P1,
                   observed);
  assertMasks(observed, 0x8220, 0x0808, 0x2525, 0x0101,
              0x1456, 0x8442, 0x3232);

  LaneRelationship relation{};
  relation.source = RhythmRole::Backbeat;
  relation.target = RhythmRole::Kick;
  relation.op = RelationshipOp::Exclude;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;

  EncodedAtlasPattern encoded{};
  encodeObservedMasks(encoded, 102, RhythmFamily::UkTwoStep,
                      observed, relation);
  assert(validateRhythmCatalog(encoded.catalog));
}

void testDeepChordP1IsRepresentable() {
  StepMask observed[kRhythmRoleCount]{};
  extractRoleMasks(AtlasGenerated::kEvents_PAT_ED_DUB_DEEP_CHORD_P1, observed);
  assertMasks(observed, 0x8888, 0x0808, 0x2222, 0x0202,
              0x1111, 0x9090, 0x2323);

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
                      observed, relation);
  assert(validateRhythmCatalog(encoded.catalog));
}

}  // namespace

int main() {
  testRollingAcidP1IsRepresentable();
  testClassicTwoStepP1IsRepresentable();
  testDeepChordP1IsRepresentable();
  return 0;
}
