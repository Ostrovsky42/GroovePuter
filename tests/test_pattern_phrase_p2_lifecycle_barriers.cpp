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
    caseSongPosition(synth);
    caseSynthEngineConflict(synth);
  }
  if (g_failures != 0) {
    std::fprintf(stderr, "P2 lifecycle barrier characterization: %d failure(s)\n", g_failures);
    return 1;
  }
  std::puts("P2 lifecycle barrier characterization: PASS");
  return 0;
}
