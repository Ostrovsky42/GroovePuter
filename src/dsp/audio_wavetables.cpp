#include "audio_wavetables.h"
#include <cmath>

// Static member initialization
bool Wavetable::initialized_ = false;
float Wavetable::sineTable_[kWavetableSize];
float Wavetable::sawTable_[kWavetableSize];
float Wavetable::triangleTable_[kWavetableSize];
float Wavetable::squareTable_[kWavetableSize];

void Wavetable::init() {
  if (initialized_) return;
  
  constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
  
  // Sine wave (primary for TB303)
  for (uint32_t i = 0; i < kWavetableSize; i++) {
    sineTable_[i] = sinf(kTwoPi * static_cast<float>(i) / static_cast<float>(kWavetableSize));
  }
  
  // Sawtooth wave
  for (uint32_t i = 0; i < kWavetableSize; i++) {
    sawTable_[i] = 2.0f * static_cast<float>(i) / static_cast<float>(kWavetableSize) - 1.0f;
  }
  
  // Triangle wave
  for (uint32_t i = 0; i < kWavetableSize; i++) {
    if (i < kWavetableSize / 2) {
      triangleTable_[i] = 4.0f * static_cast<float>(i) / static_cast<float>(kWavetableSize) - 1.0f;
    } else {
      triangleTable_[i] = 3.0f - 4.0f * static_cast<float>(i) / static_cast<float>(kWavetableSize);
    }
  }
  
  // Square wave (30% duty cycle for acid sound)
  constexpr uint32_t kDutyCycle = static_cast<uint32_t>(kWavetableSize * 0.3f);
  for (uint32_t i = 0; i < kWavetableSize; i++) {
    squareTable_[i] = (i < kDutyCycle) ? 1.0f : -1.0f;
  }
  
  initialized_ = true;
}
