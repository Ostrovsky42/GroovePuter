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

bool routeUsesLegacyStab(StrongRhythmRoute route) {
  return route == StrongRhythmRoute::DubTechno ||
         route == StrongRhythmRoute::DeepChord;
}

StepMask roleOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(role.structural | role.secondary | role.ghosts);
}

bool remapLegacyStab(const SynthPattern& legacy,
                     StepMask chordOnsets,
                     SynthPattern& destination) {
  if (chordOnsets == 0) return false;

  SynthStep sourceEvents[SynthPattern::kSteps]{};
  uint8_t sourceCount = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (legacy.steps[step].note < 0) continue;
    sourceEvents[sourceCount++] = legacy.steps[step];
  }
  if (sourceCount == 0) return false;

  SynthPattern next{};
  uint8_t sourceIndex = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((chordOnsets & stepBit(step)) == 0) continue;
    // Preserve the legacy musical event in chronological order. Vocabulary
    // changes only its onset coordinate; pitch/performance data remain legacy.
    next.steps[step] = sourceEvents[sourceIndex % sourceCount];
    ++sourceIndex;
  }
  destination = next;
  return true;
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
    SynthPattern& synthB) {
  DrumPatternSet nextDrums = drums;
  StrongRhythmMigrationResult result =
      migrateStrongRhythmDrums(settings, context, nextDrums);
  if (result.status != StrongRhythmMigrationStatus::Applied) {
    return result;
  }

  if (!routeUsesLegacyStab(result.route)) {
    drums = nextDrums;
    return result;
  }

  SynthPattern nextSynthB{};
  if (!remapLegacyStab(synthB, result.chordOnsets, nextSynthB)) {
    result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
    return result;
  }

  // Atomic Stage 5 commit: if legacy pitch binding cannot be established,
  // neither the Vocabulary drums nor the stab topology escape the scratch copy.
  drums = nextDrums;
  synthB = nextSynthB;
  result.chordRhythmApplied = true;
  return result;
}

}  // namespace GroovePuterRhythm
