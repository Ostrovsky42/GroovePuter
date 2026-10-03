// A played note must pulse the LED in StepTrig mode for the selected source, like a sequenced one.
#include <cassert>
#include <cstdio>

#include "src/dsp/miniacid_engine.h"
#include "src/ui/led_manager.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

int main() {
  MiniAcid engine(44100.0f, nullptr);
  LedSettings& led = engine.sceneManager().currentScene().led;
  led.mode = LedMode::StepTrig;
  led.source = VoiceId::SynthA;
  LedManager& manager = LedManager::instance();
  manager.update();  // drain anything pending
  assert(!manager.hasPendingPulse());

  engine.liveNoteOn(0, 60, 100);
  assert(manager.hasPendingPulse());          // Synth A note pulses the Synth A LED
  manager.update();
  assert(!manager.hasPendingPulse());

  engine.liveNoteOn(1, 60, 100);
  assert(!manager.hasPendingPulse());         // Synth B is not the selected source
  manager.update();

  led.mode = LedMode::Off;
  engine.liveNoteOn(0, 62, 100);
  assert(!manager.hasPendingPulse());         // the LED mode still decides
  std::puts("live note pulses the LED: PASS");
  return 0;
}
