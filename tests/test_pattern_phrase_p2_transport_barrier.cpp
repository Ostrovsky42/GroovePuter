#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

// Focused host characterization. Open current runtime state without adding a
// production-only probe API.
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"

SerialMock Serial;
SDMock SD;

namespace {

int g_failures = 0;

void expect(bool condition, const char* caseName, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "%s FAIL: %s\n", caseName, message);
  ++g_failures;
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

MusicalEventTarget targetForSynth(int synth) {
  return synth == 0 ? MusicalEventTarget::SynthA : MusicalEventTarget::SynthB;
}

struct BarrierTrace {
  std::vector<MusicalEvent> events;

  std::size_t count(MusicalEventType type, MusicalEventTarget target) const {
    return static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [&](const MusicalEvent& event) {
          return event.type == type && event.target == target &&
                 event.source == MusicalEventSource::PatternPlayer;
        }));
  }

  std::size_t patternEventCount() const {
    return static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [](const MusicalEvent& event) {
          return event.source == MusicalEventSource::PatternPlayer;
        }));
  }
};

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};
  uint32_t blockSequence{1};

  Fixture() {
    engine.setBpm(120.0f);
    if (!engine.rebuildPatternRuntimeEventBank()) {
      std::fprintf(stderr, "fixture setup failed: runtime Pattern bank\n");
      std::abort();
    }
    engine.setCurrentPage(
        static_cast<int8_t>(PatternPagingService::activePageIndex()));
    engine.setPatternEventQueue(&queue);
    queue.setPhaseReader(readEnginePhase, &engine);
  }

  bool noteHeld(int synth) const {
    const SwappableSynthVoice* voice = engine.synthVoices_[synth].get();
    return voice != nullptr && voice->noteHeld_;
  }

  void beginBlock() {
    queue.beginMidiRenderBlock(
        blockSequence++,
        256,
        engine.transportPhaseSteps(),
        engine.bpm(),
        engine.sampleRate(),
        engine.isPlaying(),
        false,
        false);
  }

  void endBlock() { queue.endMidiRenderBlock(); }

  BarrierTrace drain() {
    BarrierTrace trace{};
    ScheduledMusicalEvent scheduled{};
    while (queue.tryPop(scheduled)) {
      trace.events.push_back(scheduled.event);
    }
    const uint8_t panicMask = queue.takePendingAllNotesOffMask();
    if (panicMask != 0) {
      if (panicMask & ScheduledMusicalEventQueue::kSynthAMask) {
        trace.events.push_back(MusicalEvent{
            MusicalEventType::AllNotesOff,
            MusicalEventSource::PatternPlayer,
            MusicalEventTarget::SynthA,
            0,
            0,
            0,
        });
      }
      if (panicMask & ScheduledMusicalEventQueue::kSynthBMask) {
        trace.events.push_back(MusicalEvent{
            MusicalEventType::AllNotesOff,
            MusicalEventSource::PatternPlayer,
            MusicalEventTarget::SynthB,
            0,
            0,
            0,
        });
      }
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
  const BarrierTrace onset = f.drain();
  const MusicalEventTarget target = targetForSynth(synth);
  if (onset.count(MusicalEventType::NoteOn, target) != 1 ||
      !f.engine.patternPlaybackState_[synth].active() ||
      !f.engine.patternOwnsInternalSynth(synth) || !f.noteHeld(synth)) {
    std::fprintf(stderr,
                 "fixture setup failed: Pattern onset noteOn=%zu active=%d owner=%d held=%d\n",
                 onset.count(MusicalEventType::NoteOn, target),
                 f.engine.patternPlaybackState_[synth].active() ? 1 : 0,
                 f.engine.patternOwnsInternalSynth(synth) ? 1 : 0,
                 f.noteHeld(synth) ? 1 : 0);
    std::abort();
  }
}

enum class BarrierKind : uint8_t { Stop, Pause };

BarrierTrace invokeBarrier(Fixture& f, BarrierKind kind) {
  f.beginBlock();
  if (kind == BarrierKind::Stop) {
    f.engine.stop();
  } else {
    f.engine.pauseTransport();
  }
  f.endBlock();
  return f.drain();
}

void casePatternOnly(BarrierKind kind, int synth) {
  Fixture f;
  startPattern(f, synth, static_cast<uint8_t>(60 + synth));
  const BarrierTrace trace = invokeBarrier(f, kind);
  const MusicalEventTarget target = targetForSynth(synth);
  const MusicalEventTarget other = targetForSynth(1 - synth);
  const char* name = kind == BarrierKind::Stop
      ? "CASE A Pattern-only STOP"
      : "CASE A Pattern-only PAUSE";

  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "RuntimeSynthPlaybackState remained active after barrier");
  expect(!f.noteHeld(synth), name,
         "internal Pattern voice remained held after barrier");
  expect(!f.engine.patternOwnsInternalSynth(synth), name,
         "Pattern physical ownership bit remained set after barrier");
  expect(trace.count(MusicalEventType::NoteOff, target) == 1, name,
         "target did not receive exactly one Pattern NoteOff");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "ordinary Pattern Release used target AllNotesOff instead of NoteOff");
  expect(trace.count(MusicalEventType::NoteOff, other) == 0, name,
         "barrier emitted a NoteOff for the inactive other Pattern target");
  expect(trace.count(MusicalEventType::AllNotesOff, other) == 0, name,
         "barrier emitted global/panic cleanup for the inactive other target");
}

void caseLiveOnly(BarrierKind kind, int synth) {
  Fixture f;
  const uint8_t liveNote = static_cast<uint8_t>(70 + synth);
  f.engine.liveNoteOn(synth, liveNote, 100);
  f.engine.playing = true;
  expect(f.engine.liveNote(synth) == liveNote, "CASE B setup",
         "live candidate was not recorded");
  expect(f.noteHeld(synth), "CASE B setup",
         "live candidate did not own the internal voice");

  const BarrierTrace trace = invokeBarrier(f, kind);
  const char* name = kind == BarrierKind::Stop
      ? "CASE B Live-only STOP"
      : "CASE B Live-only PAUSE";

  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "inactive Pattern state unexpectedly became active");
  expect(f.engine.liveNote(synth) == -1, name,
         "existing live candidate was not cleared");
  expect(!f.noteHeld(synth), name,
         "existing live-only STOP/PAUSE physical cleanup changed");
  expect(trace.patternEventCount() == 0, name,
         "live-only barrier emitted a Pattern lifetime event");
}

void casePatternSuppressesLive(BarrierKind kind, int synth) {
  Fixture f;
  const uint8_t liveNote = static_cast<uint8_t>(72 + synth);
  const uint8_t patternNote = static_cast<uint8_t>(58 + synth);
  f.engine.liveNoteOn(synth, liveNote, 100);
  startPattern(f, synth, patternNote);

  const uint8_t authorityAtEntry =
      f.engine.patternOwnedMask_.load(std::memory_order_acquire);
  const uint8_t targetBit = static_cast<uint8_t>(1u << synth);
  expect((authorityAtEntry & targetBit) != 0u, "CASE C setup",
         "Pattern did not own the physical backend at barrier entry");
  expect(f.engine.liveNote(synth) == liveNote, "CASE C setup",
         "suppressed live candidate disappeared before the barrier");

  const BarrierTrace trace = invokeBarrier(f, kind);
  const MusicalEventTarget target = targetForSynth(synth);
  const char* name = kind == BarrierKind::Stop
      ? "CASE C Pattern-suppresses-live STOP"
      : "CASE C Pattern-suppresses-live PAUSE";

  expect(!f.engine.patternPlaybackState_[synth].active(), name,
         "Pattern logical lifetime survived the barrier");
  expect(!f.noteHeld(synth), name,
         "Pattern-owned physical voice survived the barrier");
  expect(f.engine.liveNote(synth) == -1, name,
         "suppressed live candidate survived cleanup and can resurrect");
  expect(trace.count(MusicalEventType::NoteOff, target) == 1, name,
         "Pattern Release did not produce exactly one target-scoped NoteOff");
  expect(trace.count(MusicalEventType::AllNotesOff, target) == 0, name,
         "Pattern Release used panic/global cleanup");

  f.engine.liveNoteOff(synth, liveNote);
  expect(f.engine.liveNote(synth) == -1 && !f.noteHeld(synth), name,
         "released suppressed live candidate resurrected after late NoteOff");
}

void caseRepeatedStop(int synth) {
  Fixture f;
  startPattern(f, synth, static_cast<uint8_t>(64 + synth));
  const MusicalEventTarget target = targetForSynth(synth);

  const BarrierTrace first = invokeBarrier(f, BarrierKind::Stop);
  expect(first.count(MusicalEventType::NoteOff, target) == 1,
         "CASE D repeated STOP",
         "first STOP did not translate one Pattern Release");
  expect(!f.engine.patternPlaybackState_[synth].active(),
         "CASE D repeated STOP",
         "first STOP did not make Pattern state inactive");

  const BarrierTrace second = invokeBarrier(f, BarrierKind::Stop);
  expect(second.count(MusicalEventType::NoteOff, target) == 0,
         "CASE D repeated STOP",
         "second STOP emitted an additional Pattern NoteOff");
  expect(second.count(MusicalEventType::AllNotesOff, target) == 0,
         "CASE D repeated STOP",
         "second STOP emitted Pattern panic cleanup");
  expect(second.patternEventCount() == 0,
         "CASE D repeated STOP",
         "second STOP emitted a new Pattern backend event");
}

}  // namespace

int main() {
  for (int synth = 0; synth < 2; ++synth) {
    casePatternOnly(BarrierKind::Stop, synth);
    casePatternOnly(BarrierKind::Pause, synth);
    caseLiveOnly(BarrierKind::Stop, synth);
    caseLiveOnly(BarrierKind::Pause, synth);
    casePatternSuppressesLive(BarrierKind::Stop, synth);
    casePatternSuppressesLive(BarrierKind::Pause, synth);
    caseRepeatedStop(synth);
  }

  if (g_failures != 0) {
    std::fprintf(stderr,
                 "P2 STOP/PAUSE ownership characterization: %d failure(s)\n",
                 g_failures);
    return 1;
  }

  std::puts("P2 STOP/PAUSE ownership characterization: PASS");
  return 0;
}
