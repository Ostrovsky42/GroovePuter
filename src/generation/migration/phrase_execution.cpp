#include "phrase_execution.h"

#include "../rhythm/bar_evolution.h"
#include "../rhythm/reference_phrase_vocabulary.h"

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

// GF2-I3: a declared law only takes effect where the archetype is admitted to
// phrase evolution and the trajectory it maps to is eligible at this level.
// Anything else leaves the phrase on its established per-bar realization.
TrajectoryId admittedPhraseTrajectory(RhythmArchetypeId archetypeId,
                                      PhraseEvolutionLawId law,
                                      RealizationLevel level,
                                      uint8_t phraseBars) {
  if (phraseBars <= 1) return kNoTrajectoryId;
  const TrajectoryId requested = phraseTrajectoryForLaw(law, level);
  if (requested == kNoTrajectoryId) return kNoTrajectoryId;

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  if (definition == nullptr ||
      !ReferenceVocabulary::phraseEvolutionEnabled(definition->key)) {
    return kNoTrajectoryId;
  }

  const RhythmCatalogView& catalog =
      ReferenceVocabulary::phraseEvolutionCatalog();
  const RealizationLevelMask levelBit = realizationLevelBit(level);
  for (uint16_t index = 0; index < catalog.archetypeCount; ++index) {
    const RhythmArchetype& archetype = catalog.archetypes[index];
    if (archetype.id != archetypeId) continue;
    for (uint8_t ref = 0; ref < archetype.trajectoryCount; ++ref) {
      if (archetype.trajectories[ref].id != requested) continue;
      return (archetype.trajectories[ref].allowedLevels & levelBit) != 0
          ? requested
          : kNoTrajectoryId;
    }
    return kNoTrajectoryId;
  }
  return kNoTrajectoryId;
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

  // GF2-I3: realize the declared bar-function programme once for the phrase.
  destination.phraseTrajectory = admittedPhraseTrajectory(
      destination.selection.composition.rhythmArchetypeId,
      destination.selection.composition.phraseLaw, materialization.level,
      destination.length.effectivePhraseBars);
  if (destination.phraseTrajectory != kNoTrajectoryId) {
    BarEvolutionRequest evolution{};
    evolution.catalog = &ReferenceVocabulary::phraseEvolutionCatalog();
    evolution.archetypeId = definition->archetypeId;
    evolution.phraseBars = destination.length.effectivePhraseBars > kMaxPhraseBars
        ? kMaxPhraseBars
        : destination.length.effectivePhraseBars;
    evolution.level = materialization.level;
    evolution.generation = destination.selection.realizationGeneration;
    evolution.requestedTrajectoryId = destination.phraseTrajectory;

    const BarEvolutionResult evolved = evolveRhythmPhrase(evolution);
    if (evolved.status != BarEvolutionStatus::Ok || evolved.plan.barCount == 0) {
      // A law that cannot be realized must not fail the phrase; the established
      // per-bar realization stays in force.
      destination.phraseTrajectory = kNoTrajectoryId;
    } else {
      destination.phrasePlan = evolved.plan;
    }
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
    if (destination.phraseTrajectory != kNoTrajectoryId) {
      execution.barPlan = &destination.phrasePlan.bars[
          bar % destination.phrasePlan.barCount];
    }
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
    // P1R deliberately has no non-trivial cross-bar lifetime producer yet.
    // The fixed-capacity carrier remains present and all-false.
    melodicLifetime[bar] = MelodicCrossBarLifetime{};
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
  if (prepared.phraseTrajectory != kNoTrajectoryId) {
    execution.barPlan = &prepared.phrasePlan.bars[
        phraseBarOrdinal % prepared.phrasePlan.barCount];
  }
  context.phraseExecutionOverride = &execution;

  return migrateStrongRhythmFrozenMaterial(
      prepared.settings, prepared.selection, context, drums, synthA, synthB);
}

}  // namespace GroovePuterRhythm
