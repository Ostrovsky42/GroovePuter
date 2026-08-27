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

GenreSettings genre(GenerativeMode mode, GenreRecipeId recipe = kBaseRecipeId) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = recipe;
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

PreparedPhraseExecution prepareReady(const GenreSettings& settings,
                                     uint16_t identity,
                                     uint8_t bars) {
  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution prepared{};
  const PhraseExecutionStatus status = preparePhraseExecution(
      settings, materializationSettings(), identity, bars, scratch, prepared);
  assert(status == PhraseExecutionStatus::Ready);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  assert(prepared.semantic.status == PhraseSemanticContractStatus::Ready);
  assert(prepared.phraseGenerationIdentity == identity);
  assert(prepared.length.effectivePhraseBars == bars);
  return prepared;
}

PhraseMelodicBoundaryObservation observation(
    uint16_t identity,
    uint8_t bar,
    SemanticSynthBRole role,
    MelodicMotifStatus status,
    StepMask onsets,
    StepMask continuations) {
  PhraseMelodicBoundaryObservation value{};
  value.phraseGenerationIdentity = identity;
  value.phraseBarOrdinal = bar;
  value.role = role;
  value.melodicStatus = status;
  value.admittedOnsets = onsets;
  value.admittedContinuations = continuations;
  return value;
}

void assertPaired(const PreparedPhraseExecution& prepared) {
  assert(!prepared.semantic.bars[0].melodicLifetime.entersFromPreviousBar);
  const uint8_t bars = prepared.length.effectivePhraseBars;
  for (uint8_t bar = 0; static_cast<uint8_t>(bar + 1u) < bars; ++bar) {
    assert(prepared.semantic.bars[bar].melodicLifetime.continuesIntoNextBar ==
           prepared.semantic.bars[bar + 1u].melodicLifetime.entersFromPreviousBar);
  }
  assert(!prepared.semantic.bars[bars - 1u].melodicLifetime.continuesIntoNextBar);
}

PhysicalBar materialize(const PreparedPhraseExecution& prepared,
                        uint8_t bar,
                        int16_t address) {
  PhysicalBar physical = seededPhysicalBar();
  const StrongRhythmMigrationResult result = materializePreparedPhraseBar(
      prepared, bar, address,
      physical.drums, physical.synthA, physical.synthB);
  assert(result.status == StrongRhythmMigrationStatus::Applied);
  return physical;
}

void testT2KnownAOnsetPositive() {
  const PreparedPhraseExecution prepared =
      prepareReady(GenreSettings{}, 2, 2);
  assert(prepared.selection.composition.secondaryRole ==
         CompositionSecondaryRole::Melodic);
  assert(prepared.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(prepared.semantic.bars[1].melodicLifetime.entersFromPreviousBar);
  assertPaired(prepared);
  std::puts("T2 known A-onset positive: OK");
}

void testT3ProductionDefaultPositive() {
  GenreSettings settings{};
  assert(settings.generativeMode == static_cast<uint8_t>(GenerativeMode::Acid));
  assert(settings.recipe == kBaseRecipeId);
  assert(settings.rhythmSelectionMode ==
         static_cast<uint8_t>(RhythmSelectionMode::Auto));
  const PreparedPhraseExecution prepared = prepareReady(settings, 2, 2);
  assert(prepared.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(prepared.semantic.bars[1].melodicLifetime.entersFromPreviousBar);
  std::puts("T3 production-default positive: OK");
}

void testT4LengthCoverage() {
  const PreparedPhraseExecution two = prepareReady(GenreSettings{}, 2, 2);
  const PreparedPhraseExecution four = prepareReady(GenreSettings{}, 2, 4);
  const PreparedPhraseExecution eight =
      prepareReady(genre(GenerativeMode::Broken, 9), 0, 8);
  const PreparedPhraseExecution one =
      prepareReady(genre(GenerativeMode::Rave), 23, 1);

  assert(two.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(four.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(eight.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(!one.semantic.bars[0].melodicLifetime.entersFromPreviousBar);
  assert(!one.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  std::puts("T4 exact length coverage 1/2/4/8: OK");
}

void testT5EvolutionSeam() {
  const PreparedPhraseExecution prepared =
      prepareReady(genre(GenerativeMode::Broken, 9), 0, 8);
  assert(prepared.semantic.bars[3].temporal.evolutionOrdinal == 0);
  assert(prepared.semantic.bars[4].temporal.evolutionOrdinal == 1);
  assert(prepared.semantic.bars[3].melodicLifetime.continuesIntoNextBar);
  assert(prepared.semantic.bars[4].melodicLifetime.entersFromPreviousBar);
  assertPaired(prepared);
  std::puts("T5 8-bar 3->4 evolution seam: OK");
}

void testT6ToT11ClassifierNegatives() {
  const StepMask step0 = stepBit(0);
  const StepMask step12 = stepBit(12);
  const StepMask step14 = stepBit(14);
  const StepMask step15 = stepBit(15);

  const auto aOut = observation(
      7, 0, SemanticSynthBRole::Melodic, MelodicMotifStatus::Ok,
      static_cast<StepMask>(step12 | step15), 0);
  const auto aIn = observation(
      7, 1, SemanticSynthBRole::Melodic, MelodicMotifStatus::Ok,
      step12, 0);
  assert(c2AOnsetBoundaryEligible(aOut, aIn, 2, true));

  auto step0In = aIn;
  step0In.admittedOnsets = static_cast<StepMask>(step0 | step12);
  assert(!c2AOnsetBoundaryEligible(aOut, step0In, 2, true));
  std::puts("T6 incoming step0 rejected: OK");

  auto emptyIn = aIn;
  emptyIn.melodicStatus = MelodicMotifStatus::ValidButEmpty;
  emptyIn.admittedOnsets = 0;
  assert(!c2AOnsetBoundaryEligible(aOut, emptyIn, 2, true));
  std::puts("T7 ValidButEmpty rejected: OK");

  auto bOut = aOut;
  bOut.admittedOnsets = step14;
  assert(!c2AOnsetBoundaryEligible(bOut, aIn, 2, true));
  std::puts("T8 step14-or-earlier rejected: OK");

  auto hybridOut = aOut;
  hybridOut.role = SemanticSynthBRole::ChordWithMelodicFill;
  assert(!c2AOnsetBoundaryEligible(hybridOut, aIn, 2, true));
  auto chordIn = aIn;
  chordIn.role = SemanticSynthBRole::Chord;
  assert(!c2AOnsetBoundaryEligible(aOut, chordIn, 2, true));
  assert(!c2AOnsetBoundaryEligible(aOut, aIn, 2, false));
  std::puts("T9 hybrid/chord/different logical voice rejected: OK");

  auto continuationOut = aOut;
  continuationOut.admittedOnsets = step14;
  continuationOut.admittedContinuations = step15;
  assert(!c2AOnsetBoundaryEligible(continuationOut, aIn, 2, true));
  std::puts("T10 A-continuation rejected: OK");

  auto overlapOut = aOut;
  overlapOut.admittedContinuations = step15;
  assert(!c2AOnsetBoundaryEligible(overlapOut, aIn, 2, true));
  std::puts("T11 A-overlap rejected: OK");

  auto otherIdentity = aIn;
  otherIdentity.phraseGenerationIdentity = 8;
  assert(!c2AOnsetBoundaryEligible(aOut, otherIdentity, 2, true));
  auto nonSequential = aIn;
  nonSequential.phraseBarOrdinal = 2;
  assert(!c2AOnsetBoundaryEligible(aOut, nonSequential, 4, true));
  auto loopWrapOut = aOut;
  loopWrapOut.phraseBarOrdinal = 1;
  auto loopWrapIn = aIn;
  loopWrapIn.phraseBarOrdinal = 0;
  assert(!c2AOnsetBoundaryEligible(loopWrapOut, loopWrapIn, 2, true));
  assert(!c2AOnsetBoundaryEligible(aOut, aIn, 1, true));
  std::puts("T6-T11 identity/sequential/loop-wrap fail-closed: OK");
}

void testT12T13TerminalAndFirst() {
  const PreparedPhraseExecution prepared =
      prepareReady(genre(GenerativeMode::Broken, 9), 0, 8);
  assert(!prepared.semantic.bars[0].melodicLifetime.entersFromPreviousBar);
  assert(!prepared.semantic.bars[7].melodicLifetime.continuesIntoNextBar);
  std::puts("T12 last bar terminal: OK");
  std::puts("T13 first bar has no incoming lifetime: OK");
}

void testT14PairedCarrier() {
  assertPaired(prepareReady(GenreSettings{}, 2, 4));
  assertPaired(prepareReady(genre(GenerativeMode::Broken, 9), 0, 8));
  std::puts("T14 paired carrier invariant: OK");
}

void testT15RandomAccess() {
  const PreparedPhraseExecution prepared =
      prepareReady(genre(GenerativeMode::Broken, 9), 0, 8);
  const MelodicCrossBarLifetime before[8] = {
      prepared.semantic.bars[0].melodicLifetime,
      prepared.semantic.bars[1].melodicLifetime,
      prepared.semantic.bars[2].melodicLifetime,
      prepared.semantic.bars[3].melodicLifetime,
      prepared.semantic.bars[4].melodicLifetime,
      prepared.semantic.bars[5].melodicLifetime,
      prepared.semantic.bars[6].melodicLifetime,
      prepared.semantic.bars[7].melodicLifetime,
  };

  const PhysicalBar sevenA = materialize(prepared, 7, 107);
  const PhysicalBar zero = materialize(prepared, 0, 100);
  const PhysicalBar four = materialize(prepared, 4, 104);
  const PhysicalBar sevenB = materialize(prepared, 7, 207);
  (void)zero;
  (void)four;
  assert(samePhysical(sevenA, sevenB));
  for (uint8_t bar = 0; bar < 8; ++bar) {
    assert(prepared.semantic.bars[bar].melodicLifetime.entersFromPreviousBar ==
           before[bar].entersFromPreviousBar);
    assert(prepared.semantic.bars[bar].melodicLifetime.continuesIntoNextBar ==
           before[bar].continuesIntoNextBar);
  }
  std::puts("T15 random access 7->0->4->7 deterministic: OK");
}

void testT16PatternAddressFirewall() {
  const PreparedPhraseExecution prepared = prepareReady(GenreSettings{}, 2, 2);
  const PhysicalBar a = materialize(prepared, 0, 10);
  const PhysicalBar b = materialize(prepared, 0, 200);
  assert(samePhysical(a, b));
  assert(prepared.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  std::puts("T16 patternAddress firewall: OK");
}

void testT17MaterialInvariance() {
  const PreparedPhraseExecution withLifetime =
      prepareReady(GenreSettings{}, 2, 4);
  PreparedPhraseExecution withoutLifetime = withLifetime;
  for (uint8_t bar = 0; bar < withoutLifetime.length.effectivePhraseBars; ++bar) {
    withoutLifetime.semantic.bars[bar].melodicLifetime =
        MelodicCrossBarLifetime{};
  }

  for (uint8_t bar = 0; bar < withLifetime.length.effectivePhraseBars; ++bar) {
    const PhysicalBar with = materialize(withLifetime, bar, 40 + bar);
    const PhysicalBar without = materialize(withoutLifetime, bar, 40 + bar);
    assert(samePhysical(with, without));
  }
  assert(withLifetime.selection.composition.progression ==
         withoutLifetime.selection.composition.progression);
  assert(withLifetime.progressionSource.id == withoutLifetime.progressionSource.id);
  assert(withLifetime.progressionSource.period ==
         withoutLifetime.progressionSource.period);
  assert(withLifetime.harmonicClock.timeline.totalEventPositions ==
         withoutLifetime.harmonicClock.timeline.totalEventPositions);
  std::puts("T17 lifetime-only material invariance: OK");
}

void testT20Memory() {
  static_assert(sizeof(PreparedPhraseExecution) == 324,
                "C2 must not grow PreparedPhraseExecution");
  static_assert(sizeof(PhraseSemanticResult) == 82,
                "C2 must not grow PhraseSemanticResult");
  static_assert(sizeof(StrongRhythmFrozenSelection) == 48,
                "C2 must not grow frozen selection");
  static_assert(sizeof(ChordProgressionSource) == 14,
                "C2 must not grow progression source");
  static_assert(sizeof(SynthPattern) == 112,
                "C2 must not grow SynthPattern");
  static_assert(sizeof(DrumPatternSet) == 1192,
                "C2 must not grow DrumPatternSet");
  static_assert(sizeof(PhraseMelodicBoundaryObservation) <= 12,
                "C2 transient observation unexpectedly large");

  std::printf(
      "C2 MEMORY PreparedPhraseExecution=%zu PhraseSemanticResult=%zu "
      "StrongRhythmFrozenSelection=%zu ChordProgressionSource=%zu "
      "SynthPattern=%zu DrumPatternSet=%zu BoundaryObservation=%zu\n",
      sizeof(PreparedPhraseExecution), sizeof(PhraseSemanticResult),
      sizeof(StrongRhythmFrozenSelection), sizeof(ChordProgressionSource),
      sizeof(SynthPattern), sizeof(DrumPatternSet),
      sizeof(PhraseMelodicBoundaryObservation));
  std::puts("T20 memory: OK");
}

}  // namespace

int main() {
  testT2KnownAOnsetPositive();
  testT3ProductionDefaultPositive();
  testT4LengthCoverage();
  testT5EvolutionSeam();
  testT6ToT11ClassifierNegatives();
  testT12T13TerminalAndFirst();
  testT14PairedCarrier();
  testT15RandomAccess();
  testT16PatternAddressFirewall();
  testT17MaterialInvariance();
  testT20Memory();
  std::puts("PHRASE-C2 focused A-onset lifetime producer: OK");
  return 0;
}
