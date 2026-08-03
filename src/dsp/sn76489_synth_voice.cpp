#include "sn76489_synth_voice.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kSnClockHz = 3579545.0f;
constexpr float kDcBlockPole = 0.995f;
}

Sn76489SynthVoice::Sn76489SynthVoice(float sampleRate) {
  static const char* const kStackOptions[] = {"Uni", "Oct", "Fifth", "Chord"};
  static const char* const kNoiseOptions[] = {
      "W-Hi", "W-Mid", "W-Low", "W-T3",
      "P-Hi", "P-Mid", "P-Low", "P-T3"};

  params_[0] = Parameter("Stack", "", kStackOptions, 4, 1);
  params_[1] = Parameter("Tone2", "", 0.0f, 1.0f, 0.65f, 1.0f / 64.0f);
  params_[2] = Parameter("Tone3", "", 0.0f, 1.0f, 0.45f, 1.0f / 64.0f);
  params_[3] = Parameter("Noise", "", 0.0f, 1.0f, 0.0f, 1.0f / 64.0f);
  params_[4] = Parameter("NMode", "", kNoiseOptions, 8, 1);
  params_[5] = Parameter("Decay", "ms", 20.0f, 2000.0f, 260.0f, 10.0f);

  setSampleRate(sampleRate);
  reset();
}

float Sn76489SynthVoice::clamp01(float value) {
  return std::clamp(value, 0.0f, 1.0f);
}

float Sn76489SynthVoice::polyBlep(float phase, float phaseIncrement) {
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

void Sn76489SynthVoice::reset() {
  phase_[0] = phase_[1] = phase_[2] = 0.0f;
  noisePhase_ = 0.0f;
  noiseSample_ = -1.0f;
  env_ = 0.0f;
  ampSlew_ = 0.0f;
  gate_ = false;
  slide_ = false;
  retuneCounter_ = 0;
  lfsr_ = 0x4000u;
  dcInput_ = 0.0f;
  dcOutput_ = 0.0f;
  updateToneFrequencies();
}

void Sn76489SynthVoice::setSampleRate(float sampleRate) {
  if (sampleRate <= 0.0f) sampleRate = 44100.0f;
  sampleRate_ = sampleRate;
  invSampleRate_ = 1.0f / sampleRate_;
  updateEnvelopeCoefficients();
  updateToneFrequencies();
}

void Sn76489SynthVoice::updateEnvelopeCoefficients() {
  const float decaySamples = std::max(1.0f, params_[5].value() * 0.001f * sampleRate_);
  decayCoef_ = std::exp(-1.0f / decaySamples);
  const float releaseSamples = std::max(1.0f, 45.0f * 0.001f * sampleRate_);
  releaseCoef_ = std::exp(-1.0f / releaseSamples);
}

float Sn76489SynthVoice::quantizeToneFrequency(float hz) const {
  if (hz <= 0.0f) return 0.0f;
  float divider = kSnClockHz / (32.0f * hz);
  divider = std::clamp(std::floor(divider + 0.5f), 1.0f, 1023.0f);
  return kSnClockHz / (32.0f * divider);
}

void Sn76489SynthVoice::updateToneFrequencies() {
  float ratios[3] = {1.0f, 1.0f, 1.0f};
  switch (params_[0].optionIndex()) {
    case 1: ratios[1] = 0.5f; ratios[2] = 0.25f; break;
    case 2: ratios[1] = 1.5f; ratios[2] = 0.5f; break;
    case 3: ratios[1] = 1.25f; ratios[2] = 1.5f; break;
    default: ratios[1] = 1.003f; ratios[2] = 0.997f; break;
  }
  for (int i = 0; i < 3; ++i) {
    toneFreq_[i] = quantizeToneFrequency(currentFreqHz_ * ratios[i]);
  }
}

void Sn76489SynthVoice::startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) {
  if (freqHz <= 0.0f) return;
  targetFreqHz_ = freqHz;
  const bool voiceAlreadyActive = gate_ || env_ > 0.0001f;
  slide_ = slideFlag && voiceAlreadyActive;
  if (!slide_) {
    currentFreqHz_ = freqHz;
    env_ = 1.0f;
    ampSlew_ = 0.0f;
    updateToneFrequencies();
  }
  gate_ = true;
  velocityGain_ = std::clamp(static_cast<float>(velocity) / 127.0f, 0.05f, 1.0f);
  if (accent) velocityGain_ = std::min(1.15f, velocityGain_ * 1.15f);
}

void Sn76489SynthVoice::release() {
  gate_ = false;
}

float Sn76489SynthVoice::renderSquare(float phase, float phaseIncrement) const {
  float value = (phase < 0.5f) ? 1.0f : -1.0f;
  value += polyBlep(phase, phaseIncrement);
  float shifted = phase - 0.5f;
  if (shifted < 0.0f) shifted += 1.0f;
  value -= polyBlep(shifted, phaseIncrement);
  return value;
}

bool Sn76489SynthVoice::advancePhase(float& phase, float increment) {
  phase += increment;
  if (phase < 1.0f) return false;
  phase -= std::floor(phase);
  return true;
}

void Sn76489SynthVoice::shiftNoise(bool whiteNoise) {
  const uint16_t feedback = whiteNoise
      ? static_cast<uint16_t>((lfsr_ ^ (lfsr_ >> 3)) & 1u)
      : static_cast<uint16_t>(lfsr_ & 1u);
  lfsr_ = static_cast<uint16_t>((lfsr_ >> 1) | (feedback << 14));
  if (lfsr_ == 0) lfsr_ = 0x4000u;
  noiseSample_ = (lfsr_ & 1u) ? 1.0f : -1.0f;
}

float Sn76489SynthVoice::process() {
  if (!gate_ && env_ <= 0.0001f) return 0.0f;

  if (slide_) {
    currentFreqHz_ += (targetFreqHz_ - currentFreqHz_) * 0.0020f;
    if (++retuneCounter_ >= 16) {
      retuneCounter_ = 0;
      updateToneFrequencies();
    }
  } else {
    currentFreqHz_ = targetFreqHz_;
  }

  env_ *= gate_ ? decayCoef_ : releaseCoef_;
  if (!gate_ && env_ < 0.0001f) {
    env_ = 0.0f;
    return 0.0f;
  }

  float tones[3];
  bool tone3Wrapped = false;
  for (int i = 0; i < 3; ++i) {
    const float increment = std::clamp(toneFreq_[i] * invSampleRate_, 0.0f, 0.49f);
    tones[i] = renderSquare(phase_[i], increment);
    const bool wrapped = advancePhase(phase_[i], increment);
    if (i == 2) tone3Wrapped = wrapped;
  }

  const float level2 = params_[1].value();
  const float level3 = params_[2].value();
  const float toneNorm = 1.0f / std::max(1.0f, 1.0f + level2 + level3);
  float mixed = (tones[0] + tones[1] * level2 + tones[2] * level3) * toneNorm;

  const float noiseMix = params_[3].value();
  if (noiseMix > 0.0001f) {
    const int mode = params_[4].optionIndex();
    const bool whiteNoise = mode < 4;
    const int rate = mode & 3;
    if (rate == 3) {
      if (tone3Wrapped) shiftNoise(whiteNoise);
    } else {
      const float divider = static_cast<float>(512u << rate);
      const float noiseHz = kSnClockHz / divider;
      noisePhase_ += noiseHz * invSampleRate_;
      while (noisePhase_ >= 1.0f) {
        noisePhase_ -= 1.0f;
        shiftNoise(whiteNoise);
      }
    }
    mixed = mixed * (1.0f - 0.45f * noiseMix) + noiseSample_ * (0.45f * noiseMix);
  }

  const float quantizedEnv = std::floor(clamp01(env_) * 15.0f + 0.5f) * (1.0f / 15.0f);
  const float targetAmp = quantizedEnv * velocityGain_ * 0.48f;
  ampSlew_ += (targetAmp - ampSlew_) * std::min(1.0f, 900.0f * invSampleRate_);

  float out = mixed * ampSlew_;
  if (mode_ == GrooveboxMode::Dub) out *= 0.88f;
  else if (mode_ == GrooveboxMode::Electro) out *= 1.06f;

  if (loFiAmount_ > 0.001f) {
    const float levels = 96.0f - loFiAmount_ * 64.0f;
    out = std::floor(out * levels + 0.5f) / levels;
  }

  const float dcBlocked = out - dcInput_ + kDcBlockPole * dcOutput_;
  dcInput_ = out;
  dcOutput_ = dcBlocked;
  return std::clamp(dcBlocked, -1.0f, 1.0f);
}

void Sn76489SynthVoice::setParameterNormalized(uint8_t index, float norm) {
  if (index >= parameterCount()) return;
  params_[index].setNormalized(clamp01(norm));
  if (index == 0) updateToneFrequencies();
  if (index == 5) updateEnvelopeCoefficients();
}

float Sn76489SynthVoice::getParameterNormalized(uint8_t index) const {
  if (index >= parameterCount()) return 0.0f;
  return params_[index].normalized();
}

const Parameter& Sn76489SynthVoice::getParameter(uint8_t index) const {
  if (index >= parameterCount()) return params_[0];
  return params_[index];
}

void Sn76489SynthVoice::setMode(GrooveboxMode mode) {
  mode_ = mode;
}

void Sn76489SynthVoice::setLoFiAmount(float amount) {
  loFiAmount_ = clamp01(amount);
}
