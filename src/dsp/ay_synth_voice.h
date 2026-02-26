#pragma once

#include "mono_synth_voice.h"

#include <cstdint>

class AySynthVoice : public IMonoSynthVoice {
public:
  explicit AySynthVoice(float sampleRate);
  ~AySynthVoice() override = default;

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
  void release() override;
  float process() override;

  uint8_t parameterCount() const override { return 4; }
  void setParameterNormalized(uint8_t index, float norm) override;
  float getParameterNormalized(uint8_t index) const override;
  const Parameter& getParameter(uint8_t index) const override;

  const char* getEngineName() const override { return "AY"; }
  void setMode(GrooveboxMode mode) override;
  void setLoFiAmount(float amount) override;

private:
  float nextPhase(float phase, float hz) const;
  float square(float phase) const { return (phase < 0.5f) ? 1.0f : -1.0f; }
  float genNoise();

  Parameter params_[4];
  float sampleRate_ = 44100.0f;
  float freqHz_ = 220.0f;

  float phaseA_ = 0.0f;
  float phaseB_ = 0.0f;
  float phaseC_ = 0.0f;
  float noisePhase_ = 0.0f;

  float env_ = 0.0f;
  float velocityGain_ = 0.8f;
  bool gate_ = false;

  uint32_t lfsr_ = 0x1FFFFu;
  float noiseSample_ = -1.0f;
  float ampSlew_ = 0.0f;

  GrooveboxMode mode_ = GrooveboxMode::Acid;
  float loFiAmount_ = 0.0f;
};
