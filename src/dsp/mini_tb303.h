#pragma once

#include <stdint.h>
#include <memory>

#include "filter.h"
#include "mini_dsp_params.h"

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

class TB303Voice {
public:
  explicit TB303Voice(float sampleRate);

  void reset();
  void setSampleRate(float sampleRate);
  void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100);
  void release();
  float process();
  const Parameter& parameter(TB303ParamId id) const;
  void setParameter(TB303ParamId id, float value);
  void setParameterNormalized(TB303ParamId id, float norm);
  void adjustParameter(TB303ParamId id, int steps);
  float parameterValue(TB303ParamId id) const;
  int oscillatorIndex() const;

  void applyLoFiPreset(int index);
  void setMode(GrooveboxMode mode);
  void setLoFiAmount(float amount); // 0..1 for various degradations
  void setSubOscillator(bool enabled);
  void setNoiseAmount(float amount);

private:
  float oscSaw();
  float oscSquare(float saw);
  float oscPulse();
  float oscSub();
  float oscSuperSaw();
  float oscillatorSample();
  float svfProcess(float input);
  float applyLoFiDegradation(float input);
  void initParameters();
  void updateFilterModel();


  static constexpr int kSuperSawOscCount = 6;

  float phase;
  float superPhases[kSuperSawOscCount];
  
  // Wavetable phase accumulators (10.22 fixed-point)
  uint32_t phaseAcc_;
  uint32_t superPhasesAcc_[kSuperSawOscCount];
  
  float freq;       // current frequency (Hz)
  float targetFreq; // slide target
  float slideSpeed; // how fast we slide toward target
  float env;        // filter envelope value
  bool gate;        // note on/off
  bool slide;       // slide flag for next note
  float amp;        // amplitude

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
  struct LowShelfEQ {
    float cutoff = 0.01f;
    float boost = 1.25f; // ~2dB
    float lpf = 0.0f;
    float process(float input) {
      lpf += cutoff * (input - lpf);
      return input + lpf * (boost - 1.0f);
    }
  } bassBoost_;
};
