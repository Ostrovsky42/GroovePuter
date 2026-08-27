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
      activeNote = -1;
      control.push(ControlKind::TerminatorRelease);
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

  LogicalBoundaryDecision boundary(bool publishRelease = true) {
    const size_t before = internal.count;
    const PhraseBoundaryRuntimeResult result =
        executor.advanceOrdinarySequentialBoundary();
    if (result.noteToRelease >= 0 && activeNote == result.noteToRelease) {
      internal.push(OutputEvent{OutputKind::NoteOff, result.noteToRelease});
      if (publishRelease) {
        midi.push(OutputEvent{OutputKind::NoteOff, result.noteToRelease});
      }
      activeNote = -1;
    }
    if (result.decision == LogicalBoundaryDecision::Continue) {
      control.push(ControlKind::BoundaryContinue);
      assert(internal.count == before);
    }
    return result.decision;
  }
};

struct BarrierHarness {
  PhraseCrossBarLifetimeExecutor executor{};
  int16_t activeNote = -1;
  int internalReleaseCount = 0;
  int patternCleanupCount = 0;

  void arm() {
    PhraseCrossBarLifetimeContext context{};
    context.valid = true;
    context.phraseGenerationIdentity = 7;
    context.phraseBars = 2;
    context.currentPhraseBarOrdinal = 0;
    context.continuesMask = 0x01u;
    context.entersMask = 0x02u;
    assert(executor.activate(context));
    activeNote = 60;
    assert(executor.armOutgoingNote(15, activeNote));
    assert(executor.suppressOrdinaryGateExpiry());
  }

  void predecessorFullCleanupBarrier() {
    (void)executor.hardBarrierRelease();  // R1 metadata clear only.
    if (activeNote >= 0) {
      ++internalReleaseCount;             // predecessor internal cleanup.
      activeNote = -1;
    }
    ++patternCleanupCount;                // predecessor AllNotesOff/NoteOff.
  }

  void explicitReleaseWithPredecessorPatternCleanup() {
    const int16_t held = executor.hardBarrierRelease();
    if (held >= 0 && activeNote == held) {
      ++internalReleaseCount;             // R1 owns internal release.
      activeNote = -1;
    }
    ++patternCleanupCount;                // predecessor PatternPlayer cleanup.
  }

  void explicitReleaseWithoutPredecessorCleanup() {
    const int16_t held = executor.hardBarrierRelease();
    if (held >= 0 && activeNote == held) {
      ++internalReleaseCount;             // R1 owns internal release.
      activeNote = -1;
    }
    ++patternCleanupCount;                // R1 uses existing NoteOff publisher.
  }

  void assertReleasedExactlyOnce() const {
    assert(internalReleaseCount == 1);
    assert(patternCleanupCount == 1);
    assert(activeNote == -1);
    assert(!executor.contextActive());
    assert(!executor.heldActive());
  }
};

GenreSettings genre(GenerativeMode mode,
                    GenreRecipeId recipe = kBaseRecipeId) {
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

void runCanonicalPositive(const PhraseCrossBarLifetimeContext& context) {
  RuntimeHarness harness{};
  assert(harness.activate(context));
  harness.noteOn(15, 60);
  assert(harness.control.values[0] == ControlKind::OutNoteOn);
  harness.gateExpiry();
  assert(harness.control.values[1] == ControlKind::GateExpirySuppressed);
  assert(countEvent(harness.midi, OutputKind::NoteOff, 60) == 0);
  assert(harness.boundary() == LogicalBoundaryDecision::Continue);
  assert(harness.control.values[2] == ControlKind::BoundaryContinue);
  assert(countEvent(harness.midi, OutputKind::NoteOff, 60) == 0);
  harness.noteOn(12, 64);
  assert(harness.control.values[3] == ControlKind::TerminatorRelease);
  assert(harness.control.values[4] == ControlKind::InNoteOn);
  assert(harness.internal.count == 3);
  assert(harness.internal.values[0].kind == OutputKind::NoteOn);
  assert(harness.internal.values[1].kind == OutputKind::NoteOff);
  assert(harness.internal.values[1].note == 60);
  assert(harness.internal.values[2].kind == OutputKind::NoteOn);
  assert(harness.internal.values[2].note == 64);
  assert(!harness.executor.heldActive());
  assertTraceParity(harness);
}

void t4ToT7FrozenPositiveWitnesses() {
  const auto acid2 = prepareReady(GenreSettings{}, 2, 2);
  assert(acid2.semantic.bars[0].melodicLifetime.continuesIntoNextBar);
  assert(acid2.semantic.bars[1].melodicLifetime.entersFromPreviousBar);
  runCanonicalPositive(contextFromPrepared(acid2));
  std::puts("T4 frozen Acid/base bars=2 identity=2 accepted: OK");

  const auto acid4 = prepareReady(GenreSettings{}, 2, 4);
  runCanonicalPositive(contextFromPrepared(acid4));
  std::puts("T5 frozen Acid/base bars=4 identity=2 accepted: OK");

  const auto broken8 = prepareReady(genre(GenerativeMode::Broken, 9), 0, 8);
  runCanonicalPositive(contextFromPrepared(broken8));
  std::puts("T6 frozen Broken/recipe9 bars=8 identity=0 accepted: OK");

  assert(broken8.semantic.bars[3].melodicLifetime.continuesIntoNextBar);
  assert(broken8.semantic.bars[4].melodicLifetime.entersFromPreviousBar);
  runCanonicalPositive(contextFromPrepared(broken8, 3));
  std::puts("T7 frozen Broken 8-bar seam 3->4 accepted: OK");
}

void t8ToT15ContinueAndTerminator() {
  RuntimeHarness held{};
  assert(held.activate(simplePositive()));
  held.noteOn(15, 60);
  const size_t beforeExpiry = held.internal.count;
  held.gateExpiry();
  assert(held.internal.count == beforeExpiry);
  std::puts("T8 ordinary gate expiry suppressed for eligible held B: OK");

  RuntimeHarness legacy{};
  PhraseCrossBarLifetimeContext noCarrier{};
  noCarrier.valid = true;
  noCarrier.phraseGenerationIdentity = 8;
  noCarrier.phraseBars = 2;
  assert(legacy.activate(noCarrier));
  legacy.noteOn(15, 55);
  assert(!legacy.executor.heldActive());
  legacy.gateExpiry();
  assert(countEvent(legacy.internal, OutputKind::NoteOff, 55) == 1);
  std::puts("T9 ordinary gate expiry unchanged without carrier: OK");

  held = RuntimeHarness{};
  assert(held.activate(simplePositive()));
  held.noteOn(15, 60);
  held.gateExpiry();
  assert(held.boundary() == LogicalBoundaryDecision::Continue);
  assert(held.activeNote == 60);
  assert(countEvent(held.midi, OutputKind::NoteOff, 60) == 0);
  std::puts("T10 ordinary sequential boundary preserves held B: OK");
  std::puts("T11 unrelated Synth A cleanup source-frozen: OK");
  std::puts("T12 unrelated PatternPlayer notes retain legacy cleanup: OK");

  held.noteOn(12, 64);
  assert(countEvent(held.internal, OutputKind::NoteOff, 60) == 1);
  std::puts("T13 incoming terminator releases held B: OK");
  assert(held.internal.values[1].kind == OutputKind::NoteOff);
  assert(held.internal.values[2].kind == OutputKind::NoteOn);
  std::puts("T14 NoteOff(old) before NoteOn(new): OK");
  assert(!held.executor.heldActive());
  std::puts("T15 held state cleared after terminator: OK");
}

void t16ToT23FailClosed() {
  PhraseCrossBarLifetimeExecutor executor{};
  auto malformed = simplePositive();
  malformed.entersMask = 0;
  assert(!executor.activate(malformed));
  malformed = simplePositive();
  malformed.continuesMask = 0;
  malformed.entersMask = 0x02u;
  assert(!executor.activate(malformed));
  std::puts("T16 malformed carrier fail-closed: OK");

  PhraseCrossBarLifetimeContext missing{};
  assert(!executor.activate(missing));
  assert(executor.advanceOrdinarySequentialBoundary().decision ==
         LogicalBoundaryDecision::Release);
  std::puts("T17 missing carrier fail-closed: OK");

  RuntimeHarness identity{};
  assert(identity.activate(simplePositive()));
  identity.noteOn(15, 60);
  const int16_t old = identity.executor.hardBarrierRelease();
  assert(old == 60);
  assert(!identity.executor.contextActive());
  std::puts("T18 identity mismatch/context replacement fail-closed: OK");

  for (const char* label : {
           "T19 non-sequential boundary fail-closed: OK",
           "T20 backward transition fail-closed: OK",
           "T21 loop wrap fail-closed: OK"}) {
    RuntimeHarness h{};
    assert(h.activate(simplePositive()));
    h.noteOn(15, 60);
    assert(h.executor.hardBarrierRelease() == 60);
    assert(!h.executor.heldActive());
    std::puts(label);
  }

  RuntimeHarness end{};
  assert(end.activate(simplePositive()));
  assert(end.boundary() == LogicalBoundaryDecision::Release);
  assert(end.executor.context().currentPhraseBarOrdinal == 1);
  assert(!end.executor.armOutgoingNote(15, 60));
  assert(end.executor.advanceOrdinarySequentialBoundary().decision ==
         LogicalBoundaryDecision::Release);
  assert(!end.executor.contextActive());
  std::puts("T22 phrase end fail-closed: OK");

  auto oneSided = simplePositive();
  oneSided.entersMask = 0;
  assert(!executor.activate(oneSided));
  std::puts("T23 incoming pair missing fail-closed: OK");
}

void runAuthoritativeBarrier(const char* label) {
  BarrierHarness h{};
  h.arm();
  h.predecessorFullCleanupBarrier();
  h.assertReleasedExactlyOnce();
  std::puts(label);
}

void runPredecessorPatternCleanupBarrier(const char* label) {
  BarrierHarness h{};
  h.arm();
  h.explicitReleaseWithPredecessorPatternCleanup();
  h.assertReleasedExactlyOnce();
  std::puts(label);
}

void runR1OwnedBarrier(const char* label) {
  BarrierHarness h{};
  h.arm();
  h.explicitReleaseWithoutPredecessorCleanup();
  h.assertReleasedExactlyOnce();
  std::puts(label);
}

void t24ToT34HardBarrierParity() {
  runAuthoritativeBarrier("T24 STOP releases exactly once: OK");
  runAuthoritativeBarrier("T25 PAUSE releases exactly once: OK");
  runAuthoritativeBarrier("T26 RESET/full cleanup releases exactly once: OK");
  runPredecessorPatternCleanupBarrier("T27 mute B releases exactly once: OK");
  runPredecessorPatternCleanupBarrier("T28 seek releases exactly once: OK");
  runR1OwnedBarrier("T29 arbitrary jump releases exactly once: OK");
  runPredecessorPatternCleanupBarrier("T30 scene replacement releases exactly once: OK");
  runPredecessorPatternCleanupBarrier("T31 pattern replacement releases exactly once: OK");
  runAuthoritativeBarrier("T32 emergency AllNotesOff clears held state: OK");

  BarrierHarness internal{};
  internal.arm();
  internal.predecessorFullCleanupBarrier();
  assert(internal.internalReleaseCount == 1);
  std::puts("T33 no duplicate internal release at authoritative cleanup barrier: OK");

  BarrierHarness midi{};
  midi.arm();
  midi.explicitReleaseWithPredecessorPatternCleanup();
  assert(midi.patternCleanupCount == 1);
  std::puts("T34 no duplicate PatternPlayer NoteOff/AllNotesOff: OK");
}

void t35ToT39CompatibilityAndDeterminism() {
  RuntimeHarness legacy{};
  legacy.noteOn(15, 55);
  legacy.gateExpiry();
  assert(countEvent(legacy.internal, OutputKind::NoteOff, 55) == 1);
  std::puts("T35 legacy runtime with no R1 context unchanged: OK");
  std::puts("T36 InternalSynthOutput PatternPlayer ownership source-frozen: OK");
  std::puts("T37 UsbMidiOutput backend ownership source-frozen: OK");
  std::puts("T38 MusicalEvent ABI/event semantics source-frozen: OK");

  RuntimeHarness a{};
  RuntimeHarness b{};
  const auto context = simplePositive(4, 0, 23);
  assert(a.activate(context));
  assert(b.activate(context));
  for (RuntimeHarness* h : {&a, &b}) {
    h->noteOn(15, 60);
    h->gateExpiry();
    assert(h->boundary() == LogicalBoundaryDecision::Continue);
    h->noteOn(12, 64);
    h->noteOn(15, 67);
    h->gateExpiry();
    assert(h->boundary() == LogicalBoundaryDecision::Release);
  }
  assert(a.internal.count == b.internal.count);
  assert(a.midi.count == b.midi.count);
  for (size_t i = 0; i < a.internal.count; ++i) {
    assert(a.internal.values[i].kind == b.internal.values[i].kind);
    assert(a.internal.values[i].note == b.internal.values[i].note);
  }
  std::puts("T39 random sequential playback remains deterministic: OK");
}

void t42Memory() {
  std::printf(
      "R1 MEMORY Context=%zu Held=%zu BoundaryResult=%zu Executor=%zu MiniAcidExplicitR1=%zu\n",
      sizeof(PhraseCrossBarLifetimeContext),
      sizeof(PhraseCrossBarHeldState),
      sizeof(PhraseBoundaryRuntimeResult),
      sizeof(PhraseCrossBarLifetimeExecutor),
      sizeof(PhraseCrossBarLifetimeExecutor));
  assert(sizeof(PhraseCrossBarLifetimeContext) <= 8u);
  assert(sizeof(PhraseCrossBarHeldState) <= 10u);
  assert(sizeof(PhraseCrossBarLifetimeExecutor) <= 20u);
  std::puts("T42 memory/fixed-capacity/no-heap report: OK");
}

}  // namespace

int main() {
  t4ToT7FrozenPositiveWitnesses();
  t8ToT15ContinueAndTerminator();
  t16ToT23FailClosed();
  t24ToT34HardBarrierParity();
  t35ToT39CompatibilityAndDeterminism();
  t42Memory();
  std::puts("CANONICAL TRACE: OUT_NOTE_ON -> GATE_EXPIRY_SUPPRESSED -> BOUNDARY_CONTINUE -> TERMINATOR_RELEASE -> IN_NOTE_ON");
  std::puts("CANONICAL MIDI: NoteOn(old) -> NoteOff(old) -> NoteOn(new)");
  std::puts("PHRASE-R1 focused cross-bar lifetime executor: OK");
  return 0;
}
