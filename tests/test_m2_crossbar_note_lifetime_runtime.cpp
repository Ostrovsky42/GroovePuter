#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

// Research-only host characterization. No production seam is introduced: this
// translation unit opens the existing private runtime state only for tests.
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/input/musical_event_queue.h"
#include "src/midi/usb_midi_output.h"

// sdl_main.cpp normally owns these host compatibility globals. The focused
// runtime harness replaces sdl_main.cpp, so define the same host-only globals
// here rather than introducing a production seam just to satisfy linkage.
SerialMock Serial;
SDMock SD;

namespace {

enum class WireType : uint8_t { NoteOn, NoteOff };

struct WirePacket {
  WireType type;
  uint8_t channel;
  uint8_t note;
  uint8_t velocity;
};

class FakeUsbMidiTransport final : public IUsbMidiTransport {
 public:
  bool begin() override { return true; }
  bool mounted() const override { return true; }
  bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
    packets.push_back({WireType::NoteOn, channel, note, velocity});
    return true;
  }
  bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
    packets.push_back({WireType::NoteOff, channel, note, velocity});
    return true;
  }
  void flush() override {}

  std::vector<WirePacket> packets;
};

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

MusicalEvent panicEvent(MusicalEventTarget target) {
  return MusicalEvent{
      MusicalEventType::AllNotesOff,
      MusicalEventSource::PatternPlayer,
      target,
      0,
      0,
      0,
  };
}

struct RuntimeFixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};
  FakeUsbMidiTransport transport{};
  UsbMidiOutput midi{transport};
  uint32_t blockSequence{1};

  RuntimeFixture() {
    engine.setBpm(120.0f);
    engine.setPatternEventQueue(&queue);
    queue.setPhaseReader(readEnginePhase, &engine);
    assert(midi.begin());
    midi.pollConnection();
  }

  SwappableSynthVoice* voice(int synth = 0) {
    return engine.synthVoices_[synth].get();
  }

  bool noteHeld(int synth = 0) {
    SwappableSynthVoice* current = voice(synth);
    return current && current->noteHeld_;
  }

  void beginRender() {
    queue.beginMidiRenderBlock(
        blockSequence++,
        16384,
        engine.transportPhaseSteps(),
        engine.bpm(),
        engine.sampleRate(),
        engine.isPlaying(),
        false,
        false);
  }

  void endRender() { queue.endMidiRenderBlock(); }

  void dispatchPendingPanics() {
    const uint8_t mask = queue.takePendingAllNotesOffMask();
    if (mask & ScheduledMusicalEventQueue::kSynthAMask) {
      midi.handleMusicalEvent(panicEvent(MusicalEventTarget::SynthA));
    }
    if (mask & ScheduledMusicalEventQueue::kSynthBMask) {
      midi.handleMusicalEvent(panicEvent(MusicalEventTarget::SynthB));
    }
    if (mask & ScheduledMusicalEventQueue::kDrumsMask) {
      midi.handleMusicalEvent(panicEvent(MusicalEventTarget::Drums));
    }
  }

  void dispatchQueuedCurrentGeneration() {
    ScheduledMusicalEvent scheduled{};
    while (queue.tryPop(scheduled)) {
      if (!scheduledMusicalEventGenerationIsCurrent(
              scheduled, queue.generationFor(scheduled.event.target))) {
        continue;
      }
      midi.handleMusicalEvent(scheduled.event);
    }
  }

  void dispatchLikeProduction() {
    // cardputer_usb_midi_transport.cpp dispatches target panics before queued
    // events from the following generation.
    dispatchPendingPanics();
    dispatchQueuedCurrentGeneration();
  }
};

void clearPattern(SynthPattern& pattern) {
  pattern = SynthPattern{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i].note = -1;
  }
}

void setStep(SynthPattern& pattern,
             int step,
             int note,
             uint8_t velocity = 100,
             bool slide = false) {
  pattern.steps[step].note = static_cast<int8_t>(note);
  pattern.steps[step].velocity = velocity;
  pattern.steps[step].slide = slide;
  pattern.steps[step].probability = 100;
}

void setPattern(RuntimeFixture& f,
                int patternIndex,
                int step,
                int note,
                uint8_t velocity = 100,
                bool slide = false) {
  f.engine.set303PatternIndex(0, static_cast<int16_t>(patternIndex));
  SynthPattern& pattern = f.engine.editSynthPattern(0);
  clearPattern(pattern);
  setStep(pattern, step, note, velocity, slide);
}

std::size_t countWire(const RuntimeFixture& f, WireType type) {
  return static_cast<std::size_t>(std::count_if(
      f.transport.packets.begin(), f.transport.packets.end(),
      [&](const WirePacket& packet) { return packet.type == type; }));
}

void expectWire(const WirePacket& packet,
                WireType type,
                uint8_t channel,
                uint8_t note) {
  assert(packet.type == type);
  assert(packet.channel == channel);
  assert(packet.note == note);
}

void renderFromTick(RuntimeFixture& f,
                    uint32_t tickBeforeFirstTrigger,
                    float stepSpan) {
  f.engine.currentTick_ = tickBeforeFirstTrigger;
  f.engine.tickPhaseAccum_ = 0x100000000ULL;
  f.engine.playing = true;
  f.beginRender();
  const std::size_t frames = static_cast<std::size_t>(
      std::ceil(f.engine.samplesPerStep_ * stepSpan));
  std::vector<int16_t> audio(frames);
  f.engine.generateAudioBuffer(audio.data(), audio.size());
  f.endRender();
  f.dispatchLikeProduction();
}

void testAInsideBarGateExpires() {
  RuntimeFixture f;
  setPattern(f, 0, 2, 60, 91);
  renderFromTick(f, 47, 0.95f);  // tick 48 = step 2

  assert(f.engine.gateCountdownA_ == 0);
  assert(!f.noteHeld());
  assert(countWire(f, WireType::NoteOn) == 1);
  assert(countWire(f, WireType::NoteOff) == 1);
  std::puts("A PASS: ordinary onset/release completes inside one bar");
}

void testBFinalStepToEmptyNextBar() {
  RuntimeFixture f;
  setPattern(f, 0, 15, 60, 92);
  renderFromTick(f, 359, 1.10f);  // tick 360 step15 -> tick 384 next bar step0

  assert(f.engine.gateCountdownA_ == 0);
  assert(!f.noteHeld());
  assert(countWire(f, WireType::NoteOn) == 1);
  assert(countWire(f, WireType::NoteOff) == 1);
  std::puts("B PASS: final-step ordinary note is released before empty next bar");
}

void testCSamePitchBoundaryRetriggers() {
  RuntimeFixture f;
  setPattern(f, 0, 15, 60, 90);
  SynthPattern& pattern = f.engine.editSynthPattern(0);
  setStep(pattern, 0, 60, 111, false);
  renderFromTick(f, 359, 1.10f);

  assert(f.noteHeld());
  assert(f.voice()->lastVelocity_ == 111);
  assert(!f.voice()->lastSlide_);
  assert(f.transport.packets.size() == 3);
  expectWire(f.transport.packets[0], WireType::NoteOn, 7, 60);
  expectWire(f.transport.packets[1], WireType::NoteOff, 7, 60);
  expectWire(f.transport.packets[2], WireType::NoteOn, 7, 60);
  std::puts("C PASS: same-pitch next-bar onset is a real retrigger");
}

void testDDifferentPitchBoundaryRetriggers() {
  RuntimeFixture f;
  setPattern(f, 0, 15, 60, 90);
  SynthPattern& pattern = f.engine.editSynthPattern(0);
  setStep(pattern, 0, 64, 112, false);
  renderFromTick(f, 359, 1.10f);

  assert(f.noteHeld());
  assert(f.voice()->lastVelocity_ == 112);
  assert(f.transport.packets.size() == 3);
  expectWire(f.transport.packets[0], WireType::NoteOn, 7, 60);
  expectWire(f.transport.packets[1], WireType::NoteOff, 7, 60);
  expectWire(f.transport.packets[2], WireType::NoteOn, 7, 64);
  std::puts("D PASS: different-pitch next-bar onset replaces/retriggers");
}

void testESlideBoundaryIsStillOnset() {
  RuntimeFixture f;
  setPattern(f, 0, 15, 60, 90);
  SynthPattern& pattern = f.engine.editSynthPattern(0);
  setStep(pattern, 0, 64, 113, true);
  renderFromTick(f, 359, 1.10f);

  assert(f.noteHeld());
  assert(f.voice()->lastSlide_);
  assert(f.transport.packets.size() == 3);
  expectWire(f.transport.packets[0], WireType::NoteOn, 7, 60);
  expectWire(f.transport.packets[1], WireType::NoteOff, 7, 60);
  expectWire(f.transport.packets[2], WireType::NoteOn, 7, 64);
  std::puts("E PASS: slide at boundary is articulation on a new onset");
}

void testFStopIsHardBarrier() {
  RuntimeFixture f;
  setPattern(f, 0, 15, 60, 94);
  f.beginRender();
  f.engine.triggerSynthStep_(0, 15);
  f.endRender();
  f.dispatchLikeProduction();
  assert(f.noteHeld());
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 1);

  f.engine.playing = true;
  f.beginRender();
  f.engine.stop();
  f.endRender();
  f.dispatchLikeProduction();

  assert(!f.noteHeld());
  assert(f.engine.gateCountdownA_ == 0);
  assert(f.engine.gateCountdownB_ == 0);
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 0);
  assert(countWire(f, WireType::NoteOff) >= 1);
  std::puts("F PASS: Stop clears internal and MIDI PatternPlayer lifetime");
}

void testGExplicitPatternSwitchIsDivergent() {
  RuntimeFixture f;
  setPattern(f, 0, 15, 60, 95);
  setPattern(f, 1, 0, 64, 100);
  f.engine.set303PatternIndex(0, 0);

  f.beginRender();
  f.engine.triggerSynthStep_(0, 15);
  f.endRender();
  f.dispatchLikeProduction();
  assert(f.noteHeld());
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 1);

  const long gateBefore = f.engine.gateCountdownA_;
  f.engine.playing = true;
  f.beginRender();
  f.engine.set303PatternIndex(0, 1);
  f.endRender();
  f.dispatchLikeProduction();

  assert(f.noteHeld());
  assert(f.engine.gateCountdownA_ == gateBefore);
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 0);
  std::puts("G PASS: explicit pattern switch releases MIDI while internal voice survives");
}

void testHSongAdvanceIsDivergent() {
  RuntimeFixture f;
  setPattern(f, 0, 15, 60, 96);
  setPattern(f, 1, 0, 64, 100);

  f.engine.setSongLength(2);
  f.engine.setSongPattern(0, SongTrack::SynthA, songPatternFromBank(0, 0));
  f.engine.setSongPattern(1, SongTrack::SynthA, songPatternFromBank(0, 1));
  f.engine.songMode_ = true;
  f.engine.sceneManager_.setSongMode(true);
  f.engine.songPlaybackSlot_ = f.engine.activeSongSlot();
  f.engine.sceneManager_.setSongPosition(0);
  f.engine.applySongPositionSelection();
  assert(f.engine.current303PatternIndex(0) == 0);

  f.beginRender();
  f.engine.triggerSynthStep_(0, 15);
  f.endRender();
  f.dispatchLikeProduction();
  assert(f.noteHeld());
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 1);

  const long gateBefore = f.engine.gateCountdownA_;
  f.engine.playing = true;
  f.beginRender();
  f.engine.sceneManager_.setSongPosition(1);
  f.engine.applySongPositionSelection();
  f.endRender();
  f.dispatchLikeProduction();

  assert(f.engine.current303PatternIndex(0) == 1);
  assert(f.noteHeld());
  assert(f.engine.gateCountdownA_ == gateBefore);
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 0);
  std::puts("H PASS: Song pattern advance panics MIDI while direct internal voice survives");
}

}  // namespace

int main() {
  testAInsideBarGateExpires();
  testBFinalStepToEmptyNextBar();
  testCSamePitchBoundaryRetriggers();
  testDDifferentPitchBoundaryRetriggers();
  testESlideBoundaryIsStillOnset();
  testFStopIsHardBarrier();
  testGExplicitPatternSwitchIsDivergent();
  testHSongAdvanceIsDivergent();
  std::puts("M2-A1 runtime characterization: OK");
  return 0;
}
