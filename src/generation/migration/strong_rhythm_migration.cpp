#include "strong_rhythm_migration.h"

#include "../generation_context.h"
#include "../rhythm/rhythm_realizer.h"

namespace GroovePuterRhythm {
namespace {

uint32_t mixByte(uint32_t hash, uint8_t value) {
  return (hash ^ static_cast<uint32_t>(value)) * 16777619u;
}

uint32_t projectSeedFor(const GenreSettings& settings,
                        StrongRhythmRoute route) {
  uint32_t hash = 2166136261u;
  hash = mixByte(hash, settings.generativeMode);
  hash = mixByte(hash, settings.recipe);
  hash = mixByte(hash, settings.morphTarget);
  hash = mixByte(hash, settings.morphAmount);
  hash = mixByte(hash, static_cast<uint8_t>(route));
  return hash;
}

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) < static_cast<uint8_t>(RealizationLevel::Count);
}

RhythmRoleMask deferredSynthRoles() {
  return static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::BassRhythm) |
      rhythmRoleBit(RhythmRole::ChordRhythm) |
      rhythmRoleBit(RhythmRole::MelodicRhythm));
}

StepMask roleOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(role.structural | role.secondary | role.ghosts);
}

uint8_t onsetCount(StepMask mask) {
  uint8_t result = 0;
  while (mask != 0) {
    result = static_cast<uint8_t>(result + (mask & 1u));
    mask = static_cast<StepMask>(mask >> 1u);
  }
  return result;
}

StepMask protectedSpaceFor(const RhythmArchetype& archetype, RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask roleBit = rhythmRoleBit(role);
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    const ProtectedSpace& space = archetype.protectedSpaces[index];
    if ((space.affectedRoles & roleBit) != 0) result = static_cast<StepMask>(result | space.steps);
  }
  return result;
}

bool sparseSemanticBarsAllowed(const GenreSettings& settings, RhythmFamily family) {
  return family == RhythmFamily::DubPulse || family == RhythmFamily::SparsePulse ||
         family == RhythmFamily::HipHopBackbeat ||
         settings.generativeMode == static_cast<uint8_t>(GenerativeMode::LoFi) ||
         settings.generativeMode == static_cast<uint8_t>(GenerativeMode::HipHop) ||
         settings.generativeMode == static_cast<uint8_t>(GenerativeMode::FunkSoul);
}

uint8_t semanticBarOrdinal(const GenreSettings& settings, int16_t patternAddress) {
  // Existing routes preserve their established bar-0 realization. New slow and
  // boom-bap directions use the stable pattern address as the deterministic
  // bar coordinate for their existing empty/rest policies.
  const bool useAddress =
      settings.generativeMode == static_cast<uint8_t>(GenerativeMode::LoFi) ||
      settings.generativeMode == static_cast<uint8_t>(GenerativeMode::HipHop) ||
      settings.generativeMode == static_cast<uint8_t>(GenerativeMode::FunkSoul);
  return useAddress ? static_cast<uint8_t>(patternAddress & 0xFF) : 0;
}

SemanticSynthBRole semanticSynthBRole(CompositionSecondaryRole role) {
  switch (role) {
    case CompositionSecondaryRole::Chord: return SemanticSynthBRole::Chord;
    case CompositionSecondaryRole::Melodic: return SemanticSynthBRole::Melodic;
    case CompositionSecondaryRole::ChordWithMelodicFill:
      return SemanticSynthBRole::ChordWithMelodicFill;
    case CompositionSecondaryRole::Count: break;
  }
  return SemanticSynthBRole::Melodic;
}

StepMask admittedMelodicContinuations(StepMask originalOnsets,
                                      StepMask originalContinuations,
                                      StepMask admittedOnsets,
                                      StepMask blocked) {
  StepMask result = 0;
  bool active = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((originalOnsets & bit) != 0) {
      active = (admittedOnsets & bit) != 0;
      continue;
    }
    if ((originalContinuations & bit) != 0 && active && (blocked & bit) == 0) {
      result = static_cast<StepMask>(result | bit);
      continue;
    }
    active = false;
  }
  return result;
}

bool stage14BaseGenre(GenerativeMode mode) {
  switch (mode) {
    case GenerativeMode::House:
    case GenerativeMode::Techno:
    case GenerativeMode::HipHop:
    case GenerativeMode::FunkSoul:
    case GenerativeMode::UkGarage:
    case GenerativeMode::DrumAndBass:
    case GenerativeMode::LoFi:
      return true;
    default:
      return false;
  }
}

}  // namespace

StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings) {
  if (settings.morphAmount > 0 && settings.morphTarget != kBaseRecipeId &&
      settings.morphTarget != settings.recipe) {
    return StrongRhythmRoute::Legacy;
  }
  if (settings.generativeMode >= kGenerativeModeCount) return StrongRhythmRoute::Legacy;

  const GenerativeMode mode = static_cast<GenerativeMode>(settings.generativeMode);
  switch (settings.recipe) {
    case 2: if (mode == GenerativeMode::Broken) return StrongRhythmRoute::DrumAndBass; break;
    case 5: if (mode == GenerativeMode::Reggae) return StrongRhythmRoute::DubTechno; break;
    case 6: if (mode == GenerativeMode::Acid) return StrongRhythmRoute::ChicagoJack; break;
    case 7: if (mode == GenerativeMode::Acid) return StrongRhythmRoute::RollingAcid; break;
    case 10: if (mode == GenerativeMode::Reggae) return StrongRhythmRoute::DeepChord; break;
    case 1:
    case 3:
    case 8:
    case 9:
      if (mode == GenerativeMode::Broken) return StrongRhythmRoute::Stage7Composition;
      break;
    case 4:
      if (mode == GenerativeMode::Rave) return StrongRhythmRoute::Stage7Composition;
      break;
    case 11:
      if (mode == GenerativeMode::Reggae) return StrongRhythmRoute::Stage7Composition;
      break;
    case kClassicChillRecipeId:
    case kDrunkenGrooveRecipeId:
    case kLoFiHouseRecipeId:
    case kMinimalSleepRecipeId:
      if (mode == GenerativeMode::LoFi) return StrongRhythmRoute::Stage7Composition;
      break;
    case kGoldenEraRecipeId:
    case kDustyJazzRecipeId:
      if (mode == GenerativeMode::HipHop) return StrongRhythmRoute::Stage7Composition;
      break;
    case kBaseRecipeId: break;
    default: return StrongRhythmRoute::Legacy;
  }

  switch (mode) {
    case GenerativeMode::Acid: return StrongRhythmRoute::AcidBase;
    case GenerativeMode::Darksynth: return StrongRhythmRoute::TechnoBase;
    case GenerativeMode::Rave: return StrongRhythmRoute::RaveBase;
    case GenerativeMode::Outrun:
    case GenerativeMode::Electro:
    case GenerativeMode::Reggae:
    case GenerativeMode::TripHop:
    case GenerativeMode::Broken:
    case GenerativeMode::Chip:
      return StrongRhythmRoute::Stage7Composition;
    default:
      return stage14BaseGenre(mode) ? StrongRhythmRoute::Stage7Composition
                                    : StrongRhythmRoute::Legacy;
  }
}

StrongRhythmMigrationResult migrateStrongRhythmDrums(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& destination) {
  StrongRhythmMigrationResult result{};
  result.route = selectStrongRhythmRoute(settings);
  if (result.route == StrongRhythmRoute::Legacy) {
    result.status = StrongRhythmMigrationStatus::Legacy;
    return result;
  }
  if (context.patternAddress < 0 || context.patternAddress >= kMaxGlobalPatterns ||
      !validLevel(context.level) || !isValidFeelProfile(context.feelProfile) ||
      context.feelAmount > 100) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const uint32_t projectSeed = projectSeedFor(settings, result.route);
  GenerationContext selectionGeneration{};
  selectionGeneration.projectSeed = projectSeed;
  selectionGeneration.phraseOrdinal = static_cast<uint16_t>(context.patternAddress);
  const GenerationCompositionResult composition =
      resolveGenerationComposition(settings, selectionGeneration);
  result.compositionStatus = composition.status;
  if (composition.status != GenerationCompositionStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  result.selectionMode = composition.rhythmSelectionMode;
  result.normalizedSelectionToAuto = composition.normalizedRhythmToAuto;
  result.suggestedFeel = composition.suggestedFeel;
  result.bassRhythmId = composition.bassRhythm;
  result.chordRhythmId = composition.chordRhythm;
  result.progressionId = composition.progression;
  result.melodicRhythmId = composition.melodicRhythm;
  result.motifShapeId = composition.motifShape;
  result.phraseLaw = composition.phraseLaw;
  result.phraseBars = composition.phraseBars;
  result.corridor = composition.corridor;
  result.synthBRole = semanticSynthBRole(composition.secondaryRole);

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
  if (definition == nullptr) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  result.archetype = definition->key;

  RhythmRealizationRequest request{};
  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = definition->archetypeId;
  // Stage 12 remains production-blocked until its documented physical gate.
  request.phraseBars = 1;
  request.level = context.level;
  request.generation.projectSeed = projectSeed;
  request.generation.phraseOrdinal = static_cast<uint16_t>(context.patternAddress);

  const RhythmRealizationResult realization = realizeRhythmPhrase(request);
  result.realizationStatus = realization.status;
  if (realization.status != RealizationStatus::Ok &&
      realization.status != RealizationStatus::ValidButSparse) {
    result.status = StrongRhythmMigrationStatus::RealizationFailed;
    return result;
  }

  result.chordOnsets = roleOnsets(
      realization.plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::ChordRhythm)]);

  MaterializedPatterns candidate{};
  PatternMaterializationDiagnostics diagnostics{};
  const PatternMaterializerBinding binding = standardDrumPatternBinding(deferredSynthRoles());
  result.materializationStatus = materializeRhythmPattern(
      realization.plan, binding, candidate, &diagnostics);
  if (result.materializationStatus != PatternMaterializeStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::MaterializationFailed;
    return result;
  }
  result.feelStatus = applyFeelToMaterializedPattern(
      realization.plan, binding, context.feelProfile, context.feelAmount,
      request.generation, candidate);
  if (result.feelStatus != FeelPatternApplyStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
    return result;
  }

  DrumPatternSet next = destination;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice)
    next.voices[voice] = candidate.drums.voices[voice];
  destination = next;
  result.status = StrongRhythmMigrationStatus::Applied;
  return result;
}

StrongRhythmMigrationResult migrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  DrumPatternSet nextDrums = drums;
  StrongRhythmMigrationResult result = migrateStrongRhythmDrums(settings, context, nextDrums);
  if (result.status != StrongRhythmMigrationStatus::Applied) return result;

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(result.archetype);
  const RhythmArchetype* archetype = definition == nullptr ? nullptr
      : ReferenceVocabulary::archetypeFor(result.archetype);
  if (definition == nullptr || archetype == nullptr) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const bool allowSparse = sparseSemanticBarsAllowed(settings, definition->family);
  const uint8_t barOrdinal = semanticBarOrdinal(settings, context.patternAddress);

  BassRhythmRequest bassRequest{};
  bassRequest.requestedId = result.bassRhythmId;
  bassRequest.family = definition->family;
  bassRequest.archetypeId = definition->archetypeId;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (nextDrums.voices[KICK].steps[step].hit)
      bassRequest.kickOnsets = static_cast<StepMask>(bassRequest.kickOnsets | stepBit(step));
  }
  bassRequest.protectedSpace = protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
  bassRequest.generation.projectSeed = projectSeedFor(settings, result.route);
  bassRequest.generation.phraseOrdinal = static_cast<uint16_t>(context.patternAddress);
  bassRequest.barOrdinal = barOrdinal;
  bassRequest.allowEmptyBar = allowSparse;
  const BassRhythmResult bass = realizeBassRhythm(bassRequest);
  result.bassRhythmStatus = bass.status;
  result.bassRhythmId = bass.plan.id;
  if (bass.status != BassRhythmStatus::Ok && bass.status != BassRhythmStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  SynthPattern nextSynthA{};
  result.bassProjectionStatus = projectLegacyPitchPattern(
      synthA, bass.plan.onsets, bass.plan.continuations, nextSynthA);
  if (result.bassProjectionStatus != SemanticPatternProjectStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
    return result;
  }
  result.bassFeelStatus = applyFeelToSemanticPattern(
      RhythmRole::BassRhythm, bass.plan.onsets, context.feelProfile,
      context.feelAmount, bassRequest.generation, nextSynthA);
  if (result.bassFeelStatus != FeelInterpretStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
    return result;
  }

  ChordRhythmRequest chordRequest{};
  chordRequest.requestedId = result.chordRhythmId;
  chordRequest.family = definition->family;
  chordRequest.archetypeId = definition->archetypeId;
  chordRequest.bassOnsets = bass.plan.onsets;
  chordRequest.protectedSpace = protectedSpaceFor(*archetype, RhythmRole::ChordRhythm);
  chordRequest.generation = bassRequest.generation;
  chordRequest.barOrdinal = barOrdinal;
  chordRequest.allowEmptyBar = allowSparse;
  const ChordRhythmResult chord = realizeChordRhythm(chordRequest);
  result.chordRhythmStatus = chord.status;
  result.chordRhythmId = chord.plan.id;
  result.chordOnsets = chord.plan.onsets;
  if (chord.status != ChordRhythmStatus::Ok && chord.status != ChordRhythmStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  ChordProgressionRequest progressionRequest{};
  progressionRequest.requestedId = result.progressionId;
  progressionRequest.family = definition->family;
  progressionRequest.generation = chordRequest.generation;
  progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);
  // The production bridge is still intentionally one-bar. ChordRhythm owns
  // event timing; Stage 15 fills only the harmonic content of those events.
  progressionRequest.phraseBars = 1;
  const ChordProgressionResult progression =
      realizeChordProgression(progressionRequest);
  result.chordProgressionStatus = progression.status;
  result.progressionId = progression.plan.id;
  if (progression.status != ChordProgressionStatus::Ok &&
      progression.status != ChordProgressionStatus::ValidButStatic) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  MelodicMotifRequest melodicRequest{};
  melodicRequest.requestedRhythm = result.melodicRhythmId;
  melodicRequest.requestedShape = result.motifShapeId;
  melodicRequest.family = definition->family;
  melodicRequest.archetypeId = definition->archetypeId;
  melodicRequest.bassOnsets = bass.plan.onsets;
  melodicRequest.chordOnsets = result.synthBRole == SemanticSynthBRole::Melodic ? 0 : chord.plan.onsets;
  melodicRequest.protectedSpace = protectedSpaceFor(*archetype, RhythmRole::MelodicRhythm);
  melodicRequest.generation = bassRequest.generation;
  melodicRequest.barOrdinal = barOrdinal;
  melodicRequest.allowEmptyBar = allowSparse;
  const MelodicMotifResult melodic = realizeMelodicMotif(melodicRequest);
  result.melodicMotifStatus = melodic.status;
  result.melodicRhythmId = melodic.plan.rhythmId;
  result.motifShapeId = melodic.plan.motif.shape;
  if (melodic.status != MelodicMotifStatus::Ok &&
      melodic.status != MelodicMotifStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  SynthPattern nextSynthB{};
  if (result.synthBRole == SemanticSynthBRole::Chord) {
    result.chordProjectionStatus = projectLegacyPitchPattern(
        synthB, chord.plan.onsets, chord.plan.continuations, nextSynthB);
    if (result.chordProjectionStatus != SemanticPatternProjectStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
      return result;
    }
    result.chordFeelStatus = applyFeelToSemanticPattern(
        RhythmRole::ChordRhythm, chord.plan.onsets, context.feelProfile,
        context.feelAmount, chordRequest.generation, nextSynthB);
    if (result.chordFeelStatus != FeelInterpretStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
      return result;
    }
    result.chordRhythmApplied = true;
  } else if (result.synthBRole == SemanticSynthBRole::Melodic) {
    result.melodicProjectionStatus = projectLegacyPitchPatternWithOrder(
        synthB, melodic.plan.onsets, melodic.plan.continuations,
        melodic.plan.motif.sourceOrder, melodic.plan.motif.sourceOrderCount, nextSynthB);
    if (result.melodicProjectionStatus != SemanticPatternProjectStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
      return result;
    }
    result.melodicFeelStatus = applyFeelToSemanticPattern(
        RhythmRole::MelodicRhythm, melodic.plan.onsets, context.feelProfile,
        context.feelAmount, melodicRequest.generation, nextSynthB);
    if (result.melodicFeelStatus != FeelInterpretStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
      return result;
    }
    result.melodicRhythmApplied = true;
  } else {
    SynthPattern chordPattern{};
    result.chordProjectionStatus = projectLegacyPitchPattern(
        synthB, chord.plan.onsets, chord.plan.continuations, chordPattern);
    if (result.chordProjectionStatus != SemanticPatternProjectStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
      return result;
    }
    result.chordFeelStatus = applyFeelToSemanticPattern(
        RhythmRole::ChordRhythm, chord.plan.onsets, context.feelProfile,
        context.feelAmount, chordRequest.generation, chordPattern);
    if (result.chordFeelStatus != FeelInterpretStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
      return result;
    }

    const StepMask chordOccupied = static_cast<StepMask>(chord.plan.onsets | chord.plan.continuations);
    const StepMask admittedOnsets = static_cast<StepMask>(melodic.plan.onsets & ~chordOccupied);
    const StepMask admittedContinuations = admittedMelodicContinuations(
        melodic.plan.onsets, melodic.plan.continuations, admittedOnsets, chordOccupied);

    SynthPattern melodicPattern{};
    result.melodicProjectionStatus = projectLegacyPitchPatternWithOrder(
        synthB, admittedOnsets, admittedContinuations,
        melodic.plan.motif.sourceOrder, melodic.plan.motif.sourceOrderCount, melodicPattern);
    if (result.melodicProjectionStatus != SemanticPatternProjectStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
      return result;
    }
    result.melodicFeelStatus = applyFeelToSemanticPattern(
        RhythmRole::MelodicRhythm, admittedOnsets, context.feelProfile,
        context.feelAmount, melodicRequest.generation, melodicPattern);
    if (result.melodicFeelStatus != FeelInterpretStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
      return result;
    }

    nextSynthB = chordPattern;
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      const StepMask bit = stepBit(step);
      if ((chordOccupied & bit) == 0 && melodicPattern.steps[step].note >= 0)
        nextSynthB.steps[step] = melodicPattern.steps[step];
    }
    result.melodicFillOnsets = admittedOnsets;
    result.chordRhythmApplied = true;
    result.melodicRhythmApplied = true;
  }

  drums = nextDrums;
  synthA = nextSynthA;
  synthB = nextSynthB;
  return result;
}

}  // namespace GroovePuterRhythm
