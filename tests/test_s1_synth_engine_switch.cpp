// 0.9.19 S1: a synth engine switch never frees or replaces an engine in the
// audio thread. Before, SwappableSynthVoice::process() ended the crossfade by
// moving next_ into current_ (freeing the old engine) and then updating type_,
// while the UI read engineType() + activeVoice() and Parameter references
// without the audio guard: a type/object mismatch and a use-after-free window
// (the coredump-free reboot of 2026-10-10 after rapid Synth B switches).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <typeinfo>

#include "src/dsp/mini_tb303.h"
#include "src/dsp/swappable_synth_voice.h"

namespace {

constexpr float kRate = 22050.0f;
const SynthEngineType kCycle[] = {SynthEngineType::TB303, SynthEngineType::SID,
                                  SynthEngineType::AY, SynthEngineType::SH101,
                                  SynthEngineType::SN76489, SynthEngineType::WAVEMORPH};

bool objectMatchesType(const SwappableSynthVoice& v) {
  const IMonoSynthVoice* active = v.activeVoice();
  if (active == nullptr) return false;
  const bool isTb303 = dynamic_cast<const TB303Voice*>(active) != nullptr;
  return isTb303 == (v.engineType() == SynthEngineType::TB303);
}

void testTypeAndObjectAgreeThroughASwitch() {
  SwappableSynthVoice v(kRate, SynthEngineType::TB303);
  v.startNote(110.0f, false, false, 100);
  v.setEngineType(SynthEngineType::SID);
  assert(v.switchPending() && !v.switchSettled());
  assert(v.engineType() == SynthEngineType::SID && objectMatchesType(v));
  const IMonoSynthVoice* incoming = v.activeVoice();
  for (int i = 0; i < 2000; ++i) {
    (void)v.process();
    // The audio thread may end the crossfade at any sample; the UI view must
    // stay on the incoming engine and the object must not change under it.
    assert(objectMatchesType(v));
    assert(v.activeVoice() == incoming);
  }
  assert(v.switchPending() && v.switchSettled());  // settled, not committed
  v.commitSwitch();                                  // control thread
  assert(!v.switchPending() && !v.switchSettled());
  assert(v.engineType() == SynthEngineType::SID && v.activeVoice() == incoming);
  std::puts("S1: during and after a crossfade the engine type and object agree; process() frees nothing: PASS");
}

void testRapidSwitchesAndCommits() {
  SwappableSynthVoice v(kRate, SynthEngineType::TB303);
  v.startNote(220.0f, true, false, 100);
  for (int round = 0; round < 100; ++round) {
    for (SynthEngineType t : kCycle) {
      v.setEngineType(t);
      // shorter and longer than the 10 ms (220-sample) crossfade
      const int samples = (round % 3 == 0) ? 0 : (round % 3 == 1 ? 50 : 400);
      for (int i = 0; i < samples; ++i) {
        (void)v.process();
        assert(objectMatchesType(v));
      }
      if (v.switchSettled()) v.commitSwitch();
      assert(objectMatchesType(v));
    }
  }
  for (int i = 0; i < 500; ++i) (void)v.process();
  if (v.switchSettled()) v.commitSwitch();
  assert(!v.switchPending());
  assert(v.engineType() == SynthEngineType::WAVEMORPH && objectMatchesType(v));
  std::puts("S1: 600 rapid switches (0/50/400 samples apart) keep type and object consistent: PASS");
}

void testUnrenderedSwitchCanBeCommitted() {
  // A muted synth is not rendered, so its crossfade never advances; the engine
  // commits it anyway (MiniAcid::commitSettledSynthEngineSwitches).
  SwappableSynthVoice v(kRate, SynthEngineType::TB303);
  v.setEngineType(SynthEngineType::AY);
  assert(v.switchPending() && !v.switchSettled());
  v.commitSwitch();
  assert(!v.switchPending() && v.engineType() == SynthEngineType::AY && objectMatchesType(v));
  std::puts("S1: a switch that was never rendered commits cleanly: PASS");
}

}  // namespace

int main() {
  testTypeAndObjectAgreeThroughASwitch();
  testRapidSwitchesAndCommits();
  testUnrenderedSwitchCanBeCommitted();
  std::puts("S1 synth engine switch: GREEN");
  return 0;
}
