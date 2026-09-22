#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

// Characterization only. Open current private runtime state from this host-test
// translation unit instead of adding a production probe/seam.
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/input/musical_event_queue.h"
#include "src/midi/tee_midi_transport.h"
#include "src/midi/usb_midi_output.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMidi::TeeMidiTransport;

enum class WireType : uint8_t {
  NoteOn,
  NoteOff,
  ControlChange,
  Clock,
  Start,
  Continue,
  Stop,
  SongPosition,
};

struct WirePacket {
  WireType type{};
  uint8_t channel{0};
  uint8_t data1{0};
  uint8_t data2{0};
};

bool samePackets(const std::vector<WirePacket>& a,
                 const std::vector<WirePacket>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].type != b[i].type ||
        a[i].channel != b[i].channel ||
        a[i].data1 != b[i].data1 ||
        a[i].data2 != b[i].data2) {
      return false;
    }
  }
  return true;
}

class FakeTransport final : public IMidiTransport {
 public:
  explicit FakeTransport(
      MidiTransportLink link = MidiTransportLink::Enumerated)
      : link_(link) {}

  bool begin() override {
    begun = true;
    return true;
  }

  bool mounted() const override { return mountedFlag; }
  MidiTransportLink linkKind() const override { return link_; }

  bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
    packets.push_back({WireType::NoteOn, channel, note, velocity});
    return true;
  }

  bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
    packets.push_back({WireType::NoteOff, channel, note, velocity});
    return true;
  }

  bool sendControlChange(uint8_t channel,
                         uint8_t controller,
                         uint8_t value) override {
    packets.push_back({WireType::ControlChange, channel, controller, value});
    return true;
  }

  bool sendTimingClock() override {
    packets.push_back({WireType::Clock, 0, 0, 0});
    return true;
  }

  bool sendStart() override {
    packets.push_back({WireType::Start, 0, 0, 0});
    return true;
  }

  bool sendContinue() override {
    packets.push_back({WireType::Continue, 0, 0, 0});
    return true;
  }

  bool sendStop() override {
    packets.push_back({WireType::Stop, 0, 0, 0});
    return true;
  }

  bool sendSongPositionPointer(uint16_t midiBeats) override {
    packets.push_back({
        WireType::SongPosition,
        0,
        static_cast<uint8_t>(midiBeats & 0x7f),
        static_cast<uint8_t>((midiBeats >> 7) & 0x7f),
    });
    return true;
  }

  void flush() override {}

  bool begun{false};
  bool mountedFlag{true};
  std::vector<WirePacket> packets;

 private:
  MidiTransportLink link_;
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
  FakeTransport usb{MidiTransportLink::Enumerated};
  FakeTransport din{MidiTransportLink::Unverifiable};
  TeeMidiTransport tee{usb, din};
  UsbMidiOutput midi{tee};
  uint32_t blockSequence{1};

  RuntimeFixture() {
    tee.setSecondaryEnabled(true);
    engine.setBpm(120.0f);
    engine.setPatternEventQueue(&queue);
    queue.setPhaseReader(readEnginePhase, &engine);
    assert(midi.begin());
    midi.pollConnection();
    usb.packets.clear();
    din.packets.clear();
  }

  SwappableSynthVoice* voice(int synth) {
    return engine.synthVoices_[synth].get();
  }

  bool noteHeld(int synth) {
    SwappableSynthVoice* current = voice(synth);
    return current && current->noteHeld_;
  }

  long gateCountdown(int synth) const {
    return synth == 0 ? engine.gateCountdownA_ : engine.gateCountdownB_;
  }

  void beginRender(uint16_t frames = 512) {
    queue.beginMidiRenderBlock(
        blockSequence++,
        frames,
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
    dispatchPendingPanics();
    dispatchQueuedCurrentGeneration();
  }

  void assertEndpointParity() const {
    assert(samePackets(usb.packets, din.packets));
  }
};

void clearPattern(SynthPattern& pattern) {
  pattern = SynthPattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = -1;
    pattern.steps[step].timing = 0;
    pattern.steps[step].probability = 100;
    pattern.steps[step].ghost = false;
  }
}

void setStep(SynthPattern& pattern,
             int step,
             int note,
             uint8_t velocity = 100,
             int8_t timing = 0) {
  assert(step >= 0 && step < SynthPattern::kSteps);
  pattern.steps[step].note = static_cast<int8_t>(note);
  pattern.steps[step].velocity = velocity;
  pattern.steps[step].timing = timing;
  pattern.steps[step].slide = false;
  pattern.steps[step].accent = false;
  pattern.steps[step].ghost = false;
  pattern.steps[step].probability = 100;
  pattern.steps[step].fx = 0;
  pattern.steps[step].fxParam = 0;
}

void editPattern(RuntimeFixture& f,
                 int synth,
                 int patternIndex,
                 const std::vector<int>& notes,
                 const std::vector<int8_t>& timings = {}) {
  f.engine.set303PatternIndex(synth, static_cast<int16_t>(patternIndex));
  SynthPattern& pattern = f.engine.editSynthPattern(synth);
  clearPattern(pattern);
  for (std::size_t i = 0; i + 1 < notes.size(); i += 2) {
    const int step = notes[i];
    const int note = notes[i + 1];
    const int8_t timing = timings.empty()
        ? 0
        : timings[static_cast<std::size_t>(step)];
    setStep(pattern, step, note, 100, timing);
  }
}

MusicalEventTarget targetForSynth(int synth) {
  return synth == 0 ? MusicalEventTarget::SynthA : MusicalEventTarget::SynthB;
}

SongTrack songTrackForSynth(int synth) {
  return synth == 0 ? SongTrack::SynthA : SongTrack::SynthB;
}

uint8_t channelForSynth(int synth) {
  return synth == 0 ? 7 : 8;
}

std::size_t countWire(const std::vector<WirePacket>& packets,
                      WireType type,
                      uint8_t channel) {
  return static_cast<std::size_t>(std::count_if(
      packets.begin(), packets.end(), [&](const WirePacket& packet) {
        return packet.type == type && packet.channel == channel;
      }));
}

void processTick(RuntimeFixture& f, uint32_t absoluteTick) {
  f.engine.currentTick_ = absoluteTick;
  f.engine.processSequencerEvents(absoluteTick);
}

void renderSteps(RuntimeFixture& f,
                 uint32_t tickBeforeFirstTrigger,
                 float stepSpan) {
  f.engine.currentTick_ = tickBeforeFirstTrigger;
  f.engine.tickPhaseAccum_ = 0x100000000ULL;
  f.engine.playing = true;

  std::size_t remaining = static_cast<std::size_t>(
      f.engine.samplesPerStep_ * stepSpan + 0.5f);
  while (remaining > 0) {
    const uint16_t block = static_cast<uint16_t>(
        std::min<std::size_t>(remaining, 4096));
    f.beginRender(block);
    std::vector<int16_t> audio(block);
    f.engine.generateAudioBuffer(audio.data(), audio.size());
    f.endRender();
    f.dispatchLikeProduction();
    remaining -= block;
  }
}

std::vector<WirePacket> gridTrace(int synth, uint8_t gridSteps) {
  RuntimeFixture f;
  f.engine.songMode_ = false;
  f.engine.sceneManager_.setSongMode(false);
  f.engine.sceneManager_.currentScene().feel.gridSteps = gridSteps;

  editPattern(f, synth, 0, {0, 60, 4, 62, 8, 64, 12, 65});
  editPattern(f, 1 - synth, 0, {});
  f.engine.set303PatternIndex(synth, 0);
  f.engine.set303PatternIndex(1 - synth, 0);

  renderSteps(f, 383, 15.5f);
  f.assertEndpointParity();
  assert(countWire(f.usb.packets, WireType::NoteOn, channelForSynth(synth)) == 4);
  return f.usb.packets;
}

void testGridStepsAreSchedulerNoOp() {
  for (int synth = 0; synth < 2; ++synth) {
    const auto grid8 = gridTrace(synth, 8);
    const auto grid16 = gridTrace(synth, 16);
    const auto grid32 = gridTrace(synth, 32);
    assert(samePackets(grid8, grid16));
    assert(samePackets(grid16, grid32));
  }
  std::puts("P0-A PASS: GRID 8/16/32 is a synth scheduler no-op");
}

void testExact384TickBoundary() {
  RuntimeFixture f;
  f.engine.songMode_ = false;
  f.engine.sceneManager_.setSongMode(false);
  editPattern(f, 0, 0, {});
  editPattern(f, 1, 0, {});

  processTick(f, 383);
  assert(f.engine.currentStepIndex == 15);
  processTick(f, 384);
  assert(f.engine.currentStepIndex == 0);
  processTick(f, 767);
  assert(f.engine.currentStepIndex == 15);
  processTick(f, 768);
  assert(f.engine.currentStepIndex == 0);

  std::puts("P0-B PASS: physical bar boundary is exactly 384 ticks");
}

void testSongBoundaryCleanupDivergenceForSynth(int synth) {
  RuntimeFixture f;
  editPattern(f, synth, 0, {15, 60});
  editPattern(f, synth, 1, {4, 64});
  editPattern(f, 1 - synth, 0, {});
  editPattern(f, 1 - synth, 1, {});

  f.engine.setSongLength(2);
  f.engine.setSongPattern(0, songTrackForSynth(synth), songPatternFromBank(0, 0));
  f.engine.setSongPattern(1, songTrackForSynth(synth), songPatternFromBank(0, 1));
  f.engine.songMode_ = true;
  f.engine.sceneManager_.setSongMode(true);
  f.engine.songPlaybackSlot_ = f.engine.activeSongSlot();
  f.engine.sceneManager_.currentScene().feel.patternBars = 1;
  f.engine.sceneManager_.setSongPosition(0);
  f.engine.applySongPositionSelection();
  assert(f.engine.current303PatternIndex(synth) == 0);

  f.engine.playing = true;
  f.beginRender();
  f.engine.triggerSynthStep_(synth, 15);
  f.endRender();
  f.dispatchLikeProduction();

  assert(f.noteHeld(synth));
  const long gateBefore = f.gateCountdown(synth);
  assert(gateBefore > 0);
  assert(f.midi.activeGateCount(
             MusicalEventSource::PatternPlayer,
             targetForSynth(synth), 0) == 1);

  f.engine.songBarIndex_ = 0;
  f.beginRender();
  processTick(f, 384);
  f.endRender();
  f.dispatchLikeProduction();

  assert(f.engine.currentSongPosition() == 1);
  assert(f.engine.current303PatternIndex(synth) == 1);
  // Current 0.9.10 baseline is intentionally asymmetric: Song selection
  // publishes PatternPlayer MIDI cleanup, but direct internal voice lifetime is
  // not synchronously released by applySongPositionSelection().
  assert(f.noteHeld(synth));
  assert(f.gateCountdown(synth) == gateBefore);
  assert(f.midi.activeGateCount(
             MusicalEventSource::PatternPlayer,
             targetForSynth(synth), 0) == 0);
  assert(countWire(f.usb.packets, WireType::NoteOn, channelForSynth(synth)) == 1);
  assert(countWire(f.usb.packets, WireType::NoteOff, channelForSynth(synth)) == 1);
  f.assertEndpointParity();
}

void testSongBoundaryCleanupDivergence() {
  testSongBoundaryCleanupDivergenceForSynth(0);
  testSongBoundaryCleanupDivergenceForSynth(1);
  std::puts("P0-C PASS: Song row @384 cleans MIDI while direct internal gate survives");
}

void testLegacyTieCanExtendAnActiveGateAcrossBoundaryForSynth(int synth) {
  RuntimeFixture f;
  std::vector<int8_t> timing(SynthPattern::kSteps, 0);
  timing[15] = 23;
  timing[0] = 1;
  editPattern(f, synth, 0, {15, 60, 0, -2}, timing);
  editPattern(f, 1 - synth, 0, {});
  f.engine.set303PatternIndex(synth, 0);
  f.engine.songMode_ = false;
  f.engine.sceneManager_.setSongMode(false);
  f.engine.playing = true;

  f.beginRender();
  processTick(f, 383);  // step15 onset shifted +23 ticks
  const long gateBeforeTie = f.gateCountdown(synth);
  assert(gateBeforeTie > 0);
  processTick(f, 384);  // exact physical boundary
  assert(f.gateCountdown(synth) == gateBeforeTie);
  processTick(f, 385);  // step0 TIE shifted to barTick 1
  const long gateAfterTie = f.gateCountdown(synth);
  f.endRender();
  f.dispatchLikeProduction();

  assert(gateAfterTie > gateBeforeTie);
  assert(f.engine.patternMidiNotes_[synth] == 60);
  assert(countWire(f.usb.packets, WireType::NoteOn, channelForSynth(synth)) == 1);
  assert(countWire(f.usb.packets, WireType::NoteOff, channelForSynth(synth)) == 0);
  f.assertEndpointParity();
}

void testLegacyTieCrossingSymptom() {
  testLegacyTieCanExtendAnActiveGateAcrossBoundaryForSynth(0);
  testLegacyTieCanExtendAnActiveGateAcrossBoundaryForSynth(1);
  std::puts("P0-D PASS: legacy TIE can extend an already-active gate across 384");
}

void configureSwing(RuntimeFixture& f, int synth, uint8_t swingPct) {
  Scene& scene = f.engine.sceneManager_.currentScene();
  scene.feel.swingPct = swingPct;
  scene.feel.swingMask = static_cast<uint16_t>(
      1u << static_cast<int>(synth == 0 ? VoiceId::SynthA : VoiceId::SynthB));
}

void testSwingAndMicrotimingModuloForSynth(int synth) {
  {
    RuntimeFixture f;
    editPattern(f, synth, 0, {15, 60});
    editPattern(f, 1 - synth, 0, {});
    f.engine.set303PatternIndex(synth, 0);
    f.engine.songMode_ = false;
    f.engine.sceneManager_.setSongMode(false);
    configureSwing(f, synth, 75);
    f.engine.playing = true;

    f.beginRender();
    processTick(f, 371);
    assert(f.gateCountdown(synth) == 0);
    processTick(f, 372);  // 360 + max swing delay 12
    assert(f.gateCountdown(synth) > 0);
    f.endRender();
    f.dispatchLikeProduction();
    f.assertEndpointParity();
  }

  {
    RuntimeFixture f;
    std::vector<int8_t> timing(SynthPattern::kSteps, 0);
    timing[15] = 23;
    editPattern(f, synth, 0, {15, 60}, timing);
    editPattern(f, 1 - synth, 0, {});
    f.engine.set303PatternIndex(synth, 0);
    f.engine.songMode_ = false;
    f.engine.sceneManager_.setSongMode(false);
    configureSwing(f, synth, 75);
    f.engine.playing = true;

    f.beginRender();
    processTick(f, 10);
    assert(f.gateCountdown(synth) == 0);
    processTick(f, 11);  // (360 + 12 + 23) % 384
    assert(f.gateCountdown(synth) > 0);
    f.endRender();
    f.dispatchLikeProduction();
    f.assertEndpointParity();
  }
}

void testSwingPlusMicrotimingWrapsLateStep() {
  testSwingAndMicrotimingModuloForSynth(0);
  testSwingAndMicrotimingModuloForSynth(1);
  std::puts("P0-E PASS: swing alone reaches 372; swing+micro wraps step15 to tick11");
}

void testNegativeMicrotimingWrapsStepZeroForSynth(int synth) {
  RuntimeFixture f;
  std::vector<int8_t> timing(SynthPattern::kSteps, 0);
  timing[0] = -23;
  editPattern(f, synth, 0, {0, 60}, timing);
  editPattern(f, 1 - synth, 0, {});
  f.engine.set303PatternIndex(synth, 0);
  f.engine.songMode_ = false;
  f.engine.sceneManager_.setSongMode(false);
  f.engine.playing = true;

  f.beginRender();
  processTick(f, 360);
  assert(f.gateCountdown(synth) == 0);
  processTick(f, 361);  // (0 - 23 + 384) % 384
  assert(f.gateCountdown(synth) > 0);
  f.endRender();
  f.dispatchLikeProduction();
  f.assertEndpointParity();
}

void testNegativeMicrotimingWrapsStepZero() {
  testNegativeMicrotimingWrapsStepZeroForSynth(0);
  testNegativeMicrotimingWrapsStepZeroForSynth(1);
  std::puts("P0-F PASS: step0 microtiming -23 wraps to barTick361");
}

}  // namespace

int main() {
  testGridStepsAreSchedulerNoOp();
  testExact384TickBoundary();
  testSongBoundaryCleanupDivergence();
  testLegacyTieCrossingSymptom();
  testSwingPlusMicrotimingWrapsLateStep();
  testNegativeMicrotimingWrapsStepZero();
  std::puts("PATTERN/PHRASE P0 runtime characterization: OK");
  return 0;
}
