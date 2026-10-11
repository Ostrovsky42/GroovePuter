#include <cassert>
#include <cstdio>

#include "src/dsp/drum_bus_lofi.h"

int main() {
  DrumBusLoFi effect;
  const float sample = 0.12345f;
  assert(effect.process(sample) == sample);
  effect.configure(true, 1.0f);
  const float crushed = effect.process(sample);
  assert(crushed != sample);
  assert(effect.process(0.0f) == crushed);  // sample hold
  effect.configure(false, 1.0f);
  assert(effect.process(sample) == sample);
  effect.configure(true, 0.0f);
  assert(effect.process(sample) == sample);
  std::puts("drum bus LoFi: PASS");
}
