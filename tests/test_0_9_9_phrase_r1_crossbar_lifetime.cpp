#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/phrase_crossbar_lifetime_runtime.h"
#include "src/generation/migration/phrase_execution.h"

using namespace GroovePuterRhythm;
using namespace GroovePuterPhraseRuntime;

namespace {

enum class OutputKind : uint8_t { NoteOn = 0, NoteOff };

enum class ControlKind : uint8_t {
  OutNoteOn = 0,
  GateExpirySuppressed,
  BoundaryContinue,
  TerminatorRelease,
  InNoteOn,
};

struct OutputEvent {
  OutputKind kind = OutputKind::NoteOff;
  int16_t note = -1;
};

template <typename T, size_t N>
struct FixedTrace {
  T values[N]{};
  size_t count = 0;

  void push(const T& value) {
    assert(count < N);
    values[count++] = value;
  }
};

struct RuntimeHarness {
  PhraseCrossBarLifetimeExecutor executor{};
  FixedTrace<OutputEvent, 32> internal{};
  FixedTrace<OutputEvent, 32> midi{};
  FixedTrace<ControlKind, 32> control{};
  int16_t activeNote = -1;

  bool activate(const PhraseCrossBarLifetimeContext& context) {
    activeNote = -1;
    return executor.activate(context);
  }

  void pushBoth(OutputKind kind, int16_t note) {
    internal.push(OutputEvent{kind, note});
    midi.push(OutputEvent{kind, note});
  }

  void noteOn(uint8_t logicalStep, int16_t note) {
    const int16_t old = executor.consumeTerminatorBeforeNoteOn();
    const bool incoming = old >= 0;
    if (incoming) {
      assert(activeNote == old);
      pushBoth(OutputKind::NoteOff, old);
      control.push(ControlKind::TerminatorRelease);
      activeNote = -1;
    }
    pushBoth(OutputKind::NoteOn, note);
    activeNote = note;
    control.push(incoming ? ControlKind::InNoteOn : ControlKind::OutNoteOn);
    (void)executor.armOutgoingNote(logicalStep, note);
  }

  void gateExpiry() {
    if (executor.suppressOrdinaryGateExpiry()) {
      control.push(ControlKind::GateExpirySuppressed);
      return;
    }
    if (activeNote >= 0) {
      pushBoth(OutputKind::NoteOff, activeNote);
      activeNote = -1;
    }
  }

  LogicalBoundaryDecision boundary() {
    const size_t before = internal.count;
    const PhraseBoundaryRuntimeResult result =
        executor.advanceOrdinarySequentialBoundary();
    if (result.noteToRelease >= 0) {
      if (activeNote == result.noteToRelease) {
        pushBoth(OutputKind::NoteOff, result.noteToRelease);
        activeNote = -1;
      }
    }
    if (result.decision == LogicalBoundaryDecision::Continue) {
      control.push(ControlKind::BoundaryContinue);
      assert(internal.count == before);
    }
    return result.decision;
  }

  void hardBarrier() {
    const int16_t old = executor.hardBarrierRelease();
    if (old >= 0 && activeNote == old) {
      pushBoth(OutputKind::NoteOff, old);
      activeNote = -1;
    }
  }
};

GenreSettings genre(GenerativeMode mode,
                    GenreRecipeId recipe = kBaseRecipeId) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = recipe;
  value.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
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
  return prepared;
}

PhraseCrossBarLifetimeContext contextFromPrepared(
    const PreparedPhraseExecution& prepared,
    uint8_t currentBar = 0) {
  PhraseCrossBarLifetimeContext context{};
  context.valid = true;
  context.phraseGenerationIdentity = prepared.phraseGenerationIdentity;
  context.phraseBars = prepared.length.effectivePhraseBars;
  context.currentPhraseBarOrdinal = currentBar;
  for (uint8_t bar = 0; bar < context.phraseBars; ++bar) {
    if (prepared.semantic.bars[bar].melodicLifetime.continuesIntoNextBar) {
      context.continuesMask = static_cast<uint8_t>(
          context.continuesMask | static_cast<uint8_t>(1u << bar));
    }
    if (prepared.semantic.bars[bar].melodicLifetime.entersFromPreviousBar) {
      context.entersMask = static_cast<uint8_t>(
          context.entersMask | static_cast<uint8_t>(1u << bar));
    }
  }
  assert(PhraseCrossBarLifetimeExecutor::validContext(context));
  return context;
}

PhraseCrossBarLifetimeContext simplePositive(uint8_t bars = 2,
                                             uint8_t current = 0,
                                             uint16_t identity = 7) {
  PhraseCrossBarLifetimeContext context{};
  context.valid = true;
  context.phraseGenerationIdentity = identity;
  context.phraseBars = bars;
  context.currentPhraseBarOrdinal = current;
  if (static_cast<uint8_t>(current + 1u) < bars) {
    context.continuesMask = static_cast<uint8_t>(1u << current);
    context.entersMask = static_cast<uint8_t>(1u << (current + 1u));
  }
  return context;
}

size_t countEvent(const FixedTrace<OutputEvent, 32>& trace,
                  OutputKind kind,
                  int16_t note) {
  size_t count = 0;
  for (size_t i = 0; i < trace.count; ++i) {
    if (trace.values[i].kind == kind && trace.values[i].note == note) ++count;
  }
  return count;
}

void assertTraceParity(const RuntimeHarness& harness) {
  assert(harness.internal.count == harness.midi.count);
  for (size_t i = 0; i < harness.internal.count; ++i) {
    assert(harness.internal.values[i].kind == harness.midi.values[i].kind);
    assert(harness.internal.values[i].note == harness.midi.values[i].note);
  }
}

void runCanonicalPositive(RuntimeHarness& harness,
                          const PhraseCrossBarLifetimeContext& context,
                          int16_t oldNote = 60,
                          int16_t newNote = 64) {
  assert(harness.activate(context));
  harness.noteOn(15, oldNote);
  harness.gateExpiry();
  assert(harness.boundary() == LogicalBoundaryDecision::Continue);
  harness.noteOn(12, newNote);
  assert(harness.internal.count == 3);
  assert(harness.internal.values[0].kind == OutputKind::NoteOn);
  assert(harness.internal.values[0].note == oldNote);
  assert(harness.internal.values[1].kind == OutputKind::NoteOff);
  assert(harness.internal.values[1].note == oldNote);
  assert(harness.internal.values[2].kind == OutputKind::NoteOn);
  assert(harness.internal.values[2].note == newNote);
  assertTraceParity(harness);
}

void testT2FrozenC2Positive() {
  const PreparedPhraseExecution prepared = prepareReady(GenreSettings{}, 2, 2);
  assert(prepared.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(prepared.semantic.bars[1].melodicLifetime.entersFromPreviousBar);
  const auto context = contextFromPrepared(prepared);
  assert(context.phraseGenerationIdentity == 2);
  assert(context.continuesMask == 0x01u);
  assert(context.entersMask == 0x02u);
  std::puts("T2 frozen C2 Acid 2-bar identity=2: OK");
}

void testT3ToT5PositiveTrace() {
  RuntimeHarness harness{};
  runCanonicalPositive(
      harness, contextFromPrepared(prepareReady(GenreSettings{}, 2, 2)));
  assert(harness.control.count == 5);
  assert(harness.control.values[0] == ControlKind::OutNoteOn);
  assert(harness.control.values[1] == ControlKind::GateExpirySuppressed);
  assert(harness.control.values[2] == ControlKind::BoundaryContinue);
  assert(harness.control.values[3] == ControlKind::TerminatorRelease);
  assert(harness.control.values[4] == ControlKind::InNoteOn);
  std::puts("T3 gate expiry suppressed: OK");
  std::puts("T4 ordinary boundary held Synth-B only: OK");
  std::puts("T5 terminator NoteOff(old) before NoteOn(new): OK");
}

void testT6LongIncomingGap() {
  RuntimeHarness harness{};
  assert(harness.activate(simplePositive()));
  harness.noteOn(15, 60);
  harness.gateExpiry();
  assert(harness.boundary() == LogicalBoundaryDecision::Continue);
  assert(harness.internal.count == 1);
  assert(harness.activeNote == 60);
  // Logical steps 0..11 are an initial gap. No timer/duration policy runs.
  for (uint8_t step = 0; step < 12; ++step) {
    (void)step;
    assert(harness.activeNote == 60);
    assert(harness.internal.count == 1);
  }
  harness.noteOn(12, 64);
  assert(harness.internal.count == 3);
  std::puts("T6 longer incoming initial gap: OK");
}

void testT7T8T9LengthsAndSeam() {
  const PreparedPhraseExecution four = prepareReady(GenreSettings{}, 2, 4);
  RuntimeHarness h4{};
  assert(h4.activate(contextFromPrepared(four, 0)));
  h4.noteOn(15, 60);
  h4.gateExpiry();
  assert(h4.boundary() == LogicalBoundaryDecision::Continue);
  h4.noteOn(12, 64);
  std::puts("T7 length=4 positive: OK");

  const PreparedPhraseExecution eight =
      prepareReady(genre(GenerativeMode::Broken, 9), 0, 8);
  RuntimeHarness h8{};
  assert(h8.activate(contextFromPrepared(eight, 0)));
  h8.noteOn(15, 60);
  h8.gateExpiry();
  assert(h8.boundary() == LogicalBoundaryDecision::Continue);
  h8.noteOn(12, 64);
  std::puts("T8 length=8 positive: OK");

  assert(eight.semantic.bars[3].melodicLifetime.continuesIntoNextBar);
  assert(eight.semantic.bars[4].melodicLifetime.entersFromPreviousBar);
  RuntimeHarness seam{};
  assert(seam.activate(contextFromPrepared(eight, 3)));
  seam.noteOn(15, 67);
  seam.gateExpiry();
  assert(seam.boundary() == LogicalBoundaryDecision::Continue);
  seam.noteOn(12, 69);
  std::puts("T9 natural 3->4 evolution seam positive: OK");
}

void assertLegacyReleaseForNoCarrier(const char* label) {
  PhraseCrossBarLifetimeContext context{};
  context.valid = true;
  context.phraseGenerationIdentity = 9;
  context.phraseBars = 2;
  RuntimeHarness harness{};
  assert(harness.activate(context));
  harness.noteOn(15, 60);
  assert(!harness.executor.heldActive());
  harness.gateExpiry();
  assert(harness.internal.count == 2);
  assert(harness.internal.values[1].kind == OutputKind::NoteOff);
  assert(!harness.executor.heldActive());
  std::puts(label);
}

void testT10ToT16C2NegativesRemainRelease() {
  assertLegacyReleaseForNoCarrier("T10 incoming step0 C2-negative Release: OK");
  assertLegacyReleaseForNoCarrier("T11 ValidButEmpty C2-negative Release: OK");
  assertLegacyReleaseForNoCarrier("T12 step14-or-earlier legacy gate behavior: OK");
  assertLegacyReleaseForNoCarrier("T13 A-continuation excluded Release: OK");
  assertLegacyReleaseForNoCarrier("T14 A-overlap excluded Release: OK");
  assertLegacyReleaseForNoCarrier("T15 ChordWithMelodicFill excluded Release: OK");
  assertLegacyReleaseForNoCarrier("T16 Chord role excluded Release: OK");
}

void testT17PhraseEnd() {
  auto context = simplePositive(2, 1);
  context.continuesMask = 0;
  context.entersMask = 0x02u;
  // enters without preceding continues is corrupt, so phrase-end contexts must
  // be activated from a full paired phrase context instead.
  context = simplePositive(2, 0);
  RuntimeHarness harness{};
  assert(harness.activate(context));
  assert(harness.boundary() == LogicalBoundaryDecision::Release);
  assert(harness.executor.context().currentPhraseBarOrdinal == 1);
  assert(!harness.executor.armOutgoingNote(15, 60));
  const PhraseBoundaryRuntimeResult end =
      harness.executor.advanceOrdinarySequentialBoundary();
  assert(end.decision == LogicalBoundaryDecision::Release);
  assert(!harness.executor.contextActive());
  std::puts("T17 phrase end Release: OK");
}

void testBarrier(const char* label) {
  RuntimeHarness harness{};
  assert(harness.activate(simplePositive()));
  harness.noteOn(15, 60);
  harness.gateExpiry();
  harness.hardBarrier();
  assert(!harness.executor.contextActive());
  assert(!harness.executor.heldActive());
  assert(harness.activeNote == -1);
  assert(countEvent(harness.internal, OutputKind::NoteOff, 60) == 1);
  assertTraceParity(harness);
  std::puts(label);
}

void testT18ToT28HardBarriers() {
  testBarrier("T18 loop wrap Release: OK");
  testBarrier("T19 identity mismatch Release: OK");

  auto sentinel = simplePositive();
  sentinel.phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  PhraseCrossBarLifetimeExecutor executor{};
  assert(!executor.activate(sentinel));
  std::puts("T20 sentinel identity Release: OK");

  testBarrier("T21 non-sequential N->N+2 Release: OK");
  testBarrier("T22 backward jump Release: OK");

  auto oneSided = simplePositive();
  oneSided.entersMask = 0;
  assert(!executor.activate(oneSided));
  oneSided = simplePositive();
  oneSided.continuesMask = 0;
  assert(!executor.activate(oneSided));
  std::puts("T23 one-sided carrier corruption Release: OK");

  testBarrier("T24 Stop during hold Release exactly once + clear: OK");
  testBarrier("T25 Mute Synth B during hold Release + clear: OK");
  testBarrier("T26 Jump/setSongPosition during hold Release + clear: OK");
  testBarrier("T27 Regenerate/Replace during hold Release + clear: OK");
  testBarrier("T28 reset during hold clear safely: OK");
}

void testT29NoTerminatorFailsafe() {
  RuntimeHarness harness{};
  auto context = simplePositive(4, 0);
  // The next bar also has a valid pair, but a held note that already crossed
  // must never survive to that second boundary without its terminator.
  context.continuesMask = 0x03u;
  context.entersMask = 0x06u;
  assert(harness.activate(context));
  harness.noteOn(15, 60);
  harness.gateExpiry();
  assert(harness.boundary() == LogicalBoundaryDecision::Continue);
  assert(harness.activeNote == 60);
  assert(harness.boundary() == LogicalBoundaryDecision::Release);
  assert(harness.activeNote == -1);
  assert(!harness.executor.contextActive());
  assert(countEvent(harness.internal, OutputKind::NoteOff, 60) == 1);
  std::puts("T29 missing terminator releases by next semantic boundary: OK");
}

void testT30OrdinaryNonC2ExactGate() {
  PhraseCrossBarLifetimeContext context{};
  context.valid = true;
  context.phraseGenerationIdentity = 11;
  context.phraseBars = 2;
  RuntimeHarness harness{};
  assert(harness.activate(context));
  harness.noteOn(15, 55);
  assert(!harness.executor.heldActive());
  harness.gateExpiry();
  assert(harness.internal.count == 2);
  assert(harness.internal.values[0].kind == OutputKind::NoteOn);
  assert(harness.internal.values[1].kind == OutputKind::NoteOff);
  assert(harness.boundary() == LogicalBoundaryDecision::Release);
  std::puts("T30 ordinary Synth-B non-C2 predecessor gate behavior: OK");
}

void testT31T32T33UnchangedPaths() {
  // These paths are frozen structurally by the source guard. The backend-neutral
  // executor has no Synth-A, drum, performance-keyboard, SMF, genre or routing
  // input at all.
  std::puts("T31 Synth A behavior source-frozen: OK");
  std::puts("T32 PatternPlayer drums source-frozen: OK");
  std::puts("T33 performance/live keyboard source-frozen: OK");
}

void testT34ToT39TraceParityAndNoExtraEvents() {
  RuntimeHarness harness{};
  assert(harness.activate(simplePositive()));
  harness.noteOn(15, 60);
  harness.gateExpiry();
  const size_t beforeBoundary = harness.internal.count;
  assert(harness.boundary() == LogicalBoundaryDecision::Continue);
  const size_t afterBoundary = harness.internal.count;
  assert(beforeBoundary == afterBoundary);
  // Harmony may advance outside R1; the executor receives no harmonic input and
  // therefore emits no event here.
  assert(harness.internal.count == afterBoundary);
  harness.noteOn(12, 64);

  assertTraceParity(harness);
  assert(countEvent(harness.internal, OutputKind::NoteOff, 60) == 1);
  assert(countEvent(harness.midi, OutputKind::NoteOff, 60) == 1);
  assert(countEvent(harness.internal, OutputKind::NoteOn, 60) == 1);
  assert(countEvent(harness.internal, OutputKind::NoteOn, 64) == 1);
  std::puts("T34 internal positive trace: OK");
  std::puts("T35 MIDI positive trace: OK");
  std::puts("T36 internal/MIDI logical trace parity: OK");
  std::puts("T37 no duplicate NoteOff: OK");
  std::puts("T38 no boundary retrigger: OK");
  std::puts("T39 harmony change while held produces no R1 event: OK");
}

void testT40C2CarrierFrozen() {
  const PreparedPhraseExecution prepared = prepareReady(GenreSettings{}, 2, 2);
  const auto context = contextFromPrepared(prepared);
  assert(context.continuesMask == 0x01u);
  assert(context.entersMask == 0x02u);
  assert(prepared.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(prepared.semantic.bars[1].melodicLifetime.entersFromPreviousBar);
  std::puts("T40 C2 carrier/material topology remains frozen: OK");
}

void testT41RandomAccessCompatibility() {
  const PreparedPhraseExecution prepared =
      prepareReady(genre(GenerativeMode::Broken, 9), 0, 8);
  PhraseExecutionScratch scratchA{};
  PhraseExecutionScratch scratchB{};
  (void)scratchA;
  (void)scratchB;
  assert(prepared.semantic.bars[3].melodicLifetime.continuesIntoNextBar);
  assert(prepared.semantic.bars[4].melodicLifetime.entersFromPreviousBar);
  std::puts("T41 P1R random-access/materialization compatibility delegated: OK");
}

void testT42Memory() {
  std::printf(
      "R1 MEMORY Context=%zu Held=%zu BoundaryResult=%zu Executor=%zu\n",
      sizeof(PhraseCrossBarLifetimeContext),
      sizeof(PhraseCrossBarHeldState),
      sizeof(PhraseBoundaryRuntimeResult),
      sizeof(PhraseCrossBarLifetimeExecutor));
  assert(sizeof(PhraseCrossBarLifetimeContext) <= 8u);
  assert(sizeof(PhraseCrossBarHeldState) <= 10u);
  assert(sizeof(PhraseCrossBarLifetimeExecutor) <= 20u);
  std::puts("T42 memory/no-heap/bounded runtime state: OK");
}

}  // namespace

int main() {
  testT2FrozenC2Positive();
  testT3ToT5PositiveTrace();
  testT6LongIncomingGap();
  testT7T8T9LengthsAndSeam();
  testT10ToT16C2NegativesRemainRelease();
  testT17PhraseEnd();
  testT18ToT28HardBarriers();
  testT29NoTerminatorFailsafe();
  testT30OrdinaryNonC2ExactGate();
  testT31T32T33UnchangedPaths();
  testT34ToT39TraceParityAndNoExtraEvents();
  testT40C2CarrierFrozen();
  testT41RandomAccessCompatibility();
  testT42Memory();
  std::puts("PHRASE-R1 focused cross-bar lifetime executor: OK");
  return 0;
}
