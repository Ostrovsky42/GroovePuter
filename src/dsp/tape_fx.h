#pragma once
#include <cstdint>
#include <cmath>
#include "../audio/audio_config.h"

// TapeFX provides lo-fi characteristics:
// - Wow: Slow pitch modulation (simulates motor drift)
// - Flutter: Fast pitch modulation (simulates tape crinkle)
// - Saturation: Soft-clipping distortion
class TapeFX {
public:
  TapeFX();

  void setWow(float amount) { wowAmount_ = amount; }           // 0..1
  void setFlutter(float amount) { flutterAmount_ = amount; }   // 0..1
  void setSaturation(float amount) { saturationAmount_ = amount; } // 0..1

  // Process a single sample
  float process(float input);

private:
  static constexpr uint32_t kDelaySize = 1024;
  static constexpr uint32_t kDelayMask = kDelaySize - 1;
  float buffer_[kDelaySize];
  uint32_t writePos_ = 0;

  float wowAmount_ = 0;
  float flutterAmount_ = 0;
  float saturationAmount_ = 0;

  float wowSin_ = 0, wowCos_ = 1.0f;
  float flutterSin_ = 0, flutterCos_ = 1.0f;
  
  // Precomputed rotation steps
  float wowStepSin_ = 0;
  float wowStepCos_ = 1.0f;
  float flutterStepSin_ = 0;
  float flutterStepCos_ = 1.0f;
  
  // Internal LFO update
  void updateLFO();
};
