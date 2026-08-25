#include "e3_listen_review_hook.h"

#include "e3_listen_fixture_generated.h"
#include "../rhythm/reference_vocabulary.h"

namespace GroovePuterRhythm {
namespace {

struct ReviewState {
  bool active = false;
  uint8_t caseIndex = 0;
  E3ListenVariant variant = E3ListenVariant::Canonical;
};

ReviewState g_review{};

const E3ListenFixtureData::PlanMasks* selectedMasks() {
  if (!g_review.active ||
      g_review.caseIndex >= E3ListenFixtureData::kCaseCount) {
    return nullptr;
  }
  switch (g_review.variant) {
    case E3ListenVariant::Canonical:
      return &E3ListenFixtureData::kCanonical[g_review.caseIndex];
    case E3ListenVariant::Before:
      return &E3ListenFixtureData::kBefore[g_review.caseIndex];
    case E3ListenVariant::After:
      return &E3ListenFixtureData::kAfter[g_review.caseIndex];
    case E3ListenVariant::Count:
      return nullptr;
  }
  return nullptr;
}

ReferenceVocabulary::Archetype reviewArchetype(uint8_t familyIndex) {
  switch (familyIndex) {
    case 0: return ReferenceVocabulary::Archetype::StraightDrive;
    case 1: return ReferenceVocabulary::Archetype::OffbeatOpenHat;
    case 2: return ReferenceVocabulary::Archetype::SparseFastBreak;
    case 3: return ReferenceVocabulary::Archetype::HalftimeSwitch;
    case 4: return ReferenceVocabulary::Archetype::HypnoticSparse;
    case 5: return ReferenceVocabulary::Archetype::RollingAcid;
    default: return ReferenceVocabulary::Archetype::Count;
  }
}

StepMask allOnsets(const E3ListenFixtureData::RolePlanMasks& role) {
  return static_cast<StepMask>(
      role.structural | role.secondary | role.ghosts);
}

}  // namespace

void configureE3ListenReview(uint8_t caseIndex, E3ListenVariant variant) {
  g_review.active =
      caseIndex < E3ListenFixtureData::kCaseCount &&
      static_cast<uint8_t>(variant) <
          static_cast<uint8_t>(E3ListenVariant::Count);
  g_review.caseIndex = caseIndex;
  g_review.variant = variant;
}

void disableE3ListenReview() {
  g_review = ReviewState{};
}

bool e3ListenReviewActive() {
  return g_review.active;
}

void e3ListenOverrideComposition(GenerationCompositionResult& composition) {
  if (!g_review.active ||
      g_review.caseIndex >= E3ListenFixtureData::kCaseCount) {
    return;
  }

  const auto& meta = E3ListenFixtureData::kCases[g_review.caseIndex];
  const ReferenceVocabulary::Archetype key =
      reviewArchetype(meta.familyIndex);
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(key);
  if (definition == nullptr) {
    composition.status = GenerationCompositionStatus::NoCompatibleRhythm;
    return;
  }

  // The review context is intentionally production-owned downstream. We pin
  // only the frozen E3R-B archetype family and ask existing role owners to
  // realize their normal AUTO context around the injected rhythm material.
  composition.rhythmSelectionMode = RhythmSelectionMode::Manual;
  composition.rhythmArchetypeId = definition->archetypeId;
  composition.normalizedRhythmToAuto = false;
  composition.bassRhythm = BassRhythmId::Auto;
  composition.chordRhythm = ChordRhythmId::Auto;
  composition.progression = ProgressionId::Auto;
  composition.melodicRhythm = MelodicRhythmId::Auto;
  composition.motifShape = MotifShapeId::Auto;
  composition.secondaryRole = CompositionSecondaryRole::Melodic;
}

void e3ListenOverrideRhythmPlan(RhythmPhrasePlan& plan) {
  const auto* masks = selectedMasks();
  if (masks == nullptr) return;

  const auto& meta = E3ListenFixtureData::kCases[g_review.caseIndex];
  RhythmPhrasePlan next{};
  next.barCount = 1;
  next.trajectoryId = kNoTrajectoryId;
  next.level = static_cast<RealizationLevel>(meta.level);
  next.intent = TransformationIntent::Auto;
  next.bars[0].function = BarFunction::Statement;

  for (uint8_t roleIndex = 0; roleIndex < kRhythmRoleCount; ++roleIndex) {
    const auto& source = masks->roles[roleIndex];
    RoleRhythmPlan& target = next.bars[0].roles[roleIndex];
    target.structural = source.structural;
    target.secondary = source.secondary;
    target.ghosts = source.ghosts;
    target.shortGate = source.shortGate;
    target.heldGate = source.heldGate;
    target.tieGate = source.tieGate;
    target.accents = source.accents;
  }
  plan = next;
}

void e3ListenOverrideBassPlan(BassRhythmPlan& plan) {
  const auto* masks = selectedMasks();
  if (masks == nullptr) return;

  const auto& source =
      masks->roles[static_cast<uint8_t>(RhythmRole::BassRhythm)];

  // IMPORTANT AUDIBILITY BOUNDARY:
  // RhythmPhrasePlan's BassRhythm role has onset importance/gate masks, while
  // BassRhythmPlan has only onset/continuation topology plus its production id
  // and kick relationship. There is no frozen production contract converting
  // those gate masks into BassRhythmPlan continuations.
  //
  // For E3L only, inject the exact frozen onset timing at the narrowest
  // existing production boundary and deliberately use no invented
  // continuation semantics. Pitch behavior, progression, register, scale,
  // tonal materialization, Synth A adaptation and Feel remain production
  // owners. For this reason every E3L case is labelled
  // PRODUCTION_CONTEXT_AUDITION rather than byte/plan-exact playback.
  plan.onsets = allOnsets(source);
  plan.continuations = 0;
}

}  // namespace GroovePuterRhythm
