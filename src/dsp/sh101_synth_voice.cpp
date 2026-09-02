#include "sh101_synth_voice.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kTwoPi = 6.28318530718f;
constexpr float kDcBlockPole = 0.995f;
}

Sh101SynthVoice::Sh101SynthVoice(float sampleRate) {
  static const char* const kWaveOptions[] = {"Saw", "Pulse", "Mix"};
  params_[0] = Parameter("Wave", "", kWaveOptions, 3, 2);
  params_[1] = Parameter("Sub", "", 0.0f, 1.0f, 0.35f, 1.0f / 64.0f);
  params_[2] = Parameter("Noise", "", 0.0f, 1.0f, 0.0f, 1.0f / 64.0f);
  params_[3] = Parameter("Cutoff", "Hz", 80.0f, 6500.0f, 1800.0f, 20.0f);
  params_[4] = Parameter("Reso", "", 0.0f, 0.92f, 0.30f, 0.01f);
  params_[5] = Parameter("Decay", "ms", 30.0f, 2400.0f, 420.0f, 10.0f);
  setSampleRate(sampleRate);
  reset();
}

float Sh101SynthVoice::clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float Sh101SynthVoice::polyBlep(float phase, float phaseIncrement) {
  if (phaseIncrement <= 0.0f || phaseIncrement >= 1.0f) return 0.0f;
  if (phase < phaseIncrement) {
    phase /= phaseIncrement;
    return phase + phase - phase * phase - 1.0f;
  }
  if (phase > 1.0f - phaseIncrement) {
    phase = (phase - 1.0f) / phaseIncrement;
    return phase * phase + phase + phase + 1.0f;
  }
  return 0.0f;
}

float Sh101SynthVoice::fastSaturate(float value) {
  return value / (1.0f + std::fabs(value));
}

void Sh101SynthVoice::reset() {
  phase_ = 0.0f;
  subPhase_ = 0.0f;
  ampEnv_ = 0.0f;
  filterEnv_ = 0.0f;
  gate_ = false;
  attack_ = false;
  slide_ = false;
  filter1_ = 0.0f;
  filter2_ = 0.0f;
  dcInput_ = 0.0f;
  dcOutput_ = 0.0f;
  noiseState_ = 0x6D2B79F5u;
}

void Sh101SynthVoice::setSampleRate(float sampleRate) {
  if (sampleRate <= 0.0f) sampleRate = 44100.0f;
  sampleRate_ = sampleRate;
  invSampleRate_ = 1.0f / sampleRate_;
  updateEnvelopeCoefficients();
}

void Sh101SynthVoice::updateEnvelopeCoefficients() {
  const float decaySamples = std::max(1.0f, params_[5].value() * 0.001f * sampleRate_);
  decayCoef_ = std::exp(-1.0f / decaySamples);
  const float releaseSamples = std::max(1.0f, 70.0f * 0.001f * sampleRate_);
  releaseCoef_ = std::exp(-1.0f / releaseSamples);
}

void Sh101SynthVoice::startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) {
  if (freqHz <= 0.0f) return;
  targetFreqHz_ = freqHz;
  const bool voiceAlreadyActive = gate_ || ampEnv_ > 0.0001f;
  slide_ = slideFlag && voiceAlreadyActive;
  if (!slide_) {
    currentFreqHz_ = freqHz;
    ampEnv_ = 0.0f;
    filterEnv_ = 0.0f;
    attack_ = true;
  }
  gate_ = true;
  velocityGain_ = std::clamp(static_cast<float>(velocity) / 127.0f, 0.05f, 1.0f);
  if (accent) {
    velocityGain_ = std::min(1.15f, velocityGain_ * 1.15f);
    filterEnv_ = std::max(filterEnv_, 1.15f);
  }
}

void Sh101SynthVoice::release() {
  gate_ = false;
  attack_ = false;
}

void Sh101SynthVoice::advancePhase(float& phase, float increment) {
  phase += increment;
  if (phase >= 1.0f) phase -= std::floor(phase);
}

float Sh101SynthVoice::renderSaw(float phase, float phaseIncrement) const {
  return (phase + phase - 1.0f) - polyBlep(phase, phaseIncrement);
}

float Sh101SynthVoice::renderPulse(float phase, float phaseIncrement, float duty) const {
  float value = (phase < duty) ? 1.0f : -1.0f;
  value += polyBlep(phase, phaseIncrement);
  float shifted = phase - duty;
  if (shifted < 0.0f) shifted += 1.0f;
  value -= polyBlep(shifted, phaseIncrement);
  return value;
}

float Sh101SynthVoice::renderOscillator(float phaseIncrement) {
  const float saw = renderSaw(phase_, phaseIncrement);
  const float pulse = renderPulse(phase_, phaseIncrement, 0.48f);
  switch (params_[0].optionIndex()) {
    case 0: return saw;
    case 1: return pulse;
    default: return saw * 0.58f + pulse * 0.42f;
  }
}

float Sh101SynthVoice::renderSub(float phaseIncrement) {
  const float subIncrement = phaseIncrement * 0.5f;
  return renderPulse(subPhase_, subIncrement, 0.5f);
}

float Sh101SynthVoice::nextNoise() {
  noiseState_ ^= noiseState_ << 13;
  noiseState_ ^= noiseState_ >> 17;
  noiseState_ ^= noiseState_ << 5;
  const int32_t signedValue = static_cast<int32_t>(noiseState_ >> 9) - 0x003FFFFF;
  return static_cast<float>(signedValue) * (1.0f / 4194304.0f);
}

float Sh101SynthVoice::process() {
  if (!gate_ && ampEnv_ <= 0.0001f) return 0.0f;

  if (slide_) {
    currentFreqHz_ += (targetFreqHz_ - currentFreqHz_) * 0.0018f;
  } else {
    currentFreqHz_ = targetFreqHz_;
  }

  if (attack_) {
    const float attackStep = std::min(1.0f, 700.0f * invSampleRate_);
    ampEnv_ += (1.0f - ampEnv_) * attackStep;
    filterEnv_ += (1.0f - filterEnv_) * attackStep;
    if (ampEnv_ >= 0.995f) {
      ampEnv_ = 1.0f;
      attack_ = false;
    }
  } else if (gate_) {
    constexpr float sustain = 0.62f;
    ampEnv_ = sustain + (ampEnv_ - sustain) * decayCoef_;
    filterEnv_ *= decayCoef_;
  } else {
    ampEnv_ *= releaseCoef_;
    filterEnv_ *= releaseCoef_;
  }

  if (!gate_ && ampEnv_ < 0.0001f) {
    ampEnv_ = 0.0f;
    filterEnv_ = 0.0f;
    return 0.0f;
  }

  const float maxFreq = sampleRate_ * 0.45f;
  const float safeFreq = std::clamp(currentFreqHz_, 1.0f, maxFreq);
  const float phaseIncrement = safeFreq * invSampleRate_;

  float source = renderOscillator(phaseIncrement);
  const float subMix = params_[1].value();
  if (subMix > 0.0001f) source = source * (1.0f - 0.45f * subMix) + renderSub(phaseIncrement) * (0.45f * subMix);
  const float noiseMix = params_[2].value();
  if (noiseMix > 0.0001f) source = source * (1.0f - 0.35f * noiseMix) + nextNoise() * (0.35f * noiseMix);

  advancePhase(phase_, phaseIncrement);
  advancePhase(subPhase_, phaseIncrement * 0.5f);

  const float baseCutoff = params_[3].value();
  const float envelopeHeadroom = std::max(0.0f, 6500.0f - baseCutoff);
  float cutoff = baseCutoff + envelopeHeadroom * 0.72f * clamp01(filterEnv_);
  if (mode_ == GrooveboxMode::Dub) cutoff *= 0.80f;
  else if (mode_ == GrooveboxMode::Electro) cutoff *= 1.08f;
  cutoff = std::clamp(cutoff, 60.0f, sampleRate_ * 0.38f);

  float g = kTwoPi * cutoff * invSampleRate_;
  const float alpha = std::clamp(g / (1.0f + g), 0.001f, 0.82f);
  const float feedback = params_[4].value() * 3.35f;
  const float driven = fastSaturate(source - filter2_ * feedback);
  filter1_ += alpha * (driven - filter1_);
  filter2_ += alpha * (filter1_ - filter2_);

  float out = filter2_ * ampEnv_ * velocityGain_ * 1.15f;
  if (loFiAmount_ > 0.001f) {
    const float levels = 128.0f - loFiAmount_ * 96.0f;
    out = std::floor(out * levels + 0.5f) / levels;
  }

  const float dcBlocked = out - dcInput_ + kDcBlockPole * dcOutput_;
  dcInput_ = out;
  dcOutput_ = dcBlocked;
  return std::clamp(dcBlocked, -1.0f, 1.0f);
}

void Sh101SynthVoice::setParameterNormalized(uint8_t index, float norm) {
  if (index >= parameterCount()) return;
  params_[index].setNormalized(clamp01(norm));
  if (index == 5) updateEnvelopeCoefficients();
}

float Sh101SynthVoice::getParameterNormalized(uint8_t index) const {
  if (index >= parameterCount()) return 0.0f;
  return params_[index].normalized();
}

const Parameter& Sh101SynthVoice::getParameter(uint8_t index) const {
  if (index >= parameterCount()) return params_[0];
  return params_[index];
}

void Sh101SynthVoice::setMode(GrooveboxMode mode) {
  mode_ = mode;
}

void Sh101SynthVoice::setLoFiAmount(float amount) {
  loFiAmount_ = clamp01(amount);
}
