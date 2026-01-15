#pragma once
#include <cstdint>
#include <cmath>

// Wavetable lookup for fast oscillator generation
// Replaces expensive sinf() calls with O(1) table lookup

static constexpr uint32_t kWavetableSize = 1024;
static constexpr uint32_t kWavetableBits = 10;  // 2^10 = 1024
static constexpr uint32_t kWavetableMask = 0x3FF;

class Wavetable {
public:
  static void init();
  
  // Fast lookup functions using phase in 10.22 fixed-point format
  // Phase range: 0x00000000 to 0xFFFFFFFF maps to 0.0 to 1.0
  static inline float lookupSine(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return sineTable_[index];
  }
  
  static inline float lookupSaw(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return sawTable_[index];
  }
  
  static inline float lookupTriangle(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return triangleTable_[index];
  }
  
  static inline float lookupSquare(uint32_t phase) {
    uint32_t index = (phase >> 22) & kWavetableMask;
    return squareTable_[index];
  }
  
  static bool isInitialized() { return initialized_; }

private:
  static bool initialized_;
  static float sineTable_[kWavetableSize];
  static float sawTable_[kWavetableSize];
  static float triangleTable_[kWavetableSize];
  static float squareTable_[kWavetableSize];
};
