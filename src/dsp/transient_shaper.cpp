#include "transient_shaper.h"

#include <cmath>

namespace {
constexpr float kFastAttackMs = 0.3f;
constexpr float kFastReleaseMs = 10.0f;
constexpr float kSlowAttackMs = 35.0f;
constexpr float kSlowReleaseMs = 200.0f;
}

TransientShaper::TransientShaper() {
  updateEnvelopeTimes();
}

void TransientShaper::reset() {
  fastEnv_.reset();
  slowEnv_.reset();
}

void TransientShaper::setSampleRate(float sr) {
  if (sr <= 0.0f) return;
  sampleRate_ = sr;
  fastEnv_.setSampleRate(sr);
  slowEnv_.setSampleRate(sr);
  updateEnvelopeTimes();
}

void TransientShaper::setAttackAmount(float amount) {
  if (amount < -1.0f) amount = -1.0f;
  if (amount > 1.0f) amount = 1.0f;
  attackAmount_ = amount;
}

void TransientShaper::setSustainAmount(float amount) {
  if (amount < -1.0f) amount = -1.0f;
  if (amount > 1.0f) amount = 1.0f;
  sustainAmount_ = amount;
}

float TransientShaper::process(float input) {
  float fast = fastEnv_.process(input);
  float slow = slowEnv_.process(input);
  
  // Transient mask: how much "more" signal is in the fast vs slow follower
  // Boost sensitivity so it responds to real-world drum levels
  float delta = fast - slow;
  if (delta < 0.0f) delta = 0.0f;
  float transientMask = delta * 12.0f; 
  if (transientMask > 1.0f) transientMask = 1.0f;

  // Calculate gains. Use higher multipliers for audible effect.
  // Attack: range from 0.1x to 5.0x
  float attackLeverage = (attackAmount_ >= 0.0f) ? (attackAmount_ * 4.0f) : (attackAmount_ * 0.9f);
  // Sustain: range from 0.1x to 3.0x
  float sustainLeverage = (sustainAmount_ >= 0.0f) ? (sustainAmount_ * 2.0f) : (sustainAmount_ * 0.9f);

  float totalGain = 1.0f;
  totalGain += attackLeverage * transientMask;
  totalGain += sustainLeverage * (1.0f - transientMask);

  return input * totalGain;
}

void TransientShaper::EnvelopeFollower::reset() {
  env = 0.0f;
}

void TransientShaper::EnvelopeFollower::setSampleRate(float sr) {
  if (sr <= 0.0f) return;
  sampleRate = sr;
}

void TransientShaper::EnvelopeFollower::setTimes(float attackMs, float releaseMs) {
  float attackTime = attackMs * 0.001f;
  float releaseTime = releaseMs * 0.001f;
  if (attackTime <= 0.0f) attackTime = 0.0001f;
  if (releaseTime <= 0.0f) releaseTime = 0.0001f;
  attackCoeff = std::exp(-1.0f / (attackTime * sampleRate));
  releaseCoeff = std::exp(-1.0f / (releaseTime * sampleRate));
}

float TransientShaper::EnvelopeFollower::process(float input) {
  float x = std::fabs(input);
  if (x > env) {
    env = attackCoeff * (env - x) + x;
  } else {
    env = releaseCoeff * (env - x) + x;
  }
  return env;
}

void TransientShaper::updateEnvelopeTimes() {
  fastEnv_.setTimes(kFastAttackMs, kFastReleaseMs);
  slowEnv_.setTimes(kSlowAttackMs, kSlowReleaseMs);
}
