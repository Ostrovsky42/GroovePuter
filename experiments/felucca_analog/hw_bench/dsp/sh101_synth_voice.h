#pragma once

#include "mono_synth_voice.h"

#include <cstdint>

class Sh101SynthVoice final : public IMonoSynthVoice {
 public:
  explicit Sh101SynthVoice(float sampleRate);
  ~Sh101SynthVoice() override = default;

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
  void release() override;
  float process() override;

  uint8_t parameterCount() const override { return 6; }
  void setParameterNormalized(uint8_t index, float norm) override;
  float getParameterNormalized(uint8_t index) const override;
  const Parameter& getParameter(uint8_t index) const override;

  const char* getEngineName() const override { return "SH101"; }
  void setMode(GrooveboxMode mode) override;
  void setLoFiAmount(float amount) override;

 private:
  static float clamp01(float value);
  static float polyBlep(float phase, float phaseIncrement);
  static float fastSaturate(float value);

  float renderSaw(float phase, float phaseIncrement) const;
  float renderPulse(float phase, float phaseIncrement, float duty) const;
  float renderOscillator(float phaseIncrement);
  float renderSub(float phaseIncrement);
  float nextNoise();
  void advancePhase(float& phase, float increment);
  void updateEnvelopeCoefficients();

  Parameter params_[6];
  float sampleRate_ = 44100.0f;
  float invSampleRate_ = 1.0f / 44100.0f;
  float currentFreqHz_ = 110.0f;
  float targetFreqHz_ = 110.0f;

  float phase_ = 0.0f;
  float subPhase_ = 0.0f;
  float ampEnv_ = 0.0f;
  float filterEnv_ = 0.0f;
  float velocityGain_ = 0.8f;
  float decayCoef_ = 0.999f;
  float releaseCoef_ = 0.995f;
  bool gate_ = false;
  bool attack_ = false;
  bool slide_ = false;

  float filter1_ = 0.0f;
  float filter2_ = 0.0f;
  float dcInput_ = 0.0f;
  float dcOutput_ = 0.0f;
  uint32_t noiseState_ = 0x6D2B79F5u;

  GrooveboxMode mode_ = GrooveboxMode::Acid;
  float loFiAmount_ = 0.0f;
};
