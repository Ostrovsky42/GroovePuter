#pragma once

#include "mono_synth_voice.h"

#include <cstdint>

class Sn76489SynthVoice final : public IMonoSynthVoice {
 public:
  explicit Sn76489SynthVoice(float sampleRate);
  ~Sn76489SynthVoice() override = default;

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
  void release() override;
  float process() override;

  uint8_t parameterCount() const override { return 6; }
  void setParameterNormalized(uint8_t index, float norm) override;
  float getParameterNormalized(uint8_t index) const override;
  const Parameter& getParameter(uint8_t index) const override;

  const char* getEngineName() const override { return "SN76489"; }
  void setMode(GrooveboxMode mode) override;
  void setLoFiAmount(float amount) override;

 private:
  static float clamp01(float value);
  static float polyBlep(float phase, float phaseIncrement);

  float quantizeToneFrequency(float hz) const;
  float renderSquare(float phase, float phaseIncrement) const;
  bool advancePhase(float& phase, float increment);
  void updateToneFrequencies();
  void shiftNoise(bool whiteNoise);
  void updateEnvelopeCoefficients();

  Parameter params_[6];
  float sampleRate_ = 44100.0f;
  float invSampleRate_ = 1.0f / 44100.0f;
  float currentFreqHz_ = 220.0f;
  float targetFreqHz_ = 220.0f;
  float toneFreq_[3] = {220.0f, 220.0f, 220.0f};
  float phase_[3] = {0.0f, 0.0f, 0.0f};
  float noisePhase_ = 0.0f;
  float noiseSample_ = -1.0f;

  float env_ = 0.0f;
  float ampSlew_ = 0.0f;
  float velocityGain_ = 0.8f;
  float decayCoef_ = 0.999f;
  float releaseCoef_ = 0.995f;
  bool gate_ = false;
  bool slide_ = false;
  uint8_t retuneCounter_ = 0;

  uint16_t lfsr_ = 0x4000u;
  float dcInput_ = 0.0f;
  float dcOutput_ = 0.0f;

  GrooveboxMode mode_ = GrooveboxMode::Acid;
  float loFiAmount_ = 0.0f;
};
