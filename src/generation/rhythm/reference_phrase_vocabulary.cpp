#include "reference_phrase_vocabulary.h"

namespace GroovePuterRhythm {
namespace ReferenceVocabulary {
namespace {

constexpr PhraseBarsMask kStage12PhraseBars = static_cast<PhraseBarsMask>(
    phraseBarsBit(1) | phraseBarsBit(2) | phraseBarsBit(4));

constexpr RealizationLevelMask kP2P3 = static_cast<RealizationLevelMask>(
    realizationLevelBit(RealizationLevel::P2Variation) |
    realizationLevelBit(RealizationLevel::P3Transformation));

constexpr MutationPolicy stage12SubtractiveMutationPolicy() {
  MutationPolicy policy{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P1Canonical)] =
      MutationBudget{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P2Variation)] =
      MutationBudget{
          2,
          1,
          0,
          0,
          static_cast<uint16_t>(AllowGhostConversion |
                                AllowPreferredDrops |
                                AllowReduction),
          transformationIntentBit(TransformationIntent::Reduce)};
  policy.level[static_cast<uint8_t>(RealizationLevel::P3Transformation)] =
      MutationBudget{
          3,
          3,
          0,
          0,
          static_cast<uint16_t>(AllowOptionalAdds |
                                AllowPreferredDrops |
                                AllowReduction |
                                AllowTurnaround |
                                AllowBreak),
          static_cast<TransformationIntentMask>(
              transformationIntentBit(TransformationIntent::Reduce) |
              transformationIntentBit(TransformationIntent::Turnaround) |
              transformationIntentBit(TransformationIntent::Break))};
  return policy;
}

constexpr MutationPolicy stage12NonSubtractiveMutationPolicy() {
  MutationPolicy policy{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P1Canonical)] =
      MutationBudget{};
  policy.level[static_cast<uint8_t>(RealizationLevel::P2Variation)] =
      MutationBudget{
          2,
          0,
          0,
          0,
          AllowGhostConversion,
          0};
  policy.level[static_cast<uint8_t>(RealizationLevel::P3Transformation)] =
      MutationBudget{
          3,
          0,
          0,
          0,
          static_cast<uint16_t>(AllowOptionalAdds | AllowTurnaround),
          transformationIntentBit(TransformationIntent::Turnaround)};
  return policy;
}

// These trajectories promote the already-tested Stage 12 fixture vocabulary.
// The only additional trajectory is id 8, which makes the existing bounded
// Break transform explicitly reachable at P3. Atlas Pass 2 measured both
// DROP_ONLY and MIXED two-bar transitions, with median drops of 2.5/3.0.
constexpr BarTrajectory kPhraseTrajectories[] = {
    {1, 1, {BarFunction::Statement,
            BarFunction::Statement,
            BarFunction::Statement,
            BarFunction::Statement}},
    {2, 2, {BarFunction::Statement,
            BarFunction::Repeat,
            BarFunction::Statement,
            BarFunction::Statement}},
    {3, 2, {BarFunction::Statement,
            BarFunction::RepeatWithGhosts,
            BarFunction::Statement,
            BarFunction::Statement}},
    {5, 4, {BarFunction::Statement,
            BarFunction::Response,
            BarFunction::Repeat,
            BarFunction::Return}},
    {6, 4, {BarFunction::Statement,
            BarFunction::Repeat,
            BarFunction::Reduction,
            BarFunction::Return}},
    {7, 4, {BarFunction::Statement,
            BarFunction::Build,
            BarFunction::RepeatWithGhosts,
            BarFunction::Turnaround}},
    {8, 4, {BarFunction::Statement,
            BarFunction::RepeatWithGhosts,
            BarFunction::Break,
            BarFunction::Return}},
};

constexpr TrajectoryRef kSubtractivePhraseTrajectoryRefs[] = {
    {1, 100, kAllRealizationLevels},
    {2, 70, kAllRealizationLevels},
    {3, 30, kP2P3},
    {5, 70, kAllRealizationLevels},
    {6, 30, kP2P3},
    {7, 20, realizationLevelBit(RealizationLevel::P3Transformation)},
    {8, 20, realizationLevelBit(RealizationLevel::P3Transformation)},
};

// halftime_switch has no removable headroom at its current structural minima.
// Keep its accepted P1 identity intact and expose only non-subtractive phrase
// functions until an evidence-backed archetype revision can create real drop
// headroom without weakening the canonical groove.
constexpr TrajectoryRef kNonSubtractivePhraseTrajectoryRefs[] = {
    {1, 100, kAllRealizationLevels},
    {2, 70, kAllRealizationLevels},
    {3, 30, kP2P3},
    {5, 70, kAllRealizationLevels},
    {7, 20, realizationLevelBit(RealizationLevel::P3Transformation)},
};

bool stage12PhraseEnabledId(RhythmArchetypeId id) {
  switch (id) {
    case 404:  // broken_techno
    case 420:  // machine_syncopation
    case 712:  // electro_backskip
    case 714:  // electro_gap_push
    case 413:  // two_step_roll
    case 414:  // ghosted_roll
    case 415:  // sparse_fast_break
    case 416:  // halftime_switch
    case 417:  // classic_2step
    case 418:  // skippy_2step
      return true;
    default:
      return false;
  }
}

bool stage12SubtractiveEnabledId(RhythmArchetypeId id) {
  return stage12PhraseEnabledId(id) && id != 416;
}

constexpr uint16_t kReferenceArchetypeCapacity =
    static_cast<uint16_t>(Archetype::Count);

struct PhraseCatalogStorage {
  RhythmArchetype archetypes[kReferenceArchetypeCapacity]{};
  RhythmCatalogView view{};

  PhraseCatalogStorage() {
    const RhythmCatalogView& base = catalog();
    if (base.archetypeCount != kReferenceArchetypeCapacity) {
      view = base;
      return;
    }

    for (uint16_t index = 0; index < base.archetypeCount; ++index) {
      archetypes[index] = base.archetypes[index];
      if (!stage12PhraseEnabledId(archetypes[index].id)) continue;

      archetypes[index].allowedPhraseBars = kStage12PhraseBars;
      if (stage12SubtractiveEnabledId(archetypes[index].id)) {
        archetypes[index].trajectories = kSubtractivePhraseTrajectoryRefs;
        archetypes[index].trajectoryCount = static_cast<uint8_t>(
            sizeof(kSubtractivePhraseTrajectoryRefs) /
            sizeof(kSubtractivePhraseTrajectoryRefs[0]));
        archetypes[index].mutation = stage12SubtractiveMutationPolicy();
      } else {
        archetypes[index].trajectories = kNonSubtractivePhraseTrajectoryRefs;
        archetypes[index].trajectoryCount = static_cast<uint8_t>(
            sizeof(kNonSubtractivePhraseTrajectoryRefs) /
            sizeof(kNonSubtractivePhraseTrajectoryRefs[0]));
        archetypes[index].mutation = stage12NonSubtractiveMutationPolicy();
      }
    }

    view.archetypes = archetypes;
    view.archetypeCount = base.archetypeCount;
    view.trajectories = kPhraseTrajectories;
    view.trajectoryCount = static_cast<uint8_t>(
        sizeof(kPhraseTrajectories) / sizeof(kPhraseTrajectories[0]));
  }
};

}  // namespace

const RhythmCatalogView& phraseEvolutionCatalog() {
  static const PhraseCatalogStorage storage{};
  return storage.view;
}

bool phraseEvolutionEnabled(Archetype key) {
  const Definition* definition = definitionFor(key);
  return definition != nullptr && stage12PhraseEnabledId(definition->archetypeId);
}

}  // namespace ReferenceVocabulary
}  // namespace GroovePuterRhythm
