#pragma once

#include "mono_synth_voice.h"

#include <cstdint>

class Opl2SynthVoice : public IMonoSynthVoice {
public:
  explicit Opl2SynthVoice(float sampleRate);
  ~Opl2SynthVoice() override = default;

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
  void release() override;
  float process() override;

  uint8_t parameterCount() const override { return 4; }
  void setParameterNormalized(uint8_t index, float norm) override;
  float getParameterNormalized(uint8_t index) const override;
  const Parameter& getParameter(uint8_t index) const override;

  const char* getEngineName() const override { return "OPL2"; }
  void setMode(GrooveboxMode mode) override;
  void setLoFiAmount(float amount) override;

private:
  float ratioToHz(float baseHz, float ratio) const { return baseHz * ratio; }

  Parameter params_[4];
  float sampleRate_ = 44100.0f;
  float baseFreqHz_ = 220.0f;
  float velocityGain_ = 0.8f;

  float carrierPhase_ = 0.0f;
  float modPhase_ = 0.0f;
  float feedbackSample_ = 0.0f;
  float env_ = 0.0f;
  float envSlew_ = 0.0f;
  bool gate_ = false;

  GrooveboxMode mode_ = GrooveboxMode::Acid;
  float loFiAmount_ = 0.0f;
};
