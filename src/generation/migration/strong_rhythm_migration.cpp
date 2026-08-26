#include "strong_rhythm_migration.h"

#include "../generation_context.h"
#include "../rhythm/rhythm_realizer.h"

namespace GroovePuterRhythm {
#ifdef GROOVEPUTER_M1_TEST_PROBE
namespace {
StrongRhythmMelodicRequestProbe* g_melodicRequestProbe = nullptr;
}

void setStrongRhythmMelodicRequestProbe(
    StrongRhythmMelodicRequestProbe* probe) {
  g_melodicRequestProbe = probe;
}
#endif
namespace {

uint32_t mixByte(uint32_t hash, uint8_t value) {
  return (hash ^ static_cast<uint32_t>(value)) * 16777619u;
}

uint32_t mix32(uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value;
}

constexpr uint32_t kGenerationAttemptSalt = 0x4750524cu;  // "GPRL"

uint32_t projectSeedFor(const GenreSettings& settings,
                        StrongRhythmRoute route) {
  uint32_t hash = 2166136261u;
  hash = mixByte(hash, settings.generativeMode);
  hash = mixByte(hash, settings.recipe);

  // F-02/F-07 migration: these two zero bytes intentionally occupy the exact
  // slots previously owned by morphTarget/morphAmount. A historical default
  // scene (morphTarget=0, morphAmount=0) therefore keeps the exact pre-migration
  // attempt-0 seed, while persisted MORPH state can no longer alter generation.
  hash = mixByte(hash, 0);
  hash = mixByte(hash, 0);
  hash = mixByte(hash, static_cast<uint8_t>(route));
  return hash;
}

uint32_t realizationSeedFor(uint32_t selectionSeed, uint32_t attemptOrdinal) {
  if (attemptOrdinal == 0) return selectionSeed;
  return mix32(selectionSeed ^
               mix32(attemptOrdinal ^ kGenerationAttemptSalt));
}

void applyFullMaterialRerollArticulation(uint32_t attemptOrdinal,
                                         DrumPatternSet& drums) {
  if (attemptOrdinal == 0) return;

  uint16_t hitCount = 0;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (drums.voices[voice].steps[step].hit) ++hitCount;
    }
  }
  if (hitCount == 0) return;

  // Realization is intentionally bounded and can occasionally quantize two
  // adjacent RNG seeds to the same topology. Give every non-zero full-material
  // reroll a deterministic articulation within that topology. Attempt zero is
  // untouched for compatibility; selection/composition metadata is untouched.
  uint16_t target = static_cast<uint16_t>((attemptOrdinal - 1u) % hitCount);
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      DrumStep& event = drums.voices[voice].steps[step];
      if (!event.hit) continue;
      if (target != 0) {
        --target;
        continue;
      }
      event.accent = !event.accent;
      constexpr uint8_t kVelocityDelta = 9;
      event.velocity = event.velocity <= 127u - kVelocityDelta
          ? static_cast<uint8_t>(event.velocity + kVelocityDelta)
          : static_cast<uint8_t>(event.velocity - kVelocityDelta);
      return;
    }
  }
}

void applySynthRerollArticulation(uint32_t attemptOrdinal,
                                  SynthPattern& pattern) {
  if (attemptOrdinal == 0) return;

  uint8_t noteCount = 0;
  for (const SynthStep& event : pattern.steps) {
    if (event.note >= 0) ++noteCount;
  }
  if (noteCount == 0) return;

  uint8_t target = static_cast<uint8_t>((attemptOrdinal - 1u) % noteCount);
  for (SynthStep& event : pattern.steps) {
    if (event.note < 0) continue;
    if (target != 0) {
      --target;
      continue;
    }
    event.accent = !event.accent;
    constexpr uint8_t kVelocityDelta = 7;
    event.velocity = event.velocity <= 127u - kVelocityDelta
        ? static_cast<uint8_t>(event.velocity + kVelocityDelta)
        : static_cast<uint8_t>(event.velocity - kVelocityDelta);
    return;
  }
}

bool sameDrumMaterial(const DrumPatternSet& left,
                      const DrumPatternSet& right) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& a = left.voices[voice].steps[step];
      const DrumStep& b = right.voices[voice].steps[step];
      if (a.hit != b.hit || a.accent != b.accent ||
          a.velocity != b.velocity || a.timing != b.timing ||
          a.fx != b.fx || a.fxParam != b.fxParam ||
          a.probability != b.probability) {
        return false;
      }
    }
  }
  return true;
}

bool sameSynthMaterial(const SynthPattern& left,
                       const SynthPattern& right) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& a = left.steps[step];
    const SynthStep& b = right.steps[step];
    if (a.note != b.note || a.slide != b.slide || a.accent != b.accent ||
        a.ghost != b.ghost || a.velocity != b.velocity ||
        a.timing != b.timing || a.fx != b.fx ||
        a.fxParam != b.fxParam || a.probability != b.probability) {
      return false;
    }
  }
  return true;
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
    if ((space.affectedRoles & roleBit) != 0)
      result = static_cast<StepMask>(result | space.steps);
  }
  return result;
}

bool sparseSemanticBarsAllowed(const GenreSettings& settings,
                               RhythmFamily family) {
  return family == RhythmFamily::DubPulse ||
         family == RhythmFamily::SparsePulse ||
         family == RhythmFamily::HipHopBackbeat ||
         settings.generativeMode == static_cast<uint8_t>(GenerativeMode::LoFi) ||
         settings.generativeMode == static_cast<uint8_t>(GenerativeMode::HipHop) ||
         settings.generativeMode == static_cast<uint8_t>(GenerativeMode::FunkSoul);
}

uint8_t semanticBarOrdinal(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context) {
  if (context.phraseBarOrdinal != kUnspecifiedPhraseBarOrdinal) {
    return phraseVocabularyBarOrdinal(context.phraseBarOrdinal);
  }

  const bool useAddress =
      settings.generativeMode == static_cast<uint8_t>(GenerativeMode::LoFi) ||
      settings.generativeMode == static_cast<uint8_t>(GenerativeMode::HipHop) ||
      settings.generativeMode == static_cast<uint8_t>(GenerativeMode::FunkSoul);
  return useAddress
      ? static_cast<uint8_t>(context.patternAddress & 0xFF)
      : 0;
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
    if ((originalContinuations & bit) != 0 && active &&
        (blocked & bit) == 0) {
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

void copyCompositionToResult(const GenerationCompositionResult& composition,
                             StrongRhythmMigrationResult& result) {
  result.compositionStatus = composition.status;
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
}

bool validMigrationContext(const StrongRhythmMigrationContext& context) {
  return context.patternAddress >= 0 &&
         context.patternAddress < kMaxGlobalPatterns &&
         validLevel(context.level) && isValidFeelProfile(context.feelProfile) &&
         context.feelAmount <= 100;
}

}  // namespace

StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings) {
  if (settings.generativeMode >= kGenerativeModeCount)
    return StrongRhythmRoute::Legacy;

  const GenerativeMode mode = static_cast<GenerativeMode>(settings.generativeMode);
  switch (settings.recipe) {
    case 2:
      if (mode == GenerativeMode::Broken) return StrongRhythmRoute::DrumAndBass;
      break;
    case 5:
      if (mode == GenerativeMode::Reggae) return StrongRhythmRoute::DubTechno;
      break;
    case 6:
      if (mode == GenerativeMode::Acid) return StrongRhythmRoute::ChicagoJack;
      break;
    case 7:
      if (mode == GenerativeMode::Acid) return StrongRhythmRoute::RollingAcid;
      break;
    case 10:
      if (mode == GenerativeMode::Reggae) return StrongRhythmRoute::DeepChord;
      break;
    case 1:
    case 3:
    case 8:
    case 9:
      if (mode == GenerativeMode::Broken)
        return StrongRhythmRoute::Stage7Composition;
      break;
    case 4:
      if (mode == GenerativeMode::Rave)
        return StrongRhythmRoute::Stage7Composition;
      break;
    case 11:
      if (mode == GenerativeMode::Reggae)
        return StrongRhythmRoute::Stage7Composition;
      break;
    case kClassicChillRecipeId:
    case kDrunkenGrooveRecipeId:
    case kLoFiHouseRecipeId:
    case kMinimalSleepRecipeId:
      if (mode == GenerativeMode::LoFi)
        return StrongRhythmRoute::Stage7Composition;
      break;
    case kGoldenEraRecipeId:
    case kDustyJazzRecipeId:
      if (mode == GenerativeMode::HipHop)
        return StrongRhythmRoute::Stage7Composition;
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

StrongRhythmMigrationResult resolveStrongRhythmFrozenSelection(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    uint16_t phraseGenerationIdentity,
    StrongRhythmFrozenSelection& destination) {
  StrongRhythmMigrationResult result{};
  destination = StrongRhythmFrozenSelection{};
  result.route = selectStrongRhythmRoute(settings);
  if (result.route == StrongRhythmRoute::Legacy) {
    result.status = StrongRhythmMigrationStatus::Legacy;
    return result;
  }
  if (!validMigrationContext(context) ||
      phraseGenerationIdentity == kUnspecifiedPhraseGenerationIdentity) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const uint32_t selectionSeed = projectSeedFor(settings, result.route);
  destination.route = result.route;
  destination.selectionGeneration.projectSeed = selectionSeed;
  destination.selectionGeneration.phraseOrdinal = phraseGenerationIdentity;
  destination.realizationGeneration.projectSeed = realizationSeedFor(
      selectionSeed, context.generationAttemptOrdinal);
  destination.realizationGeneration.phraseOrdinal = phraseGenerationIdentity;
  destination.phraseGenerationIdentity = phraseGenerationIdentity;
  destination.composition = resolveGenerationComposition(
      settings, destination.selectionGeneration);
  copyCompositionToResult(destination.composition, result);
  if (destination.composition.status != GenerationCompositionStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(
          destination.composition.rhythmArchetypeId);
  if (definition == nullptr) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  result.archetype = definition->key;
  destination.resolved = true;
  result.status = StrongRhythmMigrationStatus::Applied;
  return result;
}

StrongRhythmMigrationResult migrateStrongRhythmDrums(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& destination) {
  StrongRhythmMigrationResult result{};
  StrongRhythmFrozenSelection compatibilitySelection{};
  const StrongRhythmFrozenSelection* selection = context.frozenSelection;
  if (selection == nullptr) {
    const StrongRhythmMigrationResult resolved =
        resolveStrongRhythmFrozenSelection(
            settings, context,
            static_cast<uint16_t>(context.patternAddress),
            compatibilitySelection);
    if (resolved.status != StrongRhythmMigrationStatus::Applied) return resolved;
    selection = &compatibilitySelection;
  }
  if (!validMigrationContext(context) || !selection->resolved ||
      selection->phraseGenerationIdentity ==
          kUnspecifiedPhraseGenerationIdentity) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }
  result.route = selection->route;
  copyCompositionToResult(selection->composition, result);
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(
          selection->composition.rhythmArchetypeId);
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
  request.generation = selection->realizationGeneration;

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

  DrumPatternSet next = destination;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice)
    next.voices[voice] = candidate.drums.voices[voice];
  destination = next;
  result.status = StrongRhythmMigrationStatus::Applied;
  return result;
}

namespace {

bool validTonalContext(const StrongRhythmMigrationContext& context) {
  return context.rootPitchClass <= 11 &&
         isValidScaleTypeValue(context.scaleTypeValue);
}

bool usableTonalResult(const TonalMaterializationResult& result) {
  return result.status == TonalMaterializationStatus::Ok ||
         result.status == TonalMaterializationStatus::ValidButEmpty;
}

TonalMaterializationResult materializeRole(
    const StrongRhythmMigrationContext& context,
    const TonalRegisterCorridor& corridor,
    const ChordProgressionPlan& progression,
    StepMask harmonicEventOnsets,
    StepMask onsets,
    StepMask continuations,
    const int8_t* tonalOffsets,
    uint16_t semitoneOffsetOrdinals) {
  TonalMaterializationRequest request{};
  request.onsets = onsets;
  request.continuations = continuations;
  request.harmonicEventOnsets = harmonicEventOnsets;
  request.progression = progression;
  request.semitoneOffsetOrdinals = semitoneOffsetOrdinals;
  for (uint8_t ordinal = 0; ordinal < kStepsPerBar; ++ordinal)
    request.tonalOffsets[ordinal] = tonalOffsets == nullptr ? 0 : tonalOffsets[ordinal];
  request.rootPitchClass = context.rootPitchClass;
  request.scaleTypeValue = context.scaleTypeValue;
  request.minMidi = corridor.minMidi;
  request.maxMidi = corridor.maxMidi;
  request.maxAdjacentLeapSemitones = corridor.maxAdjacentLeapSemitones;
  return materializeTonalIntent(request);
}

uint8_t filteredMelodicOffsets(const MelodicPitchIntentPlan& plan,
                               StepMask admittedOnsets,
                               int8_t* destination) {
  uint8_t output = 0;
  for (uint8_t ordinal = 0; ordinal < plan.onsetCount; ++ordinal) {
    if ((admittedOnsets & stepBit(plan.onsetSteps[ordinal])) == 0) continue;
    destination[output++] = plan.degreeOffsets[ordinal];
  }
  return output;
}

}  // namespace

namespace {

StrongRhythmMigrationResult migrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB,
    bool replaceDrums) {
  DrumPatternSet nextDrums = drums;
  StrongRhythmMigrationResult result =
      migrateStrongRhythmDrums(settings, context, nextDrums);
  if (result.status != StrongRhythmMigrationStatus::Applied) return result;
  if (!replaceDrums) nextDrums = drums;
  if (context.tonalMaterializationEnabled && !validTonalContext(context)) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(result.archetype);
  const RhythmArchetype* archetype = definition == nullptr ? nullptr
      : ReferenceVocabulary::archetypeFor(result.archetype);
  if (definition == nullptr || archetype == nullptr) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const bool allowSparse =
      sparseSemanticBarsAllowed(settings, definition->family);
  const uint8_t barOrdinal = semanticBarOrdinal(settings, context);
  const TonalGenerationProfile tonalProfile =
      tonalGenerationProfileFor(settings);
  GenerationContext materializationGeneration{};
  if (context.frozenSelection != nullptr) {
    materializationGeneration = context.frozenSelection->realizationGeneration;
  } else {
    const uint32_t selectionSeed = projectSeedFor(settings, result.route);
    materializationGeneration.projectSeed = realizationSeedFor(
        selectionSeed, context.generationAttemptOrdinal);
    materializationGeneration.phraseOrdinal =
        static_cast<uint16_t>(context.patternAddress);
  }

  BassRhythmRequest bassRequest{};
  bassRequest.requestedId = result.bassRhythmId;
  bassRequest.family = definition->family;
  bassRequest.archetypeId = definition->archetypeId;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if (nextDrums.voices[KICK].steps[step].hit)
      bassRequest.kickOnsets = static_cast<StepMask>(
          bassRequest.kickOnsets | stepBit(step));
  }
  bassRequest.protectedSpace =
      protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
  bassRequest.generation = materializationGeneration;
  bassRequest.barOrdinal = barOrdinal;
  bassRequest.allowEmptyBar = allowSparse;
  const BassRhythmResult bass = realizeBassRhythm(bassRequest);
  result.bassRhythmStatus = bass.status;
  result.bassRhythmId = bass.plan.id;
  if (bass.status != BassRhythmStatus::Ok &&
      bass.status != BassRhythmStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  BassPitchBehaviorResult bassPitch{};
  if (context.tonalMaterializationEnabled) {
    BassPitchBehaviorRequest pitchRequest{};
    pitchRequest.rhythmPlan = bass.plan;
    pitchRequest.archetypeId = definition->archetypeId;
    pitchRequest.generation = bassRequest.generation;
    pitchRequest.barOrdinal = barOrdinal;
    pitchRequest.policy = tonalProfile.bassPolicy;
    bassPitch = realizeBassPitchBehavior(pitchRequest);
    result.bassPitchBehaviorStatus = bassPitch.status;
    result.bassPitchContour = bassPitch.plan.contour;
    if (bassPitch.status != BassPitchBehaviorStatus::Ok &&
        bassPitch.status != BassPitchBehaviorStatus::ValidButEmpty) {
      result.status = StrongRhythmMigrationStatus::InvalidContext;
      return result;
    }
  }

  ChordRhythmRequest chordRequest{};
  chordRequest.requestedId = result.chordRhythmId;
  chordRequest.family = definition->family;
  chordRequest.archetypeId = definition->archetypeId;
  chordRequest.bassOnsets = bass.plan.onsets;
  chordRequest.protectedSpace =
      protectedSpaceFor(*archetype, RhythmRole::ChordRhythm);
  chordRequest.generation = bassRequest.generation;
  chordRequest.barOrdinal = barOrdinal;
  chordRequest.allowEmptyBar = allowSparse;
  const ChordRhythmResult chord = realizeChordRhythm(chordRequest);
  result.chordRhythmStatus = chord.status;
  result.chordRhythmId = chord.plan.id;
  result.chordOnsets = chord.plan.onsets;
  if (chord.status != ChordRhythmStatus::Ok &&
      chord.status != ChordRhythmStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  ChordProgressionRequest progressionRequest{};
  progressionRequest.requestedId = result.progressionId;
  progressionRequest.family = definition->family;
  progressionRequest.generation = chordRequest.generation;
  progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);
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
  melodicRequest.chordOnsets =
      result.synthBRole == SemanticSynthBRole::Melodic ? 0
                                                       : chord.plan.onsets;
  melodicRequest.protectedSpace =
      protectedSpaceFor(*archetype, RhythmRole::MelodicRhythm);
  melodicRequest.generation = bassRequest.generation;
  melodicRequest.barOrdinal = barOrdinal;
  melodicRequest.allowEmptyBar = allowSparse;
#ifdef GROOVEPUTER_M1_TEST_PROBE
  if (g_melodicRequestProbe != nullptr) {
    g_melodicRequestProbe->request = melodicRequest;
    g_melodicRequestProbe->captured = true;
  }
#endif
  const MelodicMotifResult melodic = realizeMelodicMotif(melodicRequest);
  result.melodicMotifStatus = melodic.status;
  result.melodicRhythmId = melodic.plan.rhythmId;
  result.motifShapeId = melodic.plan.motif.shape;
  if (melodic.status != MelodicMotifStatus::Ok &&
      melodic.status != MelodicMotifStatus::ValidButEmpty) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  MelodicPitchIntentResult melodicPitch{};
  if (context.tonalMaterializationEnabled) {
    MelodicPitchIntentRequest pitchRequest{};
    pitchRequest.rhythmPlan = melodic.plan;
    pitchRequest.archetypeId = definition->archetypeId;
    pitchRequest.generation = melodicRequest.generation;
    pitchRequest.barOrdinal = barOrdinal;
    pitchRequest.policy = tonalProfile.melodicPolicy;
    pitchRequest.allowedOnsetSteps = kAllSteps;
    pitchRequest.allowedContinuationSteps = kAllSteps;
    pitchRequest.allowEmptyBar = allowSparse;
    melodicPitch = realizeMelodicPitchIntent(pitchRequest);
    result.melodicPitchIntentStatus = melodicPitch.status;
    result.melodicPitchContour = melodicPitch.plan.contour;
    if (melodicPitch.status != MelodicPitchIntentStatus::Ok &&
        melodicPitch.status != MelodicPitchIntentStatus::ValidButEmpty) {
      result.status = StrongRhythmMigrationStatus::InvalidContext;
      return result;
    }
  }

  SynthPattern nextSynthA{};
  SynthPattern nextSynthB{};

  if (!context.tonalMaterializationEnabled) {
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
          melodic.plan.motif.sourceOrder, melodic.plan.motif.sourceOrderCount,
          nextSynthB);
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

      const StepMask chordOccupied = static_cast<StepMask>(
          chord.plan.onsets | chord.plan.continuations);
      const StepMask admittedOnsets = static_cast<StepMask>(
          melodic.plan.onsets & ~chordOccupied);
      const StepMask admittedContinuations = admittedMelodicContinuations(
          melodic.plan.onsets, melodic.plan.continuations,
          admittedOnsets, chordOccupied);

      SynthPattern melodicPattern{};
      result.melodicProjectionStatus = projectLegacyPitchPatternWithOrder(
          synthB, admittedOnsets, admittedContinuations,
          melodic.plan.motif.sourceOrder, melodic.plan.motif.sourceOrderCount,
          melodicPattern);
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
        if ((chordOccupied & bit) == 0 &&
            melodicPattern.steps[step].note >= 0) {
          nextSynthB.steps[step] = melodicPattern.steps[step];
        }
      }
      result.melodicFillOnsets = admittedOnsets;
      result.chordRhythmApplied = true;
      result.melodicRhythmApplied = true;
    }
  } else {
    const TonalMaterializationResult bassTonal = materializeRole(
        context, tonalProfile.bassRegister, progression.plan,
        chord.plan.onsets, bassPitch.plan.onsets,
        bassPitch.plan.continuations, bassPitch.plan.tonalOffsets,
        bassPitch.plan.semitoneOffsetOrdinals);
    result.bassTonalStatus = bassTonal.status;
    result.bassTonalProjectionStatus = bassTonal.projectionStatus;
    if (!usableTonalResult(bassTonal)) {
      result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
      return result;
    }
    result.bassTonalAdaptStatus = adaptTonalPlanToSynthPattern(
        synthA, bassTonal.plan, bassPitch.plan.accentOnsets,
        bassPitch.plan.slideIntoOnsets, nextSynthA);
    if (result.bassTonalAdaptStatus != TonalPatternAdaptStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
      return result;
    }
    result.bassFeelStatus = applyFeelToSemanticPattern(
        RhythmRole::BassRhythm, bassPitch.plan.onsets, context.feelProfile,
        context.feelAmount, bassRequest.generation, nextSynthA);
    if (result.bassFeelStatus != FeelInterpretStatus::Ok) {
      result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
      return result;
    }

    if (result.synthBRole == SemanticSynthBRole::Chord) {
      const TonalMaterializationResult chordTonal = materializeRole(
          context, tonalProfile.secondaryRegister, progression.plan,
          chord.plan.onsets, chord.plan.onsets, chord.plan.continuations,
          nullptr, 0);
      result.chordTonalStatus = chordTonal.status;
      result.chordTonalProjectionStatus = chordTonal.projectionStatus;
      if (!usableTonalResult(chordTonal)) {
        result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
        return result;
      }
      result.chordTonalAdaptStatus = adaptTonalPlanToSynthPattern(
          synthB, chordTonal.plan, 0, 0, nextSynthB);
      if (result.chordTonalAdaptStatus != TonalPatternAdaptStatus::Ok) {
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
      const TonalMaterializationResult melodicTonal = materializeRole(
          context, tonalProfile.secondaryRegister, progression.plan,
          chord.plan.onsets, melodicPitch.plan.onsets,
          melodicPitch.plan.continuations, melodicPitch.plan.degreeOffsets, 0);
      result.melodicTonalStatus = melodicTonal.status;
      result.melodicTonalProjectionStatus = melodicTonal.projectionStatus;
      if (!usableTonalResult(melodicTonal)) {
        result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
        return result;
      }
      result.melodicTonalAdaptStatus = adaptTonalPlanToSynthPattern(
          synthB, melodicTonal.plan, 0, 0, nextSynthB,
          melodic.plan.motif.sourceOrder, melodic.plan.motif.sourceOrderCount);
      if (result.melodicTonalAdaptStatus != TonalPatternAdaptStatus::Ok) {
        result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
        return result;
      }
      result.melodicFeelStatus = applyFeelToSemanticPattern(
          RhythmRole::MelodicRhythm, melodicPitch.plan.onsets,
          context.feelProfile, context.feelAmount,
          melodicRequest.generation, nextSynthB);
      if (result.melodicFeelStatus != FeelInterpretStatus::Ok) {
        result.status = StrongRhythmMigrationStatus::FeelApplyFailed;
        return result;
      }
      result.melodicRhythmApplied = true;
    } else {
      const TonalMaterializationResult chordTonal = materializeRole(
          context, tonalProfile.secondaryRegister, progression.plan,
          chord.plan.onsets, chord.plan.onsets, chord.plan.continuations,
          nullptr, 0);
      result.chordTonalStatus = chordTonal.status;
      result.chordTonalProjectionStatus = chordTonal.projectionStatus;
      if (!usableTonalResult(chordTonal)) {
        result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
        return result;
      }

      SynthPattern chordPattern{};
      result.chordTonalAdaptStatus = adaptTonalPlanToSynthPattern(
          synthB, chordTonal.plan, 0, 0, chordPattern);
      if (result.chordTonalAdaptStatus != TonalPatternAdaptStatus::Ok) {
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

      const StepMask chordOccupied = static_cast<StepMask>(
          chord.plan.onsets | chord.plan.continuations);
      const StepMask admittedOnsets = static_cast<StepMask>(
          melodicPitch.plan.onsets & ~chordOccupied);
      const StepMask admittedContinuations = admittedMelodicContinuations(
          melodicPitch.plan.onsets, melodicPitch.plan.continuations,
          admittedOnsets, chordOccupied);
      int8_t admittedOffsets[kStepsPerBar]{};
      const uint8_t admittedCount = filteredMelodicOffsets(
          melodicPitch.plan, admittedOnsets, admittedOffsets);
      if (admittedCount != onsetCount(admittedOnsets)) {
        result.status = StrongRhythmMigrationStatus::InvalidContext;
        return result;
      }

      const TonalMaterializationResult melodicTonal = materializeRole(
          context, tonalProfile.secondaryRegister, progression.plan,
          chord.plan.onsets, admittedOnsets, admittedContinuations,
          admittedOffsets, 0);
      result.melodicTonalStatus = melodicTonal.status;
      result.melodicTonalProjectionStatus = melodicTonal.projectionStatus;
      if (!usableTonalResult(melodicTonal)) {
        result.status = StrongRhythmMigrationStatus::CompatibilityBindingFailed;
        return result;
      }

      SynthPattern melodicPattern{};
      result.melodicTonalAdaptStatus = adaptTonalPlanToSynthPattern(
          synthB, melodicTonal.plan, 0, 0, melodicPattern,
          melodic.plan.motif.sourceOrder, melodic.plan.motif.sourceOrderCount);
      if (result.melodicTonalAdaptStatus != TonalPatternAdaptStatus::Ok) {
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
        if ((chordOccupied & bit) == 0 &&
            melodicPattern.steps[step].note >= 0) {
          nextSynthB.steps[step] = melodicPattern.steps[step];
        }
      }
      result.melodicFillOnsets = admittedOnsets;
      result.chordRhythmApplied = true;
      result.melodicRhythmApplied = true;
    }
    result.tonalMaterializationApplied = true;
  }

  if (replaceDrums) {
    const bool unchanged =
        sameDrumMaterial(nextDrums, drums) &&
        sameSynthMaterial(nextSynthA, synthA) &&
        sameSynthMaterial(nextSynthB, synthB);
    if (unchanged) {
      applyFullMaterialRerollArticulation(
          context.generationAttemptOrdinal, nextDrums);
    }
    drums = nextDrums;
  } else {
    if (sameSynthMaterial(nextSynthA, synthA)) {
      applySynthRerollArticulation(
          context.generationAttemptOrdinal, nextSynthA);
    }
    if (sameSynthMaterial(nextSynthB, synthB)) {
      applySynthRerollArticulation(
          context.generationAttemptOrdinal, nextSynthB);
    }
  }
  synthA = nextSynthA;
  synthB = nextSynthB;
  return result;
}

}  // namespace

StrongRhythmMigrationResult migrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  return migrateStrongRhythmMaterial(
      settings, context, drums, synthA, synthB, true);
}

StrongRhythmMigrationResult migrateStrongRhythmSynths(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  return migrateStrongRhythmMaterial(
      settings, context, drums, synthA, synthB, false);
}

StrongRhythmMigrationResult migrateStrongRhythmFrozenMaterial(
    const GenreSettings& settings,
    const StrongRhythmFrozenSelection& selection,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  StrongRhythmMigrationContext frozenContext = context;
  frozenContext.frozenSelection = &selection;
  frozenContext.phraseGenerationIdentity = selection.phraseGenerationIdentity;
  return migrateStrongRhythmMaterial(
      settings, frozenContext, drums, synthA, synthB, true);
}

}  // namespace GroovePuterRhythm
