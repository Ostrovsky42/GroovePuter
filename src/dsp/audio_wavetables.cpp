#include "audio_wavetables.h"
#include <cmath>

// Static member initialization
bool Wavetable::initialized_ = false;
float Wavetable::sineTable_[kWavetableSize];

void Wavetable::init() {
  if (initialized_) return;
  
  constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
  
  // Sine wave (primary for TB303)
  for (uint32_t i = 0; i < kWavetableSize; i++) {
    sineTable_[i] = sinf(kTwoPi * static_cast<float>(i) / static_cast<float>(kWavetableSize));
  }
  
  initialized_ = true;
}
