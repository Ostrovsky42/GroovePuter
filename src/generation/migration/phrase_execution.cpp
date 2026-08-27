#include "phrase_execution.h"

#include "../rhythm/reference_vocabulary.h"

namespace GroovePuterRhythm {
namespace {

bool usableProgressionSourceStatus(ChordProgressionStatus status) {
  return status == ChordProgressionStatus::Ok ||
         status == ChordProgressionStatus::ValidButStatic;
}

StrongRhythmMigrationContext makeExecutionContext(
    const PreparedPhraseExecution& prepared,
    uint8_t phraseBarOrdinal,
    int16_t physicalPatternAddress) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = physicalPatternAddress;
  context.level = prepared.materialization.level;
  context.generationAttemptOrdinal =
      prepared.materialization.generationAttemptOrdinal;
  context.phraseBarOrdinal = phraseBarOrdinal;
  context.evolutionOrdinal =
      phraseTemporalCoordinatesForBar(phraseBarOrdinal).evolutionOrdinal;
  context.phraseGenerationIdentity = prepared.phraseGenerationIdentity;
  context.frozenSelection = &prepared.selection;
  context.feelProfile = prepared.materialization.feelProfile;
  context.feelAmount = prepared.materialization.feelAmount;
  context.tonalMaterializationEnabled =
      prepared.materialization.tonalMaterializationEnabled;
  context.rootPitchClass = prepared.materialization.rootPitchClass;
  context.scaleTypeValue = prepared.materialization.scaleTypeValue;
  return context;
}

void resetSemanticProbeScratch(PhraseExecutionScratch& scratch) {
  scratch = PhraseExecutionScratch{};

  // The legacy semantic projector requires a real pitch source. Preparation
  // therefore supplies a deterministic local source instead of weakening the
  // production contract or inventing a test-only bypass. The resulting physical
  // material is discarded after each bar.
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    SynthStep& a = scratch.synthA.steps[step];
    SynthStep& b = scratch.synthB.steps[step];
    a.note = static_cast<int8_t>(36 + (step % 12u));
    b.note = static_cast<int8_t>(60 + (step % 12u));
    a.velocity = 100;
    b.velocity = 100;
    a.probability = 100;
    b.probability = 100;
  }
}

PhraseMelodicBoundaryObservation observeMelodicSemanticProbe(
    uint16_t phraseGenerationIdentity,
    uint8_t phraseBarOrdinal,
    const StrongRhythmMigrationResult& realized,
    const SynthPattern& synthB) {
  PhraseMelodicBoundaryObservation observed{};
  observed.phraseGenerationIdentity = phraseGenerationIdentity;
  observed.phraseBarOrdinal = phraseBarOrdinal;
  observed.role = realized.synthBRole;
  observed.melodicStatus = realized.melodicMotifStatus;

  if (observed.role != SemanticSynthBRole::Melodic) return observed;
  if (!realized.melodicRhythmApplied ||
      (observed.melodicStatus != MelodicMotifStatus::Ok &&
       observed.melodicStatus != MelodicMotifStatus::ValidButEmpty)) {
    observed.melodicStatus = MelodicMotifStatus::InvalidRequest;
    return observed;
  }

  // P1R's semantic probe deliberately seeds every source SynthStep with
  // slide=false. In both existing pure-melodic projectors a realized onset
  // therefore remains slide=false while a local semantic continuation is
  // represented by a copied step with slide=true. Decode only this discarded
  // preparation probe; no retained physical pattern or runtime state is read.
  bool active = false;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& event = synthB.steps[step];
    if (event.note < 0) {
      active = false;
      continue;
    }

    const StepMask bit = stepBit(step);
    if (event.slide) {
      if (!active) {
        observed.admittedOnsets = 0;
        observed.admittedContinuations = 0;
        observed.melodicStatus = MelodicMotifStatus::InvalidRequest;
        return observed;
      }
      observed.admittedContinuations = static_cast<StepMask>(
          observed.admittedContinuations | bit);
      continue;
    }

    observed.admittedOnsets = static_cast<StepMask>(
        observed.admittedOnsets | bit);
    active = true;
  }

  const StepMask occupied = static_cast<StepMask>(
      observed.admittedOnsets | observed.admittedContinuations);
  if ((observed.melodicStatus == MelodicMotifStatus::ValidButEmpty &&
       occupied != 0) ||
      (observed.melodicStatus == MelodicMotifStatus::Ok &&
       observed.admittedOnsets == 0)) {
    observed.admittedOnsets = 0;
    observed.admittedContinuations = 0;
    observed.melodicStatus = MelodicMotifStatus::InvalidRequest;
  }
  return observed;
}

StrongRhythmMigrationResult invalidMaterializationResult() {
  StrongRhythmMigrationResult result{};
  result.status = StrongRhythmMigrationStatus::InvalidContext;
  return result;
}

}  // namespace

PhraseExecutionStatus preparePhraseExecution(
    const GenreSettings& settings,
    const PhraseExecutionMaterializationSettings& materialization,
    uint16_t phraseGenerationIdentity,
    uint8_t requestedPhraseBars,
    PhraseExecutionScratch& scratch,
    PreparedPhraseExecution& destination) {
  destination = PreparedPhraseExecution{};
  destination.settings = settings;
  destination.materialization = materialization;
  destination.phraseGenerationIdentity = phraseGenerationIdentity;

  StrongRhythmMigrationContext selectionContext{};
  selectionContext.patternAddress = 0;
  selectionContext.level = materialization.level;
  selectionContext.generationAttemptOrdinal =
      materialization.generationAttemptOrdinal;
  selectionContext.feelProfile = materialization.feelProfile;
  selectionContext.feelAmount = materialization.feelAmount;
  selectionContext.tonalMaterializationEnabled =
      materialization.tonalMaterializationEnabled;
  selectionContext.rootPitchClass = materialization.rootPitchClass;
  selectionContext.scaleTypeValue = materialization.scaleTypeValue;

  const StrongRhythmMigrationResult selectionResult =
      resolveStrongRhythmFrozenSelectionForPhraseBars(
          settings, selectionContext, phraseGenerationIdentity,
          requestedPhraseBars, destination.length, destination.selection);
  if (destination.length.status != PhraseLengthRequestStatus::Accepted) {
    destination.status = PhraseExecutionStatus::Rejected;
    return destination.status;
  }
  if (selectionResult.status != StrongRhythmMigrationStatus::Applied ||
      !destination.selection.resolved ||
      destination.length.effectivePhraseBars != requestedPhraseBars) {
    destination.status = PhraseExecutionStatus::InvalidContext;
    return destination.status;
  }

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(
          destination.selection.composition.rhythmArchetypeId);
  if (definition == nullptr) {
    destination.status = PhraseExecutionStatus::InvalidContext;
    return destination.status;
  }

  ChordProgressionSourceRequest sourceRequest{};
  sourceRequest.requestedId = destination.selection.composition.progression;
  sourceRequest.family = definition->family;
  sourceRequest.generation = destination.selection.realizationGeneration;
  sourceRequest.phraseBars = destination.length.effectivePhraseBars;
  const ChordProgressionSourceResult source =
      realizeChordProgressionSource(sourceRequest);
  if (!usableProgressionSourceStatus(source.status) ||
      source.source.id != destination.selection.composition.progression ||
      source.source.period == 0) {
    destination.status = PhraseExecutionStatus::ProgressionSourceFailure;
    return destination.status;
  }
  destination.progressionSource = source.source;

  destination.harmonicClock = projectPhraseHarmonicClock(
      destination.length.effectivePhraseBars,
      destination.selection.composition.progression);
  if (destination.harmonicClock.status !=
          PhraseHarmonicClockProjectionStatus::Ok ||
      destination.harmonicClock.harmonicRhythmRealizationCount !=
          destination.length.effectivePhraseBars) {
    destination.status = PhraseExecutionStatus::HarmonicProjectionFailure;
    return destination.status;
  }

  MelodicMotifStatus melodicStatus[kMaxSemanticPhraseBars]{};
  MelodicCrossBarLifetime melodicLifetime[kMaxSemanticPhraseBars]{};
  PhraseMelodicBoundaryObservation
      melodicObservation[kMaxSemanticPhraseBars]{};

  for (uint8_t bar = 0; bar < destination.length.effectivePhraseBars; ++bar) {
    resetSemanticProbeScratch(scratch);

    StrongRhythmMigrationContext barContext =
        makeExecutionContext(destination, bar, 0);
    StrongRhythmPhraseExecutionOverride execution{};
    execution.harmonicRhythm =
        &destination.harmonicClock.bars[bar].harmonicRhythm;
    execution.progressionSource = &destination.progressionSource;
    execution.firstGlobalHarmonicOrdinal =
        destination.harmonicClock.bars[bar].eventRange.firstOrdinal;
    barContext.phraseExecutionOverride = &execution;

    const StrongRhythmMigrationResult realized =
        migrateStrongRhythmFrozenMaterial(
            settings, destination.selection, barContext,
            scratch.drums, scratch.synthA, scratch.synthB);
    if (realized.status != StrongRhythmMigrationStatus::Applied) {
      resetSemanticProbeScratch(scratch);
      destination.status = PhraseExecutionStatus::SemanticProbeFailure;
      return destination.status;
    }
    melodicStatus[bar] = realized.melodicMotifStatus;
    melodicLifetime[bar] = MelodicCrossBarLifetime{};
    melodicObservation[bar] = observeMelodicSemanticProbe(
        phraseGenerationIdentity, bar, realized, scratch.synthB);
  }

  // One frozen phrase selection owns one phrase-global secondary semantic role.
  // Requiring that owner to be Melodic proves logical voice continuity without
  // deriving identity from physical Synth-B storage.
  const bool sameLogicalMelodicVoice =
      destination.selection.composition.secondaryRole ==
      CompositionSecondaryRole::Melodic;
  for (uint8_t bar = 0;
       static_cast<uint8_t>(bar + 1u) < destination.length.effectivePhraseBars;
       ++bar) {
    if (!c2AOnsetBoundaryEligible(
            melodicObservation[bar], melodicObservation[bar + 1u],
            destination.length.effectivePhraseBars,
            sameLogicalMelodicVoice)) {
      continue;
    }
    melodicLifetime[bar].continuesIntoNextBar = true;
    melodicLifetime[bar + 1u].entersFromPreviousBar = true;
  }

  resetSemanticProbeScratch(scratch);
  destination.semantic = makePhraseSemanticResult(
      phraseGenerationIdentity, destination.length,
      destination.harmonicClock.timeline, melodicStatus, melodicLifetime);
  if (destination.semantic.status != PhraseSemanticContractStatus::Ready) {
    destination.status = PhraseExecutionStatus::SemanticContractFailure;
    return destination.status;
  }

  destination.status = PhraseExecutionStatus::Ready;
  return destination.status;
}

StrongRhythmMigrationResult materializePreparedPhraseBar(
    const PreparedPhraseExecution& prepared,
    uint8_t phraseBarOrdinal,
    int16_t physicalPatternAddress,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  if (prepared.status != PhraseExecutionStatus::Ready ||
      prepared.semantic.status != PhraseSemanticContractStatus::Ready ||
      !prepared.selection.resolved ||
      phraseBarOrdinal >= prepared.length.effectivePhraseBars ||
      physicalPatternAddress < 0 ||
      physicalPatternAddress >= kMaxGlobalPatterns) {
    return invalidMaterializationResult();
  }

  StrongRhythmMigrationContext context =
      makeExecutionContext(prepared, phraseBarOrdinal, physicalPatternAddress);
  StrongRhythmPhraseExecutionOverride execution{};
  execution.harmonicRhythm =
      &prepared.harmonicClock.bars[phraseBarOrdinal].harmonicRhythm;
  execution.progressionSource = &prepared.progressionSource;
  execution.firstGlobalHarmonicOrdinal =
      prepared.harmonicClock.bars[phraseBarOrdinal].eventRange.firstOrdinal;
  context.phraseExecutionOverride = &execution;

  return migrateStrongRhythmFrozenMaterial(
      prepared.settings, prepared.selection, context, drums, synthA, synthB);
}

}  // namespace GroovePuterRhythm
