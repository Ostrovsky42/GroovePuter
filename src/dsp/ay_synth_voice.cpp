#include "ay_synth_voice.h"

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

AySynthVoice::AySynthVoice(float sampleRate) : sampleRate_(sampleRate) {
  static const char* const kEnvShapes[] = {"Hold", "Decay", "Pluck", "Gate"};

  params_[0] = Parameter("Noise", "", 0.0f, 1.0f, 0.10f, 1.0f / 64.0f);
  params_[1] = Parameter("Decay", "ms", 20.0f, 1500.0f, 220.0f, 10.0f);
  params_[2] = Parameter("Chorus", "", 0.0f, 1.0f, 0.20f, 1.0f / 64.0f);
  params_[3] = Parameter("Env", "", kEnvShapes, 4, 1);

  setSampleRate(sampleRate_);
  reset();
}

void AySynthVoice::reset() {
  phaseA_ = 0.0f;
  phaseB_ = 0.0f;
  phaseC_ = 0.0f;
  noisePhase_ = 0.0f;
  env_ = 0.0f;
  gate_ = false;
  lfsr_ = 0x1FFFFu;
  noiseSample_ = -1.0f;
}

void AySynthVoice::setSampleRate(float sampleRate) {
  if (sampleRate <= 0.0f) sampleRate = 44100.0f;
  sampleRate_ = sampleRate;
}

void AySynthVoice::startNote(float freqHz, bool /*accent*/, bool /*slideFlag*/, uint8_t velocity) {
  if (freqHz <= 0.0f) return;

  // AY/YM tone frequency quantization: f = clock / (16 * period).
  float period = sampleRate_ / (16.0f * freqHz);
  if (period < 1.0f) period = 1.0f;
  const int p = static_cast<int>(period + 0.5f);
  freqHz_ = sampleRate_ / (16.0f * static_cast<float>(p > 0 ? p : 1));

  gate_ = true;
  env_ = 1.0f;
  velocityGain_ = std::clamp(static_cast<float>(velocity) / 127.0f, 0.05f, 1.0f);
}

void AySynthVoice::release() {
  gate_ = false;
}

float AySynthVoice::nextPhase(float phase, float hz) const {
  phase += hz / sampleRate_;
  if (phase >= 1.0f) phase -= 1.0f;
  return phase;
}

float AySynthVoice::genNoise() {
  const float noiseRate = 350.0f + params_[0].value() * 4500.0f;
  noisePhase_ += noiseRate / sampleRate_;
  if (noisePhase_ >= 1.0f) {
    noisePhase_ -= 1.0f;
    // 17-bit LFSR, taps close to AY-style metallic noise feel.
    const uint32_t bit = ((lfsr_ >> 0) ^ (lfsr_ >> 3)) & 1u;
    lfsr_ = (lfsr_ >> 1) | (bit << 16);
    noiseSample_ = (lfsr_ & 1u) ? 1.0f : -1.0f;
  }
  return noiseSample_;
}

float AySynthVoice::process() {
  if (!gate_ && env_ <= 0.0001f) return 0.0f;

  const float chorus = params_[2].value();
  const float detune = chorus * 0.018f;
  const float aHz = freqHz_;
  const float bHz = freqHz_ * (1.0f + detune);
  const float cHz = freqHz_ * (0.5f - detune * 0.25f);

  phaseA_ = nextPhase(phaseA_, aHz);
  phaseB_ = nextPhase(phaseB_, bHz);
  phaseC_ = nextPhase(phaseC_, cHz);

  const float osc = (square(phaseA_) + 0.65f * square(phaseB_) + 0.45f * square(phaseC_)) * (1.0f / 2.1f);
  const float noise = genNoise();

  const float noiseMix = params_[0].value();
  float mixed = osc * (1.0f - noiseMix) + noise * (noiseMix * 0.85f);

  const int envShape = params_[3].optionIndex();
  const float decayMs = params_[1].value();
  float coef = expDecayCoef(sampleRate_, decayMs);

  if (envShape == 2) {
    coef = expDecayCoef(sampleRate_, decayMs * 0.35f);  // Pluck
  } else if (envShape == 3) {
    coef = gate_ ? expDecayCoef(sampleRate_, 4000.0f) : expDecayCoef(sampleRate_, 35.0f); // Gate-like
  }

  if (envShape == 0 && gate_) {
    env_ = 1.0f; // Hold
  } else {
    env_ *= coef;
  }
  if (!gate_ && env_ < 0.0001f) env_ = 0.0f;

  // AY volume is quantized (4-bit style).
  const float v4 = std::floor(env_ * 15.0f + 0.5f) * (1.0f / 15.0f);
  float out = mixed * v4 * velocityGain_ * 0.30f;

  // Optional light extra crunch from global lo-fi amount.
  if (loFiAmount_ > 0.001f) {
    const float levels = 128.0f - loFiAmount_ * 96.0f;
    out = std::floor(out * levels + 0.5f) / levels;
  }

  if (mode_ == GrooveboxMode::Dub) {
    out *= 0.9f;
  } else if (mode_ == GrooveboxMode::Electro) {
    out *= 1.05f;
  }

  return out;
}

void AySynthVoice::setParameterNormalized(uint8_t index, float norm) {
  if (index >= parameterCount()) return;
  params_[index].setNormalized(clamp01(norm));
}

float AySynthVoice::getParameterNormalized(uint8_t index) const {
  if (index >= parameterCount()) return 0.0f;
  return params_[index].normalized();
}

const Parameter& AySynthVoice::getParameter(uint8_t index) const {
  if (index >= parameterCount()) return params_[0];
  return params_[index];
}

void AySynthVoice::setMode(GrooveboxMode mode) {
  mode_ = mode;
}

void AySynthVoice::setLoFiAmount(float amount) {
  loFiAmount_ = clamp01(amount);
}
