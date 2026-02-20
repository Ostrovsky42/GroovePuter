#include "opl2_synth_voice.h"

#include <algorithm>
#include <cmath>

namespace {
inline float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

inline float expDecayCoef(float sampleRate, float ms) {
  if (ms < 1.0f) ms = 1.0f;
  const float samples = sampleRate * (ms * 0.001f);
  return std::exp(-1.0f / samples);
}
} // namespace

Opl2SynthVoice::Opl2SynthVoice(float sampleRate) : sampleRate_(sampleRate) {
  params_[0] = Parameter("Ratio", "", 0.25f, 8.0f, 2.0f, 0.05f);
  params_[1] = Parameter("Index", "", 0.0f, 8.0f, 2.4f, 0.06f);
  params_[2] = Parameter("Decay", "ms", 20.0f, 2000.0f, 320.0f, 8.0f);
  params_[3] = Parameter("FB", "", 0.0f, 1.0f, 0.12f, 1.0f / 96.0f);

  setSampleRate(sampleRate_);
  reset();
}

void Opl2SynthVoice::reset() {
  carrierPhase_ = 0.0f;
  modPhase_ = 0.0f;
  feedbackSample_ = 0.0f;
  env_ = 0.0f;
  gate_ = false;
}

void Opl2SynthVoice::setSampleRate(float sampleRate) {
  if (sampleRate <= 0.0f) sampleRate = 44100.0f;
  sampleRate_ = sampleRate;
}

void Opl2SynthVoice::startNote(float freqHz, bool /*accent*/, bool /*slideFlag*/, uint8_t velocity) {
  if (freqHz <= 0.0f) return;
  baseFreqHz_ = freqHz;
  velocityGain_ = std::clamp(static_cast<float>(velocity) / 127.0f, 0.05f, 1.0f);
  env_ = 1.0f;
  gate_ = true;
}

void Opl2SynthVoice::release() {
  gate_ = false;
}

float Opl2SynthVoice::process() {
  if (!gate_ && env_ <= 0.0001f) return 0.0f;

  const float ratio = params_[0].value();
  const float index = params_[1].value();
  const float decayMs = params_[2].value();
  const float feedback = params_[3].value();

  const float modHz = ratioToHz(baseFreqHz_, ratio);
  modPhase_ += modHz / sampleRate_;
  if (modPhase_ >= 1.0f) modPhase_ -= 1.0f;

  // OPL-like 2-op: modulator with feedback feeding carrier phase.
  const float modIn = 2.0f * 3.1415926535f * modPhase_ + feedbackSample_ * feedback * 6.0f;
  const float mod = std::sin(modIn);
  feedbackSample_ = mod;

  carrierPhase_ += baseFreqHz_ / sampleRate_;
  if (carrierPhase_ >= 1.0f) carrierPhase_ -= 1.0f;

  const float carIn = 2.0f * 3.1415926535f * carrierPhase_ + mod * index;
  float out = std::sin(carIn);

  float coef = expDecayCoef(sampleRate_, decayMs);
  if (gate_) {
    // Gentle sustain drift to keep movement instead of hard hold.
    coef = expDecayCoef(sampleRate_, decayMs * 2.5f);
  }
  env_ *= coef;
  if (!gate_ && env_ < 0.0001f) env_ = 0.0f;

  out *= env_ * velocityGain_ * 0.35f;

  if (loFiAmount_ > 0.001f) {
    const float levels = 256.0f - loFiAmount_ * 192.0f;
    out = std::floor(out * levels + 0.5f) / levels;
  }

  if (mode_ == GrooveboxMode::Electro) out *= 1.08f;
  if (mode_ == GrooveboxMode::Dub) out *= 0.9f;

  return out;
}

void Opl2SynthVoice::setParameterNormalized(uint8_t index, float norm) {
  if (index >= parameterCount()) return;
  params_[index].setNormalized(clamp01(norm));
}

float Opl2SynthVoice::getParameterNormalized(uint8_t index) const {
  if (index >= parameterCount()) return 0.0f;
  return params_[index].normalized();
}

const Parameter& Opl2SynthVoice::getParameter(uint8_t index) const {
  if (index >= parameterCount()) return params_[0];
  return params_[index];
}

void Opl2SynthVoice::setMode(GrooveboxMode mode) {
  mode_ = mode;
}

void Opl2SynthVoice::setLoFiAmount(float amount) {
  loFiAmount_ = clamp01(amount);
}
