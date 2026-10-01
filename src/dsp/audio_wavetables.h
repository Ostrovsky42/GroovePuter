#pragma once
#include <cstdint>
#include <cmath>

// Wavetable lookup for fast oscillator generation
// Replaces expensive sinf() calls with O(1) table lookup

static constexpr uint32_t kWavetableSize = 1024;
static constexpr uint32_t kWavetableBits = 10;  // 2^10 = 1024
static constexpr uint32_t kWavetableMask = 0x3FF;
static constexpr uint32_t kSquareDutyIndex = static_cast<uint32_t>(kWavetableSize * 0.3f);

class Wavetable {
public:
  static void init();
  
  // Fast lookup functions using phase in 10.22 fixed-point format
  // Phase range: 0x00000000 to 0xFFFFFFFF maps to 0.0 to 1.0
  static inline float lookupSine(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return sineTable_[index];
  }
  
  // Saw and square are closed-form in the table index, so they own no table
  // (8192 B of DRAM). The expressions are the ones the removed tables were
  // filled with, so every output is bit-identical.
  static inline float lookupSaw(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return 2.0f * static_cast<float>(index) / static_cast<float>(kWavetableSize) - 1.0f;
  }

  // 30% duty cycle for acid sound.
  static inline float lookupSquare(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return (index < kSquareDutyIndex) ? 1.0f : -1.0f;
  }
  
  static bool isInitialized() { return initialized_; }

private:
  static bool initialized_;
  static float sineTable_[kWavetableSize];
};
