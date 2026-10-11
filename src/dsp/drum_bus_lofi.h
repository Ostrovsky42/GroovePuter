#pragma once

#include <cmath>
#include <cstdint>

// One effect after the drum voices are mixed. This gives every drum engine
// the same LoFi behavior and avoids running a filter once for each drum voice.
class DrumBusLoFi {
public:
  void configure(bool enabled, float amount) {
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    if (enabled_ != enabled || amount_ != amount) {
      holdFramesLeft_ = 0;
      held_ = 0.0f;
    }
    enabled_ = enabled;
    amount_ = amount;
    const int bits = 12 - static_cast<int>(amount * 6.0f);
    levels_ = static_cast<float>(1u << bits);
    holdFrames_ = 1u + static_cast<uint8_t>(amount * 2.5f);
  }

  float process(float input) {
    if (!enabled_ || amount_ <= 0.0f) return input;
    if (holdFramesLeft_ == 0) {
      held_ = std::round(input * levels_) / levels_;
      holdFramesLeft_ = holdFrames_;
    }
    --holdFramesLeft_;
    return input + (held_ - input) * amount_;
  }

private:
  bool enabled_ = false;
  float amount_ = 0.0f;
  float levels_ = 4096.0f;
  float held_ = 0.0f;
  uint8_t holdFrames_ = 1;
  uint8_t holdFramesLeft_ = 0;
};
