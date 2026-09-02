#pragma once
#ifndef GROOVEPUTER_DSP_MINI_TB303_H
#define GROOVEPUTER_DSP_MINI_TB303_H

#include <stdint.h>
#include <memory>

#include "filter.h"
#include "mini_dsp_params.h"
#include "mono_synth_voice.h"

enum class TB303ParamId : uint8_t {
  Cutoff = 0,
  Resonance,
  EnvAmount,
  EnvDecay,
  Oscillator,
  FilterType,
  MainVolume,
  Count
};

struct TB303Preset {
  float cutoff;
  float resonance;
  float envAmount;
  float decay;
  bool distortion;
  bool delay;
  const char* name;
};

class TB303Voice : public IMonoSynthVoice {
public:
  explicit TB303Voice(float sampleRate);

  const Parameter& parameter(TB303ParamId id) const;
  void setParameter(TB303ParamId id, float value);
  void setParameterNormalized(TB303ParamId id, float norm);

  void reset() override;
  void setSampleRate(float sampleRate) override;
  void startNote(float freqHz, bool accent, bool slideFlag,
                 uint8_t velocity = 100) override;
  void release() override;
  float process() override;
  uint8_t parameterCount() const override;
  void setParameterNormalized(uint8_t index, float norm) override;
  float getParameterNormalized(uint8_t index) const override;
  const Parameter& getParameter(uint8_t index) const override;
  const char* getEngineName() const override { return "TB303"; }
  void setMode(GrooveboxMode mode) override;
  void setLoFiAmount(float amount) override;

  void adjustParameter(TB303ParamId id, int steps);
  float parameterValue(TB303ParamId id) const;
  int oscillatorIndex() const;

  void applyLoFiPreset(int index);
  void setSubOscillator(bool enabled);
  void setNoiseAmount(float amount);

  bool isVoiceActive() const;
  float amplitudeEnvelope() const { return ampEnvelope_; }

private:
  enum class AmpStage : uint8_t { Idle, Attack, Decay, Sustain, Release };

  float oscSaw();
  float oscSquare(float saw);
  float oscPulse();
  float oscSub();
  float oscSuperSaw();
  float oscillatorSample();
  float svfProcess(float input);
  float applyLoFiDegradation(float input);
  float advanceAmplitudeEnvelope();
  void updateAmplitudeCoefficients();
  void initParameters();
  void updateFilterModel();

  static constexpr int kSuperSawOscCount = 6;

  float phase;
  float superPhases[kSuperSawOscCount];

  uint32_t phaseAcc_;
  uint32_t subPhaseAcc_;
  uint32_t superPhasesAcc_[kSuperSawOscCount];

  float freq;
  float targetFreq;
  float slideSpeed;
  float env;
  bool gate;
  bool slide;

  AmpStage ampStage_ = AmpStage::Idle;
  float ampEnvelope_ = 0.0f;
  float ampVelocity_ = 0.0f;
  float ampAttackCoeff_ = 1.0f;
  float ampDecayCoeff_ = 1.0f;
  float ampReleaseCoeff_ = 0.0f;

  float sampleRate;
  float invSampleRate;
  float nyquist;

  Parameter params[static_cast<int>(TB303ParamId::Count)];
  std::unique_ptr<AudioFilter> filter;

  GrooveboxMode mode_ = GrooveboxMode::Acid;
  float loFiAmount_ = 0.0f;
  uint32_t noiseState_ = 12345;
  float driftPhase_ = 0.0f;

  bool subEnabled_ = false;
  float subPhase_ = 0.0f;
  float subMix_ = 0.25f;
  float subLPF_prev_ = 0.0f;
  float noiseAmount_ = 0.0f;

  int lastFilterType_ = -1;
  float postLPF_ = 0.0f;
  struct LowShelfEQ {
    float cutoff = 0.01f;
    float boost = 1.25f;
    float lpf = 0.0f;
    float process(float input) {
      lpf += cutoff * (input - lpf);
      return input + lpf * (boost - 1.0f);
    }
    void reset() { lpf = 0.0f; }
  } bassBoost_;

  float cachedLoFiLevels_ = 1.0f;
  float cachedRecipLoFiLevels_ = 1.0f;
};

#endif  // GROOVEPUTER_DSP_MINI_TB303_H
