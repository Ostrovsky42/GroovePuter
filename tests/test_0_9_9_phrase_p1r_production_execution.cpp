#include <cassert>
#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/generation/migration/phrase_execution.h"

using namespace GroovePuterRhythm;

namespace {

struct PhysicalBar {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
};

GenreSettings genre(GenerativeMode mode) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = kBaseRecipeId;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  value.rhythmArchetypeId = kNoArchetypeId;
  return value;
}

PhraseExecutionMaterializationSettings materializationSettings() {
  PhraseExecutionMaterializationSettings value{};
  value.level = RealizationLevel::P2Variation;
  value.generationAttemptOrdinal = 0;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  value.tonalMaterializationEnabled = true;
  value.rootPitchClass = 0;
  value.scaleTypeValue = kScaleDorian;
  return value;
}

StrongRhythmMigrationContext legacyContext(int16_t address, uint8_t bar) {
  StrongRhythmMigrationContext value{};
  value.patternAddress = address;
  value.phraseBarOrdinal = bar;
  value.level = RealizationLevel::P2Variation;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  value.tonalMaterializationEnabled = true;
  value.rootPitchClass = 0;
  value.scaleTypeValue = kScaleDorian;
  return value;
}

PhysicalBar seededPhysicalBar() {
  PhysicalBar value{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    value.synthA.steps[step].note = static_cast<int8_t>(36 + (step % 12));
    value.synthB.steps[step].note = static_cast<int8_t>(60 + (step % 12));
    value.synthA.steps[step].velocity = 100;
    value.synthB.steps[step].velocity = 100;
    value.synthA.steps[step].probability = 100;
    value.synthB.steps[step].probability = 100;
  }
  return value;
}

bool sameSynth(const SynthPattern& left, const SynthPattern& right) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& a = left.steps[step];
    const SynthStep& b = right.steps[step];
    if (a.note != b.note || a.slide != b.slide || a.accent != b.accent ||
        a.ghost != b.ghost || a.velocity != b.velocity ||
        a.timing != b.timing || a.fx != b.fx || a.fxParam != b.fxParam ||
        a.probability != b.probability) {
      return false;
    }
  }
  return true;
}

bool sameDrums(const DrumPatternSet& left, const DrumPatternSet& right) {
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
  for (int lane = 0; lane < DrumPatternSet::kMaxLanes; ++lane) {
    const AutomationLane& a = left.lanes[lane];
    const AutomationLane& b = right.lanes[lane];
    if (a.targetParam != b.targetParam || a.nodeCount != b.nodeCount) {
      return false;
    }
    for (int node = 0; node < AutomationLane::kMaxNodes; ++node) {
      const AutomationNode& x = a.nodes[node];
      const AutomationNode& y = b.nodes[node];
      if (x.step != y.step || x.value != y.value ||
          x.curveType != y.curveType) {
        return false;
      }
    }
  }
  return left.groove.swing == right.groove.swing &&
         left.groove.humanize == right.groove.humanize;
}

bool samePhysical(const PhysicalBar& left, const PhysicalBar& right) {
  return sameDrums(left.drums, right.drums) &&
         sameSynth(left.synthA, right.synthA) &&
         sameSynth(left.synthB, right.synthB);
}

bool sameEvent(const HarmonicEvent& left, const HarmonicEvent& right) {
  return left.degree == right.degree && left.quality == right.quality &&
         left.rootOffsetSemitones == right.rootOffsetSemitones;
}

PreparedPhraseExecution prepare(const GenreSettings& settings,
                                uint16_t identity,
                                uint8_t bars) {
  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution prepared{};
  const PhraseExecutionStatus status = preparePhraseExecution(
      settings, materializationSettings(), identity, bars, scratch, prepared);
  assert(status == prepared.status);
  return prepared;
}

void testT1PrepareOnce() {
  const PreparedPhraseExecution prepared =
      prepare(genre(GenerativeMode::LoFi), 41, 8);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  assert(prepared.phraseGenerationIdentity == 41);
  assert(prepared.length.status == PhraseLengthRequestStatus::Accepted);
  assert(prepared.length.effectivePhraseBars == 8);
  assert(prepared.selection.resolved);
  assert(prepared.selection.phraseGenerationIdentity == 41);
  assert(prepared.selection.composition.status == GenerationCompositionStatus::Ok);
  assert(prepared.selection.composition.phraseBars == 8);
  assert(prepared.length.composition.phraseBars == 8);
  assert(prepared.selection.composition.progression ==
         prepared.length.composition.progression);
  assert(prepared.progressionSource.id ==
         prepared.selection.composition.progression);
  assert(prepared.progressionSource.period > 0);
  assert(prepared.harmonicClock.status ==
         PhraseHarmonicClockProjectionStatus::Ok);
  assert(prepared.harmonicClock.harmonicRhythmRealizationCount == 8);
  assert(prepared.semantic.status == PhraseSemanticContractStatus::Ready);
  assert(prepared.semantic.phraseGenerationIdentity == 41);
  std::puts("T1 prepare once: OK");
}

void testT2ExactLengths() {
  const GenreSettings rave = genre(GenerativeMode::Rave);
  for (const uint8_t bars : {uint8_t{1}, uint8_t{2}, uint8_t{4}}) {
    const PreparedPhraseExecution prepared = prepare(rave, 23, bars);
    assert(prepared.status == PhraseExecutionStatus::Ready);
    assert(prepared.length.requestedPhraseBars == bars);
    assert(prepared.length.effectivePhraseBars == bars);
    assert(prepared.selection.composition.phraseBars == bars);
  }
  const PreparedPhraseExecution eight =
      prepare(genre(GenerativeMode::LoFi), 23, 8);
  assert(eight.status == PhraseExecutionStatus::Ready);
  assert(eight.length.requestedPhraseBars == 8);
  assert(eight.length.effectivePhraseBars == 8);
  assert(eight.selection.composition.phraseBars == 8);
  std::puts("T2 exact 1/2/4/8: OK");
}

void testT3TypedRejects() {
  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution invalidDomain{};
  PhraseExecutionStatus status = preparePhraseExecution(
      genre(GenerativeMode::Rave), materializationSettings(), 5, 3,
      scratch, invalidDomain);
  assert(status == PhraseExecutionStatus::Rejected);
  assert(invalidDomain.length.status == PhraseLengthRequestStatus::Rejected);
  assert(invalidDomain.length.rejectReason ==
         PhraseLengthRejectReason::InvalidPhraseLengthDomain);

  PreparedPhraseExecution inadmissible{};
  status = preparePhraseExecution(
      genre(GenerativeMode::Rave), materializationSettings(), 5, 8,
      scratch, inadmissible);
  assert(status == PhraseExecutionStatus::Rejected);
  assert(inadmissible.length.status == PhraseLengthRequestStatus::Rejected);
  assert(inadmissible.length.rejectReason ==
         PhraseLengthRejectReason::NoAdmissibleLawForRequestedLength);
  std::puts("T3 typed rejects: OK");
}

void testT4TemporalCoordinates() {
  const PreparedPhraseExecution prepared =
      prepare(genre(GenerativeMode::LoFi), 51, 8);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  constexpr uint8_t vocabulary[] = {0, 1, 2, 3, 0, 1, 2, 3};
  constexpr uint8_t evolution[] = {0, 0, 0, 0, 1, 1, 1, 1};
  for (uint8_t bar = 0; bar < 8; ++bar) {
    assert(prepared.semantic.bars[bar].temporal.phraseBarOrdinal == bar);
    assert(phraseVocabularyBarOrdinal(bar) == vocabulary[bar]);
    assert(prepared.semantic.bars[bar].temporal.evolutionOrdinal ==
           evolution[bar]);
  }
  std::puts("T4 temporal coordinates: OK");
}

void testT5T6ClockCardinality() {
  for (const uint8_t bars :
       {uint8_t{1}, uint8_t{2}, uint8_t{4}, uint8_t{8}}) {
    const PhraseHarmonicClockProjection staticClock =
        projectPhraseHarmonicClock(bars, ProgressionId::StaticModal);
    assert(staticClock.status == PhraseHarmonicClockProjectionStatus::Ok);
    assert(staticClock.timeline.totalEventPositions == bars);

    const PhraseHarmonicClockProjection movingClock =
        projectPhraseHarmonicClock(bars, ProgressionId::PopCycle);
    assert(movingClock.status == PhraseHarmonicClockProjectionStatus::Ok);
    assert(movingClock.timeline.totalEventPositions ==
           static_cast<uint8_t>(bars * 2u));
  }
  std::puts("T5 static 1/2/4/8 positions: OK");
  std::puts("T6 moving 2/4/8/16 positions: OK");
}

void testT7BoundaryGlobalOrdinals() {
  const PhraseHarmonicClockProjection clock =
      projectPhraseHarmonicClock(2, ProgressionId::PopCycle);
  assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);
  const PhraseHarmonicEventCoordinate endOfBar0 =
      phraseHarmonicEventCoordinate(clock.timeline, 0, 1);
  const PhraseHarmonicEventCoordinate startOfBar1 =
      phraseHarmonicEventCoordinate(clock.timeline, 1, 0);
  assert(endOfBar0.valid && endOfBar0.localStep == 8 &&
         endOfBar0.phraseHarmonicEventOrdinal == 1);
  assert(startOfBar1.valid && startOfBar1.localStep == 0 &&
         startOfBar1.phraseHarmonicEventOrdinal == 2);

  ChordProgressionSourceRequest request{};
  request.requestedId = ProgressionId::PopCycle;
  request.family = RhythmFamily::FourFloor;
  request.generation.projectSeed = 0x50315231u;
  request.generation.phraseOrdinal = 17;
  request.phraseBars = 2;
  const ChordProgressionSourceResult source =
      realizeChordProgressionSource(request);
  assert(source.status == ChordProgressionStatus::Ok);
  HarmonicEvent first{}, second{};
  assert(chordProgressionSourceEventAt(
      source.source, endOfBar0.phraseHarmonicEventOrdinal, first));
  assert(chordProgressionSourceEventAt(
      source.source, startOfBar1.phraseHarmonicEventOrdinal, second));
  assert(sameEvent(first, source.source.events[1u % source.source.period]));
  assert(sameEvent(second, source.source.events[2u % source.source.period]));
  std::puts("T7 bar boundary global ordinals: OK");
}

void testT8TwoFiveOneIntrinsicPeriod() {
  ChordProgressionSourceRequest request{};
  request.requestedId = ProgressionId::TwoFiveOne;
  request.family = RhythmFamily::HipHopBackbeat;
  request.generation.projectSeed = 0x50315232u;
  request.generation.phraseOrdinal = 29;
  request.phraseBars = 8;
  const ChordProgressionSourceResult source =
      realizeChordProgressionSource(request);
  assert(source.status == ChordProgressionStatus::Ok);
  assert(source.source.id == ProgressionId::TwoFiveOne);
  assert(source.source.period == 3);

  struct Case { uint16_t ordinal; uint8_t intrinsic; };
  constexpr Case cases[] = {
      {8, 2}, {9, 0}, {11, 2}, {14, 2}, {15, 0},
  };
  for (const Case& testCase : cases) {
    HarmonicEvent event{};
    assert(chordProgressionSourceEventAt(
        source.source, testCase.ordinal, event));
    assert(sameEvent(event, source.source.events[testCase.intrinsic]));
  }
  std::puts("T8 TwoFiveOne global 8/9/11/14/15: OK");
}

void testT9RandomAccess() {
  const PreparedPhraseExecution prepared =
      prepare(genre(GenerativeMode::LoFi), 61, 8);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  const uint16_t identity = prepared.phraseGenerationIdentity;
  const uint8_t sourcePeriod = prepared.progressionSource.period;
  const uint8_t timelineCount = prepared.semantic.harmonicTimeline.totalEventPositions;

  const uint8_t order[] = {7, 0, 4, 7};
  PhysicalBar outputs[4]{};
  for (uint8_t index = 0; index < 4; ++index) {
    outputs[index] = seededPhysicalBar();
    const StrongRhythmMigrationResult result = materializePreparedPhraseBar(
        prepared, order[index], static_cast<int16_t>(100 + index),
        outputs[index].drums, outputs[index].synthA, outputs[index].synthB);
    assert(result.status == StrongRhythmMigrationStatus::Applied);
  }
  assert(samePhysical(outputs[0], outputs[3]));
  assert(prepared.phraseGenerationIdentity == identity);
  assert(prepared.progressionSource.period == sourcePeriod);
  assert(prepared.semantic.harmonicTimeline.totalEventPositions == timelineCount);
  std::puts("T9 random access 7->0->4->7: OK");
}

void testT10PatternAddressIndependence() {
  const PreparedPhraseExecution prepared =
      prepare(genre(GenerativeMode::LoFi), 71, 8);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  PhysicalBar left = seededPhysicalBar();
  PhysicalBar right = seededPhysicalBar();
  const StrongRhythmMigrationResult a = materializePreparedPhraseBar(
      prepared, 5, 24, left.drums, left.synthA, left.synthB);
  const StrongRhythmMigrationResult b = materializePreparedPhraseBar(
      prepared, 5, 219, right.drums, right.synthA, right.synthB);
  assert(a.status == StrongRhythmMigrationStatus::Applied);
  assert(b.status == StrongRhythmMigrationStatus::Applied);
  assert(a.archetype == b.archetype);
  assert(a.progressionId == b.progressionId);
  assert(a.harmonicEventOnsets == b.harmonicEventOnsets);
  assert(a.harmonicEventCount == b.harmonicEventCount);
  assert(samePhysical(left, right));
  std::puts("T10 patternAddress invariance: OK");
}

void testT11ProductionValidButEmpty() {
  const PreparedPhraseExecution prepared =
      prepare(genre(GenerativeMode::Electro), 19, 4);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  assert(prepared.selection.composition.melodicRhythm ==
         MelodicRhythmId::SparseCall);

  uint8_t emptyBar = 0xFFu;
  for (uint8_t bar = 0; bar < prepared.length.effectivePhraseBars; ++bar) {
    if (prepared.semantic.bars[bar].melodicStatus ==
        MelodicMotifStatus::ValidButEmpty) {
      emptyBar = bar;
      break;
    }
  }
  assert(emptyBar != 0xFFu);
  assert(emptyBar < prepared.length.effectivePhraseBars);
  assert(prepared.semantic.bars[emptyBar].temporal.phraseBarOrdinal == emptyBar);
  assert(prepared.semantic.bars[emptyBar].harmonicEvents.eventCount > 0);
  assert(prepared.semantic.harmonicTimeline.status ==
         PhraseHarmonicTimelineStatus::Ok);

  PhysicalBar physical = seededPhysicalBar();
  const StrongRhythmMigrationResult result = materializePreparedPhraseBar(
      prepared, emptyBar, 144, physical.drums, physical.synthA, physical.synthB);
  assert(result.status == StrongRhythmMigrationStatus::Applied);
  assert(result.melodicMotifStatus == MelodicMotifStatus::ValidButEmpty);
  std::puts("T11 production ValidButEmpty preserved: OK");
}

void testT12LifetimeCarrierInert() {
  const PreparedPhraseExecution prepared =
      prepare(genre(GenerativeMode::LoFi), 81, 8);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  for (uint8_t bar = 0; bar < prepared.length.effectivePhraseBars; ++bar) {
    assert(!prepared.semantic.bars[bar].melodicLifetime.entersFromPreviousBar);
    assert(!prepared.semantic.bars[bar].melodicLifetime.continuesIntoNextBar);
  }
  std::puts("T12 lifetime carrier present/all-false: OK");
}

void testT13LegacyNullOverride() {
  const GenreSettings settings = genre(GenerativeMode::Electro);
  StrongRhythmFrozenSelection frozen{};
  const StrongRhythmMigrationResult selection =
      resolveStrongRhythmFrozenSelection(
          settings, legacyContext(19, 0), 19, frozen);
  assert(selection.status == StrongRhythmMigrationStatus::Applied);
  assert(frozen.resolved);
  assert(selection.melodicRhythmId == MelodicRhythmId::SparseCall);
  assert(selection.motifShapeId == MotifShapeId::Mirror);
  assert(selection.phraseBars == 4);

  PhysicalBar bar0 = seededPhysicalBar();
  PhysicalBar bar1 = seededPhysicalBar();
  const StrongRhythmMigrationResult first = migrateStrongRhythmFrozenMaterial(
      settings, frozen, legacyContext(40, 0),
      bar0.drums, bar0.synthA, bar0.synthB);
  const StrongRhythmMigrationResult second = migrateStrongRhythmFrozenMaterial(
      settings, frozen, legacyContext(41, 1),
      bar1.drums, bar1.synthA, bar1.synthB);
  assert(first.status == StrongRhythmMigrationStatus::Applied);
  assert(second.status == StrongRhythmMigrationStatus::Applied);
  assert(first.melodicMotifStatus == MelodicMotifStatus::Ok);
  assert(second.melodicMotifStatus == MelodicMotifStatus::ValidButEmpty);
  std::puts("T13 legacy nullptr override compatibility: OK");
}

void testT14NoPublicationAndInvalidOutputUntouched() {
  const PreparedPhraseExecution prepared =
      prepare(genre(GenerativeMode::LoFi), 91, 8);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  PhysicalBar physical = seededPhysicalBar();
  const PhysicalBar before = physical;
  const StrongRhythmMigrationResult invalid = materializePreparedPhraseBar(
      prepared, 8, 12, physical.drums, physical.synthA, physical.synthB);
  assert(invalid.status == StrongRhythmMigrationStatus::InvalidContext);
  assert(samePhysical(before, physical));
  std::puts("T14 caller-owned/no publication invalid-output guard: OK");
}

void printMemoryReport() {
  const size_t oneBar = sizeof(DrumPatternSet) + 2u * sizeof(SynthPattern);
  const size_t oldState8 = 8u * oneBar;
  const size_t staging8 = 8u * oneBar;
  std::printf("MEM sizeof(PreparedPhraseExecution)=%zu\n",
              sizeof(PreparedPhraseExecution));
  std::printf("MEM sizeof(PhraseSemanticResult)=%zu\n",
              sizeof(PhraseSemanticResult));
  std::printf("MEM sizeof(StrongRhythmFrozenSelection)=%zu\n",
              sizeof(StrongRhythmFrozenSelection));
  std::printf("MEM sizeof(ChordProgressionSource)=%zu\n",
              sizeof(ChordProgressionSource));
  std::printf("MEM sizeof(SynthPattern)=%zu\n", sizeof(SynthPattern));
  std::printf("MEM sizeof(DrumPatternSet)=%zu\n", sizeof(DrumPatternSet));
  std::printf(
      "I2 INFORMATIONAL ONLY / NOT I2 POLICY: oneBar=%zu oldState8=%zu staging8=%zu combined=%zu\n",
      oneBar, oldState8, staging8, oldState8 + staging8);
}

}  // namespace

int main() {
  testT1PrepareOnce();
  testT2ExactLengths();
  testT3TypedRejects();
  testT4TemporalCoordinates();
  testT5T6ClockCardinality();
  testT7BoundaryGlobalOrdinals();
  testT8TwoFiveOneIntrinsicPeriod();
  testT9RandomAccess();
  testT10PatternAddressIndependence();
  testT11ProductionValidButEmpty();
  testT12LifetimeCarrierInert();
  testT13LegacyNullOverride();
  testT14NoPublicationAndInvalidOutputUntouched();
  printMemoryReport();
  std::puts("P1R focused production phrase execution: OK");
  return 0;
}
