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
  float transient = fast - slow;
  if (transient < 0.0f) transient = 0.0f;
  if (transient > 1.0f) transient = 1.0f;

  float attackGain = 1.0f + attackAmount_ * transient;
  float sustainGain = 1.0f + sustainAmount_ * (1.0f - transient);

  float transientPart = input * transient;
  float sustainPart = input - transientPart;
  return transientPart * attackGain + sustainPart * sustainGain;
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
