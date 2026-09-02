#include "tube_distortion.h"

#include <math.h>

TubeDistortion::TubeDistortion()
  : drive_(kDefaultAudibleDrive),
    mix_(1.0f),
    cachedComp_(1.0f),
    enabled_(false) {
  updateCompensation();
}

void TubeDistortion::updateCompensation() {
  cachedComp_ = 1.0f / (1.0f + 0.06f * drive_);
}

void TubeDistortion::setDrive(float drive) {
  if (drive < kMinimumDrive) drive = kMinimumDrive;
  if (drive > 10.0f) drive = 10.0f;
  drive_ = drive;
  updateCompensation();
}

void TubeDistortion::setMix(float mix) {
  if (mix < 0.0f) mix = 0.0f;
  if (mix > 1.0f) mix = 1.0f;
  mix_ = mix;
}

void TubeDistortion::setEnabled(bool on) {
  if (on && drive_ < kMinimumAudibleDrive) {
    drive_ = kDefaultAudibleDrive;
    updateCompensation();
  }
  enabled_ = on;
}

bool TubeDistortion::isEnabled() const { return enabled_; }

float TubeDistortion::process(float input) {
  if (!enabled_) return input;

  const float driven = input * drive_;
  float shaped = driven / (1.0f + fabsf(driven));
  shaped *= cachedComp_;
  const float out = input * (1.0f - mix_) + shaped * mix_;
  return out / (1.0f + 0.35f * fabsf(out));
}
