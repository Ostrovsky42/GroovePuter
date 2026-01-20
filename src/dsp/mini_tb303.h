#pragma once
#ifndef MINIACID_DSP_MINI_TB303_H_
#define MINIACID_DSP_MINI_TB303_H_

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

class TB303Voice {
public:
  explicit TB303Voice(float sampleRate);

  void reset();
  void setSampleRate(float sampleRate);
  void startNote(float freqHz, bool accent, bool slideFlag);
  void release();
  float process();
  const Parameter& parameter(TB303ParamId id) const;
  void setParameter(TB303ParamId id, float value);
  void adjustParameter(TB303ParamId id, int steps);
  float parameterValue(TB303ParamId id) const;
  int oscillatorIndex() const;

private:
  float oscSaw();
  float oscSquare(float saw);
  float oscSuperSaw();
  float oscillatorSample();
  float svfProcess(float input);
  void initParameters();
  void createFilter(int filterTypeIndex);

  static constexpr int kSuperSawOscCount = 6;

  float phase;
  float superPhases[kSuperSawOscCount];
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
  int currentFilterTypeIndex;

  Parameter params[static_cast<int>(TB303ParamId::Count)];
  std::unique_ptr<AudioFilter> filter;
};

#endif  // MINIACID_DSP_MINI_TB303_H_
