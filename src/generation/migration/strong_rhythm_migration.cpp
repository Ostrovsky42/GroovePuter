#include "strong_rhythm_migration.h"

#include "../generation_context.h"
#include "../rhythm/rhythm_realizer.h"

namespace GroovePuterRhythm {
namespace {

uint32_t mixByte(uint32_t hash, uint8_t value) {
  // Compact FNV-1a-style mixer. This is context construction, not an RNG.
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
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
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

StepMask protectedSpaceFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask roleBit = rhythmRoleBit(role);
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    const ProtectedSpace& space = archetype.protectedSpaces[index];
    if ((space.affectedRoles & roleBit) != 0) {
      result = static_cast<StepMask>(result | space.steps);
    }
  }
  return result;
}

}  // namespace

StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings) {
  if (settings.morphAmount > 0 && settings.morphTarget != kBaseRecipeId &&
      settings.morphTarget != settings.recipe) {
    return StrongRhythmRoute::Legacy;
  }

  if (settings.generativeMode >= kGenerativeModeCount) {
    return StrongRhythmRoute::Legacy;
  }
  const GenerativeMode mode =
      static_cast<GenerativeMode>(settings.generativeMode);

  // Variant refines Genre; it never masks an incompatible Genre. Corrupted or
  // legacy mismatched pairs fall through to the base Genre route.
  switch (settings.recipe) {
    case 2:  // Drum&Bass
      if (mode == GenerativeMode::Broken) {
        return StrongRhythmRoute::DrumAndBass;
      }
      break;
    case 5:  // Dub Techno
      if (mode == GenerativeMode::Reggae) {
        return StrongRhythmRoute::DubTechno;
      }
      break;
    case 6:  // Chicago Jack
      if (mode == GenerativeMode::Acid) {
        return StrongRhythmRoute::ChicagoJack;
      }
      break;
    case 7:  // Rolling Acid
      if (mode == GenerativeMode::Acid) {
        return StrongRhythmRoute::RollingAcid;
      }
      break;
    case 10:  // Deep Chord
      if (mode == GenerativeMode::Reggae) {
        return StrongRhythmRoute::DeepChord;
      }
      break;
    case 1:   // UK Garage
    case 3:   // Footwork
    case 8:   // Classic 2-Step
    case 9:   // Dark Skippy
      if (mode == GenerativeMode::Broken) {
        return StrongRhythmRoute::Stage7Composition;
      }
      break;
    case 4:   // Psytrance
      if (mode == GenerativeMode::Rave) {
        return StrongRhythmRoute::Stage7Composition;
      }
      break;
    case 11:  // Minimal Space
      if (mode == GenerativeMode::Reggae) {
        return StrongRhythmRoute::Stage7Composition;
      }
      break;
    case kBaseRecipeId:
      break;
    default:
      return StrongRhythmRoute::Legacy;
  }

  switch (mode) {
    case GenerativeMode::Acid:
      return StrongRhythmRoute::AcidBase;
    case GenerativeMode::Darksynth:  // current visible name: Techno
      return StrongRhythmRoute::TechnoBase;
    case GenerativeMode::Rave:
      return StrongRhythmRoute::RaveBase;
    case GenerativeMode::Outrun:
    case GenerativeMode::Electro:
    case GenerativeMode::Reggae:
    case GenerativeMode::TripHop:
    case GenerativeMode::Broken:
    case GenerativeMode::Chip:
      return StrongRhythmRoute::Stage7Composition;
    default:
      return StrongRhythmRoute::Legacy;
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
  if (context.patternAddress < 0 ||
      context.patternAddress >= kMaxGlobalPatterns ||
      !validLevel(context.level) ||
      !isValidFeelProfile(context.feelProfile) ||
      context.feelAmount > 100) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const uint32_t projectSeed = projectSeedFor(settings, result.route);
  GenerationContext selectionGeneration{};
  selectionGeneration.projectSeed = projectSeed;
  selectionGeneration.phraseOrdinal =
      static_cast<uint16_t>(context.patternAddress);
  const RhythmSelectionResult selection =
      resolveRhythmSelection(settings, selectionGeneration);
  if (selection.status != RhythmSelectionStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  result.selectionMode = selection.mode;
  result.normalizedSelectionToAuto = selection.normalizedToAuto;

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(selection.archetypeId);
  if (definition == nullptr) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  result.archetype = definition->key;

  RhythmRealizationRequest request{};
  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = definition->archetypeId;
  request.phraseBars = 1;
  request.level = context.level;
  request.generation.projectSeed = projectSeed;
  request.generation.phraseOrdinal =
      static_cast<uint16_t>(context.patternAddress);

  const RhythmRealizationResult realization = realizeRhythmPhrase(request);
  result.realizationStatus = realization.status;
  if (realization.status != RealizationStatus::Ok &&
      realization.status != RealizationStatus::ValidButSparse) {
    result.status = StrongRhythmMigrationStatus::RealizationFailed;
    return result;
  }

  result.chordOnsets = roleOnsets(
      realization.plan.bars[0].roles[
          static_cast<uint8_t>(RhythmRole::ChordRhythm)]);

  MaterializedPatterns candidate{};
  PatternMaterializationDiagnostics diagnostics{};
  const PatternMaterializerBinding binding =
      standardDrumPatternBinding(deferredSynthRoles());
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

  // Vocabulary owns drum topology and Stage 8 owns bounded per-event timing.
  // Existing swing, automation and transport remain authoritative. Replace every
  // physical drum voice so unmapped legacy tom/clap events cannot contaminate
  // the relational groove, while preserving legacy lanes and PatternGroove.
  DrumPatternSet next = destination;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    next.voices[voice] = candidate.drums.voices[voice];
  }
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
  StrongRhythmMigrationResult result =
      migrateStrongRhythmDrums(settings, context, nextDrums);
  if (result.status != StrongRhythmMigrationStatus::Applied) {
    return result;
  }

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(result.archetype);
  const RhythmArchetype* archetype =
      definition == nullptr
          ? nullptr
          : ReferenceVocabulary::archetypeFor(result.archetype);
  if (definition == nullptr || archetype == nullptr) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  BassRhythmRequest bassRequest{};
  bassRequest.family = definition->family;
  bassRequest.archetypeId = definition->archetypeId;
  bassRequest.kickOnsets = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (nextDrums.voices[KICK].steps[step].hit) {
      bassRequest.kickOnsets = static_cast<StepMask>(
          bassRequest.kickOnsets | stepBit(step));
    }
  }
  bassRequest.protectedSpace =
      protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
  bassRequest.generation.projectSeed = projectSeedFor(settings, result.route);
  bassRequest.generation.phraseOrdinal =
      static_cast<uint16_t>(context.patternAddress);
  bassRequest.allowEmptyBar = definition->family == RhythmFamily::DubPulse ||
                              definition->family == RhythmFamily::SparsePulse ||
                              definition->family == RhythmFamily::HipHopBackbeat;
  const BassRhythmResult bass = realizeBassRhythm(bassRequest);
  result.bassRhythmStatus = bass.status;
  result.bassRhythmId = bass.plan.id;
  if (bass.status != BassRhythmStatus::Ok &&
      bass.status != BassRhythmStatus::ValidButEmpty) {
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
      RhythmRole::BassRhythm, bass.plan.onsets,
      context.feelProfile, context.feelAmount,
      bassRequest.generation, nextSynthA);
  if (result.bassFeelStatus != FeelInterpretStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
    return result;
  }

  ChordRhythmRequest chordRequest{};
  chordRequest.family = definition->family;
  chordRequest.archetypeId = definition->archetypeId;
  chordRequest.bassOnsets = bass.plan.onsets;
  chordRequest.protectedSpace =
      protectedSpaceFor(*archetype, RhythmRole::ChordRhythm);
  chordRequest.generation = bassRequest.generation;
  chordRequest.allowEmptyBar =
      definition->family == RhythmFamily::DubPulse ||
      definition->family == RhythmFamily::SparsePulse ||
      definition->family == RhythmFamily::HipHopBackbeat;
  const ChordRhythmResult chord = realizeChordRhythm(chordRequest);
  result.chordRhythmStatus = chord.status;
  result.chordRhythmId = chord.plan.id;
  result.chordOnsets = chord.plan.onsets;
  if (chord.status != ChordRhythmStatus::Ok &&
      chord.status != ChordRhythmStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  SynthPattern nextSynthB{};
  result.chordProjectionStatus = projectLegacyPitchPattern(
      synthB, chord.plan.onsets, chord.plan.continuations, nextSynthB);
  if (result.chordProjectionStatus != SemanticPatternProjectStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
    return result;
  }
  result.chordFeelStatus = applyFeelToSemanticPattern(
      RhythmRole::ChordRhythm, chord.plan.onsets,
      context.feelProfile, context.feelAmount,
      chordRequest.generation, nextSynthB);
  if (result.chordFeelStatus != FeelInterpretStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
    return result;
  }

  // Atomic cross-role commit: a failure in either semantic projection leaves
  // all three caller-owned patterns byte-for-byte unchanged.
  drums = nextDrums;
  synthA = nextSynthA;
  synthB = nextSynthB;
  result.chordRhythmApplied = true;
  return result;
}

}  // namespace GroovePuterRhythm
