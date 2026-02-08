#include "drum_reverb.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
constexpr float kPi = 3.14159265f;
float clampf(float value, float minValue, float maxValue) {
  return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

float clampCutoff(float cutoff, float sampleRate) {
  float maxCutoff = sampleRate * 0.45f;
  if (maxCutoff < 10.0f) maxCutoff = 10.0f;
  return clampf(cutoff, 10.0f, maxCutoff);
}

int16_t floatToInt16(float value) {
  float clamped = clampf(value, -1.0f, 1.0f);
  int scaled = static_cast<int>(std::lrintf(clamped * 32767.0f));
  if (scaled < -32768) scaled = -32768;
  if (scaled > 32767) scaled = 32767;
  return static_cast<int16_t>(scaled);
}

float int16ToFloat(int16_t value) {
  return static_cast<float>(value) / 32768.0f;
}
} // namespace

void DrumReverb::OnePoleLP::setCutoff(float cutoffHz, float sampleRate) {
  float cutoff = clampCutoff(cutoffHz, sampleRate);
  float omega = 2.0f * kPi * cutoff / sampleRate;
  a = 1.0f - std::exp(-omega);
}

void DrumReverb::OnePoleHPF::setCutoff(float cutoffHz, float sampleRate) {
  float cutoff = clampCutoff(cutoffHz, sampleRate);
  float omega = 2.0f * kPi * cutoff / sampleRate;
  a = std::exp(-omega);
}

void DrumReverb::DelayLine::setBuffer(int16_t* data, int size) {
  buffer = data;
  size_ = size > 0 ? size : 0;
  index = 0;
}

void DrumReverb::DelayLine::reset() {
  if (!buffer || size_ <= 0) return;
  std::fill(buffer, buffer + size_, 0);
  index = 0;
}

float DrumReverb::DelayLine::read() const {
  if (!buffer || size_ <= 0) return 0.0f;
  return int16ToFloat(buffer[index]);
}

void DrumReverb::DelayLine::write(float value) {
  if (!buffer || size_ <= 0) return;
  buffer[index] = floatToInt16(value);
  ++index;
  if (index >= size_) index = 0;
}

DrumReverb::DrumReverb() {
  int offset = 0;
  for (int i = 0; i < 4; ++i) {
    combDelay_[i].setBuffer(delayMemory_.data() + offset, kCombDelaySamples[i]);
    offset += kCombDelaySamples[i];
  }
  for (int i = 0; i < 2; ++i) {
    allpass_[i].delay.setBuffer(delayMemory_.data() + offset, kAllpassDelaySamples[i]);
    offset += kAllpassDelaySamples[i];
  }
  predelay_.setBuffer(delayMemory_.data() + offset, kPredelaySamples);
  hasPredelay_ = kPredelaySamples > 0;

  setSampleRate(sampleRate_);
  setMix(mix_);
  setDecay(decay_);
}

void DrumReverb::reset() {
  inputHpf_.reset();
  outputHpf_.reset();
  outputLpf_.reset();
  for (int i = 0; i < 4; ++i) {
    combDamp_[i].reset();
    combDelay_[i].reset();
  }
  for (int i = 0; i < 2; ++i) {
    allpass_[i].reset();
  }
  predelay_.reset();
}

void DrumReverb::setSampleRate(float sr) {
  if (sr <= 0.0f) return;
  sampleRate_ = sr;
  inputHpf_.setCutoff(3000.0f, sampleRate_);
  outputHpf_.setCutoff(2000.0f, sampleRate_);
  outputLpf_.setCutoff(12000.0f, sampleRate_);
  updateDecay();
  updateMix();
}

void DrumReverb::setMix(float mix) {
  mix_ = clampf(mix, 0.0f, 1.0f);
  updateMix();
}

void DrumReverb::setDecay(float decay) {
  decay_ = clampf(decay, 0.0f, 1.0f);
  updateDecay();
}

void DrumReverb::updateMix() {
  float t = mix_ * 0.5f * kPi;
  wet_ = std::sin(t);
  dry_ = std::cos(t);
}

void DrumReverb::updateDecay() {
  float shaped = std::pow(decay_, 1.2f);
  float rt60 = 0.12f + (6.0f - 0.12f) * shaped;
  if (rt60 < 0.02f) rt60 = 0.02f;
  for (int i = 0; i < 4; ++i) {
    float delaySeconds = static_cast<float>(combDelay_[i].size()) / sampleRate_;
    combFeedback_[i] = std::pow(10.0f, -3.0f * delaySeconds / rt60);
  }
  float dampCutoff = 12000.0f + (5500.0f - 12000.0f) * decay_;
  for (int i = 0; i < 4; ++i) {
    combDamp_[i].setCutoff(dampCutoff, sampleRate_);
  }
  allpassK_ = 0.65f + (0.75f - 0.65f) * decay_;
}

float DrumReverb::process(float input) {
  if (wet_ <= 0.0001f) {
    return input;
  }

  float hf = inputHpf_.process(input);
  float revIn = hf;
  if (hasPredelay_) {
    float delayed = predelay_.read();
    predelay_.write(revIn);
    revIn = delayed;
  }

  float combSum = 0.0f;
  for (int i = 0; i < 4; ++i) {
    float tap = combDelay_[i].read();
    float damped = combDamp_[i].process(tap);
    float v = revIn + combFeedback_[i] * damped;
    combDelay_[i].write(v);
    combSum += tap;
  }
  combSum *= 0.25f;

  float y = combSum;
  y = allpass_[0].process(y, allpassK_);
  y = allpass_[1].process(y, allpassK_);

  float wet = outputHpf_.process(y);
  wet = outputLpf_.process(wet);

  // Slightly amplify wet signal to compensate for loss in perceived volume.
  float wet_amplified = wet * 2.0f;

  return dry_ * input + (wet_ * wet_amplified);
}
