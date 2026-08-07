#include "src/dsp/ay_synth_voice.h"
#include "src/dsp/sh101_synth_voice.h"
#include "src/dsp/sn76489_synth_voice.h"
#include "src/dsp/swappable_synth_voice.h"
#include "src/dsp/wave_morph_synth_voice.h"

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

void verifyNativeDefaults() {
  constexpr float kSampleRate = 22050.0f;
  const char* names[] = {"AY", "SH101", "SN76489", "WAVEMORPH"};

  for (const char* name : names) {
    SwappableSynthVoice voice(kSampleRate, SynthEngineType::TB303);
    voice.setEngineName(name);
    const SynthVoiceState state = voice.getState();
    assert(std::strcmp(voice.getEngineName(), name) == 0);
    assert(state.paramCount == voice.parameterCount());
    bool anyBelowOne = false;
    for (uint8_t p = 0; p < state.paramCount; ++p) {
      assert(std::isfinite(state.params[p]));
      assert(state.params[p] >= 0.0f && state.params[p] <= 1.0f);
      if (state.params[p] < 0.999f) anyBelowOne = true;
    }
    assert(anyBelowOne);
  }

  // The old TB303 raw cutoff 800 would clamp to normalized 1.0 on AY Noise.
  // A fresh AY must keep its own neutral constructor default instead.
  SwappableSynthVoice ay(kSampleRate, SynthEngineType::TB303);
  ay.setEngineName("AY");
  assert(ay.parameterCount() >= 1);
  assert(ay.getParameterNormalized(0) < 0.25f);
}

void verifySixParameterStateRoundTrip(const char* engineName) {
  constexpr float kSampleRate = 22050.0f;
  SwappableSynthVoice source(kSampleRate, SynthEngineType::TB303);
  source.setEngineName(engineName);
  assert(source.parameterCount() == 6);

  const float values[6] = {0.07f, 0.19f, 0.33f, 0.51f, 0.72f, 0.91f};
  for (uint8_t p = 0; p < 6; ++p) {
    source.setParameterNormalized(p, values[p]);
  }

  const SynthVoiceState persisted = source.getState();
  assert(persisted.paramCount == 6);
  assert(std::fabs(persisted.params[5] - values[5]) < 0.02f);

  SwappableSynthVoice restored(kSampleRate, SynthEngineType::TB303);
  restored.setState(persisted);
  assert(std::strcmp(restored.getEngineName(), engineName) == 0);
  assert(restored.parameterCount() == 6);
  for (uint8_t p = 0; p < 6; ++p) {
    assert(std::fabs(restored.getParameterNormalized(p) -
                     source.getParameterNormalized(p)) < 0.02f);
  }
}

int main() {
  verifyVoice<Sh101SynthVoice>("SH101");
  verifyVoice<Sn76489SynthVoice>("SN76489");
  verifyNativeDefaults();
  verifySixParameterStateRoundTrip("SH101");
  verifySixParameterStateRoundTrip("SN76489");
  verifySixParameterStateRoundTrip("WAVEMORPH");
  verifyRemovedOpl2Fallback();
  return 0;
}
