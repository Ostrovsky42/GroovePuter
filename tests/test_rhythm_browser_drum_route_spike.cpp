#include "src/dsp/pattern_drum_event_tap.h"
#include "src/midi/pattern_drum_gate_scheduler.h"
#include <cassert>
#include <cstdio>
#include <memory>

namespace {
class CountingVoice final : public DrumSynthVoice {
 public:
  int triggers[8]{};
  Parameter value{};
  void reset() override {}
  void setSampleRate(float) override {}
  void triggerKick(bool, uint8_t) override { ++triggers[0]; }
  void triggerSnare(bool, uint8_t) override { ++triggers[1]; }
  void triggerHat(bool, uint8_t) override { ++triggers[2]; }
  void triggerOpenHat(bool, uint8_t) override { ++triggers[3]; }
  void triggerMidTom(bool, uint8_t) override { ++triggers[4]; }
  void triggerHighTom(bool, uint8_t) override { ++triggers[5]; }
  void triggerRim(bool, uint8_t) override { ++triggers[6]; }
  void triggerClap(bool, uint8_t) override { ++triggers[7]; }
  void triggerCymbal(bool, uint8_t) override {}
  float processKick() override { return 0; }
  float processSnare() override { return 0; }
  float processHat() override { return 0; }
  float processOpenHat() override { return 0; }
  float processMidTom() override { return 0; }
  float processHighTom() override { return 0; }
  float processRim() override { return 0; }
  float processClap() override { return 0; }
  float processCymbal() override { return 0; }
  const Parameter& parameter(DrumParamId) const override { return value; }
  void setParameter(DrumParamId, float) override {}
  void setLoFiMode(bool) override {}
  void setLoFiAmount(float) override {}
};

float phase(void*) { return 0.0f; }

void checkMode(GroovePuterOutput::Mode mode, bool local, bool midi) {
  using namespace GroovePuterOutput;
  restoreLegacyCompatibility(Track::Drums);
  assert(setMode(Track::Drums, mode));
  MusicalEventQueue queue;
  PatternEventQueueHandle handle;
  handle = &queue;
  queue.setPhaseReader(phase, nullptr);
  queue.beginMidiRenderBlock(7, 512, 0.0f, 120.0f, 22050.0f, true);
  auto owned = std::make_unique<CountingVoice>();
  CountingVoice* observed = owned.get();
  PatternPublishingDrumVoice voice(std::move(owned));
  PatternDrumGateScheduler gates;
  for (uint8_t lane = 0; lane < 8; ++lane) {
    voice.triggerPattern(lane, false, static_cast<uint8_t>(80 + lane));
    assert(observed->triggers[lane] == (local ? 1 : 0));
    ScheduledMusicalEvent event{};
    const bool received = queue.tryPop(event);
    assert(received == midi);
    if (received) {
      assert(event.event.source == MusicalEventSource::PatternPlayer);
      assert(event.event.target == MusicalEventTarget::Drums);
      assert(event.event.channel == lane && event.event.velocity == 80 + lane);
      assert(gates.scheduleOrExtend(event, 80, 22050, 512));
    }
  }
  assert(gates.activeCount() == (midi ? 8u : 0u));
  gates.clear();
  assert(gates.activeCount() == 0);
  queue.endMidiRenderBlock();
  handle = nullptr;
  restoreLegacyCompatibility(Track::Drums);
}
}  // namespace

int main() {
  checkMode(GroovePuterOutput::Mode::Internal, true, false);
  checkMode(GroovePuterOutput::Mode::Midi, false, true);
  checkMode(GroovePuterOutput::Mode::Layer, true, true);
  std::puts("Pattern drum output modes and gates: PASS");
}
