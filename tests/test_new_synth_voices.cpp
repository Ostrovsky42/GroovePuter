#include "src/dsp/sh101_synth_voice.h"
#include "src/dsp/sn76489_synth_voice.h"
#include "src/dsp/swappable_synth_voice.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

template <typename Voice>
void verifyVoice(const char* expectedName) {
  constexpr float kSampleRate = 22050.0f;
  Voice voice(kSampleRate);

  assert(std::strcmp(voice.getEngineName(), expectedName) == 0);
  assert(voice.parameterCount() == 6);

  for (uint8_t i = 0; i < voice.parameterCount(); ++i) {
    voice.setParameterNormalized(i, 0.0f);
    assert(voice.getParameterNormalized(i) >= 0.0f);
    voice.setParameterNormalized(i, 1.0f);
    assert(voice.getParameterNormalized(i) <= 1.0f);
  }

  voice.reset();
  voice.startNote(220.0f, false, false, 100);
  float peak = 0.0f;
  for (int i = 0; i < 4096; ++i) {
    const float sample = voice.process();
    assert(std::isfinite(sample));
    assert(std::fabs(sample) <= 1.0001f);
    peak = std::max(peak, std::fabs(sample));
  }
  assert(peak > 0.001f);

  // A slide flag on the first note must still start the voice rather than
  // gliding an inactive zero envelope.
  voice.reset();
  voice.startNote(330.0f, false, true, 100);
  peak = 0.0f;
  for (int i = 0; i < 2048; ++i) {
    const float sample = voice.process();
    assert(std::isfinite(sample));
    assert(std::fabs(sample) <= 1.0001f);
    peak = std::max(peak, std::fabs(sample));
  }
  assert(peak > 0.001f);

  voice.release();
  float tailPeak = 0.0f;
  for (int i = 0; i < 22050; ++i) {
    const float sample = voice.process();
    assert(std::isfinite(sample));
    assert(std::fabs(sample) <= 1.0001f);
    if (i >= 20000) tailPeak = std::max(tailPeak, std::fabs(sample));
  }
  assert(tailPeak < 0.002f);
}

void verifyRemovedOpl2Fallback() {
  constexpr float kSampleRate = 22050.0f;

  SwappableSynthVoice legacyEnum(kSampleRate, SynthEngineType::OPL2);
  assert(std::strcmp(legacyEnum.getEngineName(), "TB303") == 0);
  assert(legacyEnum.engineType() == SynthEngineType::TB303);

  legacyEnum.setEngineName("OPL2");
  assert(std::strcmp(legacyEnum.getEngineName(), "TB303") == 0);
  assert(legacyEnum.engineType() == SynthEngineType::TB303);

  SynthVoiceState persisted{};
  persisted.engineType = SynthEngineType::OPL2;
  persisted.paramCount = 0;
  legacyEnum.setState(persisted);
  assert(std::strcmp(legacyEnum.getEngineName(), "TB303") == 0);
  assert(legacyEnum.engineType() == SynthEngineType::TB303);
}

int main() {
  verifyVoice<Sh101SynthVoice>("SH101");
  verifyVoice<Sn76489SynthVoice>("SN76489");
  verifyRemovedOpl2Fallback();
  return 0;
}
