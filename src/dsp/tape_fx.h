#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include "../audio/audio_config.h"
#include "../../scenes.h"

// TapeFX provides lo-fi tape characteristics with 5 macro controls:
// - WOW:  Slow pitch modulation (motor drift)
// - AGE:  Noise + high frequency rolloff
// - SAT:  Tape saturation (soft clipping)
// - TONE: Brightness control (LPF)
// - CRUSH: Bit reduction + downsampling

class TapeFX {
public:
    TapeFX();

    // Apply all macros at once (call once per audio buffer, not per sample!)
    // Uses dirty flag to avoid expensive recalculations when unchanged
    void applyMacro(const TapeMacro& macro);

    // Process a single sample (call per sample)
    float process(float input);
    
    // Minimal Techno extensions
    void applyMinimalParams(uint8_t space, uint8_t movement, uint8_t groove);
    
    // Force parameter recalculation on next process()
    void invalidateParams() { paramsDirty_ = true; }

private:
    // Delay line for wow/flutter
    static constexpr uint32_t kDelaySize = 1024;
    static constexpr uint32_t kDelayMask = kDelaySize - 1;
    float buffer_[kDelaySize];
    uint32_t writePos_ = 0;

    // Current macro (for dirty detection)
    TapeMacro currentMacro_;
    bool paramsDirty_ = true;

    // LFO state (rotation matrix for cheap sin/cos)
    float wowSin_ = 0, wowCos_ = 1.0f;
    float wowStepSin_ = 0, wowStepCos_ = 1.0f;
    float flutterSin_ = 0, flutterCos_ = 1.0f;
    float flutterStepSin_ = 0, flutterStepCos_ = 1.0f;
    
    // LFO decimation (update every N samples to save CPU)
    static constexpr uint16_t kLFOUpdateRate = 32;
    uint16_t lfoCounter_ = 0;

    // DSP parameters (derived from macros)
    float wowDepth_ = 0;          // 0..0.012
    float flutterAmount_ = 0;     // 0..1
    float noiseAmount_ = 0;       // 0..0.08
    float drive_ = 1.0f;          // 1..8
    float lpfCoeff_ = 0.9f;       // one-pole LPF coefficient
    uint8_t crushBits_ = 16;      // 16/8/6/4
    uint8_t crushDownsample_ = 1; // 1/2/4/6

    // LPF state (one-pole)
    float lpfZ1_ = 0;
    
    // Crush state (sample-and-hold for downsampling)
    uint8_t crushCounter_ = 0;
    float crushHold_ = 0;
    
    // Noise state (simple LFSR)
    uint32_t noiseState_ = 0x12345678;

    // Minimal extensions
    float spaceAmount_ = 0;
    float movementAmount_ = 0;
    float movementPhase_ = 0;
    float movementFreq_ = 0.5f;
    float movementZ1_ = 0;

    // Simple delay for "Space"
    static constexpr uint32_t kSpaceDelaySize = 4096;
    float spaceBuffer_[kSpaceDelaySize];
    uint32_t spaceWritePos_ = 0;

    // Update LFO oscillators (called every kLFOUpdateRate samples)
    void updateLFO();
    
    // Recalculate DSP params from current macro
    void updateInternalParams();
    
    // Fast approximations - forced inline for audio thread
    __attribute__((always_inline))
    inline float fastTanh(float x) const {
        if (x < -3.0f) return -1.0f;
        if (x > 3.0f) return 1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
    
    __attribute__((always_inline))
    inline float fastNoise() {
        // 32-bit LFSR for cheap noise
        noiseState_ ^= noiseState_ << 13;
        noiseState_ ^= noiseState_ >> 17;
        noiseState_ ^= noiseState_ << 5;
        return (float)(int32_t)noiseState_ * (1.0f / 2147483648.0f);
    }
};
