#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"

SerialMock Serial;
SDMock SD;

namespace {
int g_failures = 0;

void expect(bool condition, const char* name, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "%s FAIL: %s\n", name, message);
  ++g_failures;
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

MusicalEventTarget targetForSynth(int synth) {
  return synth == 0 ? MusicalEventTarget::SynthA : MusicalEventTarget::SynthB;
}

struct Trace {
  std::vector<MusicalEvent> events;

  std::size_t count(MusicalEventType type, MusicalEventTarget target) const {
    return static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [&](const MusicalEvent& event) {
          return event.type == type && event.target == target &&
                 event.source == MusicalEventSource::PatternPlayer;
        }));
  }
};

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};
  uint32_t blockSequence{1};

  Fixture() {
    engine.setBpm(120.0f);
    if (!engine.rebuildPatternRuntimeEventBank()) std::abort();
    engine.setCurrentPage(static_cast<int8_t>(PatternPagingService::activePageIndex()));
    engine.setPatternEventQueue(&queue);
    queue.setPhaseReader(readEnginePhase, &engine);
  }

  bool noteHeld(int synth) const {
    const SwappableSynthVoice* voice = engine.synthVoices_[synth].get();
    return voice != nullptr && voice->noteHeld_;
  }

  void beginBlock() {
    queue.beginMidiRenderBlock(blockSequence++, 256, engine.transportPhaseSteps(),
                               engine.bpm(), engine.sampleRate(),
                               engine.isPlaying(), false, false);
  }
  void endBlock() { queue.endMidiRenderBlock(); }

  Trace drain() {
    Trace trace{};
    ScheduledMusicalEvent scheduled{};
    while (queue.tryPop(scheduled)) trace.events.push_back(scheduled.event);
    const uint8_t panic = queue.takePendingAllNotesOffMask();
    if (panic & ScheduledMusicalEventQueue::kSynthAMask) {
      trace.events.push_back(MusicalEvent{MusicalEventType::AllNotesOff,
          MusicalEventSource::PatternPlayer, MusicalEventTarget::SynthA, 0, 0, 0});
    }
    if (panic & ScheduledMusicalEventQueue::kSynthBMask) {
      trace.events.push_back(MusicalEvent{MusicalEventType::AllNotesOff,
          MusicalEventSource::PatternPlayer, MusicalEventTarget::SynthB, 0, 0, 0});
    }
    return trace;
  }
};

PhraseRuntime::RuntimeSynthEvent patternEvent(uint8_t note) {
  PhraseRuntime::RuntimeSynthEvent event{};
  event.startTick = 0;
  event.durationSubticks = 4096;
  event.note = note;
  event.velocity = 104;
  event.probability = 100;
  return event;
}

void startPattern(Fixture& f, int synth, uint8_t note) {
  f.engine.playing = true;
  f.beginBlock();
  const auto actions = f.engine.patternPlaybackState_[synth].acceptOnset(
      patternEvent(note), 0);
  f.engine.consumePatternPlaybackActions_(synth, actions);
  f.endBlock();
  const Trace onset = f.drain();
  if (onset.count(MusicalEventType::NoteOn, targetForSynth(synth)) != 1 ||
      !f.engine.patternPlaybackState_[synth].active() ||
      !f.engine.patternOwnsInternalSynth(synth) || !f.noteHeld(synth)) {
    std::fprintf(stderr, "fixture setup failed for synth %d\n", synth);
    std::abort();
  }
}

SynthPattern connectedAcidC1AWitness() {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
    pattern.steps[step].note = -1;
  }

  // C1A authoritative minimal causal pair from Acid / BASE / P1 / identity=2:
  // step 0 is the active predecessor and step 1 is its physical continuation
  // carrier. The continuation copies the held pitch and is marked slide=true;
  // it is not a separate semantic onset in the generation contract.
  pattern.steps[0].note = 36;
  pattern.steps[1] = pattern.steps[0];
  pattern.steps[1].slide = true;
  return pattern;
}

const PhraseRuntime::RuntimePatternEventBuffer& installConnectedAcidC1AWitness(
    Fixture& f) {
  constexpr int kSynth = 0;
  f.engine.editSynthPattern(kSynth) = connectedAcidC1AWitness();
  if (!f.engine.rebuildPatternRuntimeEventBank()) {
    std::fprintf(stderr, "C1B fixture runtime-bank rebuild failed\n");
    std::abort();
  }

  return f.engine.patternRuntimeBank_.selectForPage(
      f.engine.currentPageIndex(), static_cast<uint8_t>(kSynth),
      static_cast<uint8_t>(f.engine.current303BankIndex(kSynth)),
      static_cast<uint8_t>(f.engine.current303PatternIndex(kSynth)));
}

template <typename Fn>
void expectTargetBarrier(const char* name, int synth, Fn&& invoke) {
  Fixture f;
  startPattern(f, synth, static_cast<uint8_t>(60 + synth));
  f.beginBlock();
  invoke(f.engine);
  f.endBlock();
  const Trace trace = f.drain();
  const MusicalEventTarget target = targetForSynth(synth);
  const MusicalEventTarget other = targetForSynth(1 - synth);

  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "RuntimeSynthPlaybackState stayed active across source transition");
  expect(!f.noteHeld(synth), name,
         "old Pattern physical voice stayed held across source transition");
  expect(!f.engine.patternOwnsInternalSynth(synth), name,
         "old Pattern physical ownership bit stayed set");
  expect(trace.count(MusicalEventType::NoteOff, target) == 1, name,
         "source transition did not emit exactly one target Pattern NoteOff");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "ordinary source transition used Pattern panic instead of Release");
  expect(trace.count(MusicalEventType::NoteOff, other) == 0, name,
         "target source transition released the other Pattern target");
  expect(trace.count(MusicalEventType::AllNotesOff, other) == 0, name,
         "target source transition emitted global Pattern panic");
}

void caseMute(int synth) {
  expectTargetBarrier("MUTE", synth, [synth](MiniAcid& engine) {
    engine.setMute303(synth, true);
  });
}

void casePatternIndex(int synth) {
  expectTargetBarrier("PATTERN INDEX", synth, [synth](MiniAcid& engine) {
    const int next = engine.current303PatternIndex(synth) == 0 ? 1 : 0;
    engine.set303PatternIndex(synth, static_cast<int16_t>(next));
  });
}

void caseBankIndex(int synth) {
  expectTargetBarrier("BANK INDEX", synth, [synth](MiniAcid& engine) {
    const int next = engine.current303BankIndex(synth) == 0 ? 1 : 0;
    engine.set303BankIndex(synth, next);
  });
}

void casePageIdentity(int synth) {
  expectTargetBarrier("PAGE IDENTITY", synth, [](MiniAcid& engine) {
    const int8_t next = engine.currentPageIndex() == 0 ? 1 : 0;
    engine.setCurrentPage(next);
  });
}

void caseSongMode(int synth) {
  expectTargetBarrier("SONG MODE", synth, [](MiniAcid& engine) {
    engine.setSongMode(true);
  });
}

void caseSequencedSourceTransfer(int synth) {
  expectTargetBarrier("PATTERN -> PHRASE SOURCE", synth,
                      [synth](MiniAcid& engine) {
    engine.setSequencedSource(synth, MiniAcid::SequencedSource::Phrase);
  });
}

void caseConnectedAcidContinuationInvalidatedBySourceTransfer() {
  constexpr int kSynth = 0;
  const MusicalEventTarget target = targetForSynth(kSynth);
  const char* name = "C1B CONNECTED ACID PATTERN -> PHRASE";

  // Negative control: while Pattern remains authoritative, prove that the
  // physical step-1 continuation carrier really is present in the retained
  // runtime bank and reaches the real sequencer executor as a slide event.
  {
    Fixture control;
    const PhraseRuntime::RuntimePatternEventBuffer& events =
        installConnectedAcidC1AWitness(control);
    const PhraseRuntime::RuntimeSynthEvent* step0 = events.eventForSourceStep(0);
    const PhraseRuntime::RuntimeSynthEvent* step1 = events.eventForSourceStep(1);

    expect(step0 != nullptr, name,
           "C1A predecessor step 0 was not retained in the runtime bank");
    expect(step1 != nullptr, name,
           "C1A continuation carrier step 1 was not retained in the runtime bank");
    if (step0 == nullptr || step1 == nullptr) return;
    expect(step0->note == 36, name,
           "C1A predecessor pitch changed during runtime projection");
    expect(step1->note == 36, name,
           "C1A continuation pitch changed during runtime projection");
    expect((step1->flags & PhraseRuntime::kEventSlide) != 0, name,
           "C1A continuation lost its physical slide carrier in projection");

    control.engine.playing = true;
    control.beginBlock();
    control.engine.processSequencerEvents(step0->startTick);
    control.endBlock();
    const Trace onset = control.drain();
    expect(onset.count(MusicalEventType::NoteOn, target) == 1, name,
           "real executor did not start the C1A predecessor");
    expect(control.engine.patternPlaybackState_[kSynth].active(), name,
           "C1A predecessor did not establish sequenced lifetime ownership");

    control.beginBlock();
    control.engine.processSequencerEvents(step1->startTick);
    control.endBlock();
    const Trace continuation = control.drain();
    expect(continuation.count(MusicalEventType::NoteOn, target) == 1, name,
           "negative control did not dispatch the connected step-1 carrier");
  }

  // C1B subject: cut Pattern ownership after the predecessor but before the
  // continuation tick, then let the real executor cross that exact old tick.
  // The old Pattern carrier must not be able to reassert sound or ownership.
  Fixture f;
  const PhraseRuntime::RuntimePatternEventBuffer& events =
      installConnectedAcidC1AWitness(f);
  const PhraseRuntime::RuntimeSynthEvent* step0 = events.eventForSourceStep(0);
  const PhraseRuntime::RuntimeSynthEvent* step1 = events.eventForSourceStep(1);
  expect(step0 != nullptr && step1 != nullptr, name,
         "connected C1A pair disappeared from the runtime bank");
  if (step0 == nullptr || step1 == nullptr) return;

  const uint16_t predecessorTick = step0->startTick;
  const uint16_t continuationTick = step1->startTick;
  f.engine.playing = true;
  f.beginBlock();
  f.engine.processSequencerEvents(predecessorTick);
  f.endBlock();
  const Trace onset = f.drain();
  expect(onset.count(MusicalEventType::NoteOn, target) == 1, name,
         "connected predecessor did not start before source transfer");
  expect(f.engine.patternPlaybackState_[kSynth].active(), name,
         "connected predecessor did not own RuntimeSynthPlaybackState");
  expect(f.engine.patternOwnsInternalSynth(kSynth), name,
         "connected predecessor did not acquire physical Pattern ownership");
  expect(f.noteHeld(kSynth), name,
         "connected predecessor did not hold the physical synth voice");

  f.beginBlock();
  f.engine.setSequencedSource(kSynth, MiniAcid::SequencedSource::Phrase);
  f.endBlock();
  const Trace transfer = f.drain();
  expect(f.engine.currentSequencedSource(kSynth) ==
             MiniAcid::SequencedSource::Phrase,
         name, "source transfer did not publish Phrase authority");
  expect(!f.engine.patternPlaybackState_[kSynth].active(), name,
         "connected predecessor lifetime survived source transfer");
  expect(!f.engine.patternOwnsInternalSynth(kSynth), name,
         "old Pattern ownership survived source transfer");
  expect(!f.noteHeld(kSynth), name,
         "old connected predecessor stayed physically held after transfer");
  expect(transfer.count(MusicalEventType::NoteOff, target) == 1, name,
         "source transfer did not release the connected predecessor exactly once");
  expect(transfer.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "source transfer used panic instead of targeted connected release");
  expect(f.engine.currentPhraseBuffer(kSynth).count == 0, name,
         "C1B fixture unexpectedly has Phrase material at the continuation tick");

  f.beginBlock();
  f.engine.processSequencerEvents(continuationTick);
  f.endBlock();
  const Trace after = f.drain();
  expect(after.count(MusicalEventType::NoteOn, target) == 0, name,
         "stale Pattern continuation reasserted sound after Phrase took authority");
  expect(after.count(MusicalEventType::NoteOff, target) == 0, name,
         "stale Pattern continuation caused a second old-owner release");
  expect(after.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "stale Pattern continuation caused panic cleanup");
  expect(!f.engine.patternPlaybackState_[kSynth].active(), name,
         "stale continuation reactivated RuntimeSynthPlaybackState");
  expect(!f.engine.patternOwnsInternalSynth(kSynth), name,
         "stale continuation reacquired Pattern physical ownership");
  expect(!f.noteHeld(kSynth), name,
         "stale continuation re-held the physical synth voice");
}

void casePhraseToPatternSequencedSourceTransfer(int synth) {
  Fixture f;
  const char* name = "PHRASE -> PATTERN SOURCE";
  f.engine.setSequencedSource(synth, MiniAcid::SequencedSource::Phrase);
  (void)f.drain();
  expect(f.engine.currentSequencedSource(synth) ==
             MiniAcid::SequencedSource::Phrase,
         name, "fixture did not select Phrase before the held-note transfer");

  // RuntimeSynthPlaybackState is the common sequenced-note lifetime owner. Seed
  // an active held lifetime while Phrase is authoritative, then transfer source
  // ownership back to Pattern and require the same target-scoped release.
  startPattern(f, synth, static_cast<uint8_t>(62 + synth));
  f.beginBlock();
  f.engine.setSequencedSource(synth, MiniAcid::SequencedSource::Pattern);
  f.endBlock();
  const Trace trace = f.drain();
  const MusicalEventTarget target = targetForSynth(synth);
  const MusicalEventTarget other = targetForSynth(1 - synth);

  expect(f.engine.currentSequencedSource(synth) ==
             MiniAcid::SequencedSource::Pattern,
         name, "source transfer did not publish Pattern");
  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "RuntimeSynthPlaybackState stayed active across Phrase -> Pattern transfer");
  expect(!f.noteHeld(synth), name,
         "old Phrase physical voice stayed held across source transfer");
  expect(!f.engine.patternOwnsInternalSynth(synth), name,
         "old sequenced physical ownership bit stayed set");
  expect(trace.count(MusicalEventType::NoteOff, target) == 1, name,
         "Phrase -> Pattern transfer did not emit exactly one targeted NoteOff");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "Phrase -> Pattern transfer used panic instead of targeted release");
  expect(trace.count(MusicalEventType::NoteOff, other) == 0, name,
         "Phrase -> Pattern transfer released the other synth target");
  expect(trace.count(MusicalEventType::AllNotesOff, other) == 0, name,
         "Phrase -> Pattern transfer emitted global panic");
}

void caseMakePhraseTransfer(int synth) {
  Fixture f;
  SynthPattern& pattern = f.engine.editSynthPattern(synth);
  pattern.steps[0].note = static_cast<uint8_t>(36 + synth);
  pattern.steps[4].note = static_cast<uint8_t>(43 + synth);

  startPattern(f, synth, static_cast<uint8_t>(64 + synth));
  f.beginBlock();
  const bool made = f.engine.makePhrase(synth);
  f.endBlock();
  const Trace trace = f.drain();
  const MusicalEventTarget target = targetForSynth(synth);
  const MusicalEventTarget other = targetForSynth(1 - synth);
  const char* name = "MAKE PHRASE OWNERSHIP TRANSFER";

  expect(made, name, "MAKE PHRASE rejected a valid Pattern source");
  expect(f.engine.currentSequencedSource(synth) ==
             MiniAcid::SequencedSource::Phrase,
         name, "MAKE PHRASE did not publish the Phrase source");
  expect(f.engine.currentPhraseBuffer(synth).count > 0, name,
         "MAKE PHRASE did not publish Phrase material");
  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "old Pattern RuntimeSynthPlaybackState survived MAKE PHRASE");
  expect(!f.noteHeld(synth), name,
         "old Pattern physical voice stayed held after MAKE PHRASE");
  expect(!f.engine.patternOwnsInternalSynth(synth), name,
         "old Pattern physical ownership bit survived MAKE PHRASE");
  expect(trace.count(MusicalEventType::NoteOff, target) == 1, name,
         "MAKE PHRASE did not emit exactly one old-owner NoteOff");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "MAKE PHRASE used panic cleanup instead of targeted Release");
  expect(trace.count(MusicalEventType::NoteOff, other) == 0, name,
         "MAKE PHRASE released the other Pattern target");
  expect(trace.count(MusicalEventType::AllNotesOff, other) == 0, name,
         "MAKE PHRASE emitted global Pattern panic");
}

void caseSameSequencedSourceIsNotBarrier(int synth) {
  Fixture f;
  startPattern(f, synth, static_cast<uint8_t>(68 + synth));
  f.beginBlock();
  f.engine.setSequencedSource(synth, MiniAcid::SequencedSource::Pattern);
  f.endBlock();
  const Trace trace = f.drain();
  const MusicalEventTarget target = targetForSynth(synth);
  const char* name = "SAME SOURCE";

  expect(f.engine.patternPlaybackState_[synth].active(), name,
         "idempotent source publication cut an active Pattern lifetime");
  expect(f.noteHeld(synth), name,
         "idempotent source publication released the physical Pattern voice");
  expect(f.engine.patternOwnsInternalSynth(synth), name,
         "idempotent source publication cleared Pattern ownership");
  expect(trace.count(MusicalEventType::NoteOff, target) == 0, name,
         "idempotent source publication emitted a Pattern NoteOff");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "idempotent source publication emitted Pattern panic");
}

void caseSongPosition(int synth) {
  Fixture f;
  f.engine.setSongMode(true);
  (void)f.drain();
  startPattern(f, synth, static_cast<uint8_t>(66 + synth));
  f.beginBlock();
  const int next = f.engine.currentSongPosition() == 0 ? 1 : 0;
  f.engine.setSongPosition(next);
  f.endBlock();
  const Trace trace = f.drain();
  const MusicalEventTarget target = targetForSynth(synth);
  const char* name = "SONG POSITION";
  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "RuntimeSynthPlaybackState stayed active across SONG position source change");
  expect(!f.noteHeld(synth), name, "old SONG Pattern physical voice stayed held");
  expect(trace.count(MusicalEventType::NoteOff, target) == 1, name,
         "SONG position source transition did not emit exactly one target NoteOff");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "SONG position source transition used panic cleanup");
}

void caseSynthEngineConflict(int synth) {
  Fixture f;
  const uint8_t liveNote = static_cast<uint8_t>(72 + synth);
  f.engine.liveNoteOn(synth, liveNote, 100);
  startPattern(f, synth, static_cast<uint8_t>(58 + synth));
  const uint8_t maskAtEntry = f.engine.patternOwnedMask_.load(std::memory_order_acquire);
  expect((maskAtEntry & static_cast<uint8_t>(1u << synth)) != 0,
         "SYNTH ENGINE setup", "Pattern did not own physical backend at source change entry");

  f.beginBlock();
  f.engine.setSynthEngine(synth, "SID");
  f.endBlock();
  const Trace trace = f.drain();
  const MusicalEventTarget target = targetForSynth(synth);
  const char* name = "SYNTH ENGINE";
  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "Pattern runtime lifetime survived physical synth source swap");
  expect(f.engine.liveNote(synth) == -1, name,
         "suppressed live candidate survived physical synth source swap");
  expect(!f.engine.patternOwnsInternalSynth(synth), name,
         "Pattern ownership bit survived physical synth source swap");
  expect(trace.count(MusicalEventType::NoteOff, target) == 1, name,
         "physical synth source swap did not translate exactly one Pattern Release");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "physical synth source swap used Pattern panic cleanup");
}

}  // namespace

int main() {
  for (int synth = 0; synth < 2; ++synth) {
    caseMute(synth);
    casePatternIndex(synth);
    caseBankIndex(synth);
    casePageIdentity(synth);
    caseSongMode(synth);
    caseSequencedSourceTransfer(synth);
    casePhraseToPatternSequencedSourceTransfer(synth);
    caseMakePhraseTransfer(synth);
    caseSameSequencedSourceIsNotBarrier(synth);
    caseSongPosition(synth);
    caseSynthEngineConflict(synth);
  }
  caseConnectedAcidContinuationInvalidatedBySourceTransfer();
  if (g_failures != 0) {
    std::fprintf(stderr, "P2 lifecycle barrier characterization: %d failure(s)\n", g_failures);
    return 1;
  }
  std::puts("P2 lifecycle barrier characterization: PASS");
  return 0;
}