#include "tube_distortion.h"

#include <math.h>

TubeDistortion::TubeDistortion()
  : drive_(8.0f),
    mix_(1.0f),
    cachedComp_(1.0f / (1.0f + 0.3f * 8.0f)),
    enabled_(false) {}

void TubeDistortion::setDrive(float drive) {
  if (drive < 0.1f)
    drive = 0.1f;
  if (drive > 10.0f)
    drive = 10.0f;
  drive_ = drive;
  cachedComp_ = 1.0f / (1.0f + 0.3f * drive_);
}

void TubeDistortion::setMix(float mix) {
  if (mix < 0.0f)
    mix = 0.0f;
  if (mix > 1.0f)
    mix = 1.0f;
  mix_ = mix;
}

void TubeDistortion::setEnabled(bool on) { enabled_ = on; }

bool TubeDistortion::isEnabled() const { return enabled_; }

float TubeDistortion::process(float input) {
  if (!enabled_) {
    return input;
  }
  float driven = input * drive_;
  float shaped = driven / (1.0f + fabsf(driven));
  shaped *= cachedComp_;
  return input * (1.0f - mix_) + shaped * mix_;
}
