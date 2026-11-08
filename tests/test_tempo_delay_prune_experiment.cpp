#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
SerialMock Serial;
SDMock SD;
int main() {
  TempoDelay delay(kSampleRate);
  delay.init(0.1f);
  delay.setMix(0.5f);
  delay.setFeedback(0.25f);
  delay.setEnabled(true);
  double echoEnergy = 0;
  for (int i = 0; i < kSampleRate; ++i) {
    const float value = delay.process(i == 0 ? 1.0f : 0.0f);
    if (i > 0) echoEnergy += value * value;
  }
  const size_t bytes = delay.buffer.capacity() * sizeof(float);
#if GROOVEPUTER_EXPERIMENT_NO_TEMPO_DELAY
  assert(bytes == 0 && echoEnergy == 0);
#else
  assert(bytes == 8820 && echoEnergy > 0.01);
#endif
  std::printf("no_delay=%d one_buffer_bytes=%zu two_buffer_payload=%zu echo_energy=%.8f\n",
      GROOVEPUTER_EXPERIMENT_NO_TEMPO_DELAY, bytes, 2 * bytes, echoEnergy);
}
