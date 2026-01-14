#include "tape_fx.h"
#include <algorithm>

TapeFX::TapeFX() {
  for (uint32_t i = 0; i < kDelaySize; ++i) buffer_[i] = 0.0f;

  float thetaWow = 2.0f * 3.14159265f * 0.8f / (float)kSampleRate;
  wowStepSin_ = sinf(thetaWow);
  wowStepCos_ = cosf(thetaWow);

  float thetaFlutter = 2.0f * 3.14159265f * 12.0f / (float)kSampleRate;
  flutterStepSin_ = sinf(thetaFlutter);
  flutterStepCos_ = cosf(thetaFlutter);
}

void TapeFX::updateLFO() {
  // Wow recursion
  float wS = wowSin_ * wowStepCos_ + wowCos_ * wowStepSin_;
  float wC = wowCos_ * wowStepCos_ - wowSin_ * wowStepSin_;
  wowSin_ = wS;
  wowCos_ = wC;

  // Flutter recursion
  float fS = flutterSin_ * flutterStepCos_ + flutterCos_ * flutterStepSin_;
  float fC = flutterCos_ * flutterStepCos_ - flutterSin_ * flutterStepSin_;
  flutterSin_ = fS;
  flutterCos_ = fC;

  // Normalize every so often to avoid drift (every sample is fine if cheap, but maybe just periodically)
  // For now, simple re-norm
  float wowRescale = 1.0f / sqrtf(wowSin_*wowSin_ + wowCos_*wowCos_);
  wowSin_ *= wowRescale;
  wowCos_ *= wowRescale;
  
  float flutRescale = 1.0f / sqrtf(flutterSin_*flutterSin_ + flutterCos_*flutterCos_);
  flutterSin_ *= flutRescale;
  flutterCos_ *= flutRescale;
}

float TapeFX::process(float input) {
  updateLFO();

  // 1. Calculate modulated delay time
  // Wow depth: ~2-5ms
  // Flutter depth: ~0.5ms
  float wow = wowSin_ * wowAmount_ * 50.0f;
  float flutter = flutterSin_ * flutterAmount_ * 10.0f;
  
  float delaySamples = 100.0f + wow + flutter; // offset + mod
  
  // 2. Read with linear interpolation
  float readPos = (float)writePos_ - delaySamples;
  if (readPos < 0) readPos += (float)kDelaySize;
  
  uint32_t i0 = (uint32_t)readPos & kDelayMask;
  uint32_t i1 = (i0 + 1) & kDelayMask;
  float frac = readPos - (float)i0; // Faster than std::floor for positive readPos
  
  float delayed = buffer_[i0] + frac * (buffer_[i1] - buffer_[i0]);

  // 3. Update buffer
  buffer_[writePos_] = input;
  writePos_ = (writePos_ + 1) & kDelayMask;

  // 4. Saturation (Soft clipping)
  float x = delayed;
  if (saturationAmount_ > 0.01f) {
      float drive = 1.0f + saturationAmount_ * 4.0f;
      x *= drive;
      // Cubic clipper
      if (x > 1.0f) x = 1.0f;
      else if (x < -1.0f) x = -1.0f;
      else x = x - (x * x * x) / 3.0f;
      
      x *= 0.8f; // compensation
  }

  return x;
}
