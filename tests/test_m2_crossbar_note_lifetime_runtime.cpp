#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Research-only host characterization. No production seam is introduced: the
// test opens MiniAcid's private runtime state locally so current gate/voice
// ownership can be observed while exercising the real implementation.
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/input/musical_event_queue.h"
#include "src/midi/usb_midi_output.h"

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
  bool mounted() const override { return mounted_; }
  bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
    if (!mounted_) return false;
    packets.push_back({WireType::NoteOn, channel, note, velocity});
    return true;
  }
  bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
    if (!mounted_) return false;
    packets.push_back({WireType::NoteOff, channel, note, velocity});
    return true;
  }
  void flush() override {}

  bool mounted_{true};
  std::vector<WirePacket> packets;
};

struct RuntimeFixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};
  FakeUsbMidiTransport transport{};
  UsbMidiOutput midi{transport};

  RuntimeFixture() {
    engine.setPatternEventQueue(&queue);
    engine.setBpm(120.0f);
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

  void beginRender(uint64_t sequence = 1) {
    queue.beginMidiRenderBlock(sequence, 512, 0.0f,
                               engine.bpm(), engine.sampleRate(), true);
  }

  void endRender() { queue.endMidiRenderBlock(); }

  std::vector<MusicalEvent> drainEvents() {
    std::vector<MusicalEvent> events;
    ScheduledMusicalEvent scheduled{};
    while (queue.tryPop(scheduled)) events.push_back(scheduled.event);
    return events;
  }

  void dispatch(const std::vector<MusicalEvent>& events) {
    for (const MusicalEvent& event : events) midi.handleMusicalEvent(event);
  }
};

void setSingleStep(MiniAcid& engine,
                   int patternIndex,
                   int step,
                   int note,
                   uint8_t velocity = 100,
                   bool slide = false) {
  engine.set303PatternIndex(0, static_cast<int16_t>(patternIndex));
  SynthPattern& pattern = engine.editSynthPattern(0);
  pattern = SynthPattern{};
  pattern.steps[step].note = static_cast<int8_t>(note);
  pattern.steps[step].velocity = velocity;
  pattern.steps[step].slide = slide;
  pattern.steps[step].probability = 100;
}

void triggerCurrentStep(RuntimeFixture& f, int step) {
  f.engine.triggerSynthStep_(0, step);
}

std::size_t countType(const std::vector<MusicalEvent>& events,
                      MusicalEventType type,
                      MusicalEventTarget target = MusicalEventTarget::SynthA) {
  return static_cast<std::size_t>(std::count_if(
      events.begin(), events.end(), [&](const MusicalEvent& event) {
        return event.type == type && event.target == target &&
               event.source == MusicalEventSource::PatternPlayer;
      }));
}

void expectWire(const WirePacket& packet,
                WireType type,
                uint8_t channel,
                uint8_t note) {
  assert(packet.type == type);
  assert(packet.channel == channel);
  assert(packet.note == note);
}

void testAInsideBarGateExpires() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 2, 60, 91);
  f.beginRender(1);
  triggerCurrentStep(f, 2);
  assert(f.noteHeld());
  const long initialGate = f.engine.gateCountdownA_;
  assert(initialGate > 0);
  assert(static_cast<float>(initialGate) < f.engine.samplesPerStep_);

  f.engine.playing = true;
  std::vector<int16_t> audio(static_cast<std::size_t>(initialGate));
  f.engine.generateAudioBuffer(audio.data(), audio.size());
  assert(f.engine.gateCountdownA_ == 0);
  assert(!f.noteHeld());
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 1);
  assert(countType(events, MusicalEventType::NoteOff) == 1);
  std::puts("A PASS: inside-bar note expires through real gate countdown");
}

void testBFinalStepCannotReachEmptyNextBar() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 15, 60, 92);
  f.beginRender(2);
  triggerCurrentStep(f, 15);
  const long initialGate = f.engine.gateCountdownA_;
  assert(initialGate > 0);
  assert(static_cast<float>(initialGate) < f.engine.samplesPerStep_);

  f.engine.playing = true;
  std::vector<int16_t> audio(static_cast<std::size_t>(initialGate));
  f.engine.generateAudioBuffer(audio.data(), audio.size());
  assert(!f.noteHeld());
  assert(f.engine.gateCountdownA_ == 0);
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 1);
  assert(countType(events, MusicalEventType::NoteOff) == 1);
  std::puts("B PASS: step15 ordinary gate is released before an empty next bar");
}

void testCSamePitchBoundaryRetriggers() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 15, 60, 90);
  SynthPattern& pattern = f.engine.editSynthPattern(0);
  pattern.steps[0].note = 60;
  pattern.steps[0].velocity = 111;
  pattern.steps[0].probability = 100;

  f.beginRender(3);
  triggerCurrentStep(f, 15);
  assert(f.noteHeld());
  assert(f.voice()->lastVelocity_ == 90);
  triggerCurrentStep(f, 0);
  assert(f.noteHeld());
  assert(f.voice()->lastVelocity_ == 111);
  assert(!f.voice()->lastSlide_);
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 2);
  f.dispatch(events);
  assert(f.transport.packets.size() == 3);
  expectWire(f.transport.packets[0], WireType::NoteOn, 7, 60);
  expectWire(f.transport.packets[1], WireType::NoteOff, 7, 60);
  expectWire(f.transport.packets[2], WireType::NoteOn, 7, 60);
  std::puts("C PASS: same-pitch boundary is an internal retrigger and MIDI NoteOff/NoteOn replacement");
}

void testDDifferentPitchBoundaryRetriggers() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 15, 60, 90);
  SynthPattern& pattern = f.engine.editSynthPattern(0);
  pattern.steps[0].note = 64;
  pattern.steps[0].velocity = 112;
  pattern.steps[0].probability = 100;

  f.beginRender(4);
  triggerCurrentStep(f, 15);
  const float firstFreq = f.voice()->lastFreqHz_;
  triggerCurrentStep(f, 0);
  assert(f.noteHeld());
  assert(f.voice()->lastVelocity_ == 112);
  assert(f.voice()->lastFreqHz_ != firstFreq);
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 2);
  f.dispatch(events);
  assert(f.transport.packets.size() == 3);
  expectWire(f.transport.packets[0], WireType::NoteOn, 7, 60);
  expectWire(f.transport.packets[1], WireType::NoteOff, 7, 60);
  expectWire(f.transport.packets[2], WireType::NoteOn, 7, 64);
  std::puts("D PASS: different-pitch boundary replaces the active note on both paths");
}

void testESlideAtBoundaryIsStillOnset() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 15, 60, 90);
  SynthPattern& pattern = f.engine.editSynthPattern(0);
  pattern.steps[0].note = 64;
  pattern.steps[0].velocity = 113;
  pattern.steps[0].slide = true;
  pattern.steps[0].probability = 100;

  f.beginRender(5);
  triggerCurrentStep(f, 15);
  triggerCurrentStep(f, 0);
  assert(f.noteHeld());
  assert(f.voice()->lastSlide_);
  assert(f.voice()->lastVelocity_ == 113);
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 2);
  f.dispatch(events);
  assert(f.transport.packets.size() == 3);
  expectWire(f.transport.packets[0], WireType::NoteOn, 7, 60);
  expectWire(f.transport.packets[1], WireType::NoteOff, 7, 60);
  expectWire(f.transport.packets[2], WireType::NoteOn, 7, 64);
  std::puts("E PASS: slide at boundary remains retrigger articulation, not lifetime carry");
}

void testFStopClearsBothLifecycles() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 15, 60, 94);
  f.beginRender(6);
  triggerCurrentStep(f, 15);
  assert(f.noteHeld());
  assert(f.engine.gateCountdownA_ > 0);
  f.engine.playing = true;
  f.engine.stop();
  assert(!f.noteHeld());
  assert(f.engine.gateCountdownA_ == 0);
  assert(f.engine.gateCountdownB_ == 0);
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 1);
  assert(countType(events, MusicalEventType::AllNotesOff,
                   MusicalEventTarget::SynthA) == 1);
  f.dispatch(events);
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 0);
  std::puts("F PASS: Stop clears internal gate/held state and MIDI ownership");
}

void testGExplicitPatternSwitchIsCurrentlyDivergent() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 15, 60, 95);
  setSingleStep(f.engine, 1, 0, 64, 100);
  f.engine.set303PatternIndex(0, 0);

  f.beginRender(7);
  triggerCurrentStep(f, 15);
  assert(f.noteHeld());
  const long gateBefore = f.engine.gateCountdownA_;
  f.engine.playing = true;
  f.engine.set303PatternIndex(0, 1);
  assert(f.noteHeld());
  assert(f.engine.gateCountdownA_ == gateBefore);
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 1);
  assert(countType(events, MusicalEventType::NoteOff) == 1);
  f.dispatch(events);
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 0);
  assert(f.noteHeld());
  std::puts("G PASS: explicit pattern switch releases MIDI while internal held state survives");
}

void testHSongPatternAdvanceIsCurrentlyDivergent() {
  RuntimeFixture f;
  setSingleStep(f.engine, 0, 15, 60, 96);
  setSingleStep(f.engine, 1, 0, 64, 100);

  f.engine.setSongLength(2);
  f.engine.setSongPattern(0, SongTrack::SynthA, songPatternFromBank(0, 0));
  f.engine.setSongPattern(1, SongTrack::SynthA, songPatternFromBank(0, 1));
  f.engine.songMode_ = true;
  f.engine.sceneManager_.setSongMode(true);
  f.engine.songPlaybackSlot_ = f.engine.activeSongSlot();
  f.engine.sceneManager_.setSongPosition(0);
  f.engine.applySongPositionSelection();
  assert(f.engine.current303PatternIndex(0) == 0);

  f.beginRender(8);
  triggerCurrentStep(f, 15);
  assert(f.noteHeld());
  const long gateBefore = f.engine.gateCountdownA_;
  f.engine.playing = true;
  f.engine.sceneManager_.setSongPosition(1);
  f.engine.applySongPositionSelection();
  assert(f.engine.current303PatternIndex(0) == 1);
  assert(f.noteHeld());
  assert(f.engine.gateCountdownA_ == gateBefore);
  f.endRender();

  const std::vector<MusicalEvent> events = f.drainEvents();
  assert(countType(events, MusicalEventType::NoteOn) == 1);
  assert(countType(events, MusicalEventType::AllNotesOff,
                   MusicalEventTarget::SynthA) == 1);
  f.dispatch(events);
  assert(f.midi.activeGateCount(MusicalEventSource::PatternPlayer,
                                MusicalEventTarget::SynthA, 0) == 0);
  assert(f.noteHeld());
  std::puts("H PASS: Song pattern advance releases MIDI but not the direct internal PatternPlayer voice");
}

}  // namespace

int main() {
  testAInsideBarGateExpires();
  testBFinalStepCannotReachEmptyNextBar();
  testCSamePitchBoundaryRetriggers();
  testDDifferentPitchBoundaryRetriggers();
  testESlideAtBoundaryIsStillOnset();
  testFStopClearsBothLifecycles();
  testGExplicitPatternSwitchIsCurrentlyDivergent();
  testHSongPatternAdvanceIsCurrentlyDivergent();
  std::puts("M2-A1 runtime characterization: OK");
  return 0;
}
