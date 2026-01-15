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
// - TONE: Brightness control (Resonant LPF)
// - CRUSH: Bit reduction + downsampling (with anti-aliasing)

class ResonantLPF {
public:
    float process(float input, float cutoff, float resonance) {
        // State variable filter (smooth, musical)
        float f = cutoff * 1.16f;
        float fb = resonance * (1.0f - 0.15f * f * f);
        
        low_ += f * band_;
        float high = input - low_ - fb * band_;
        band_ += f * high;
        
        return low_;
    }
    void reset() { low_ = 0; band_ = 0; }
private:
    float low_ = 0;
    float band_ = 0;
};

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
    float wowDepth_ = 0;          
    float flutterRatio_ = 0;      
    float ageAmount_ = 0;
    float noiseAmount_ = 0;       
    float drive_ = 1.0f;          
    float satMix_ = 0.5f;
    float lpfCutoff_ = 0.9f;       
    float lpfResonance_ = 0.1f;
    uint8_t crushBits_ = 16;      
    uint8_t crushDownsample_ = 1; 
    
    // Calculated warmth cutoff (0..0.5)
    float warmthCutoffNorm_ = 0.5f;

    // Filter states
    ResonantLPF warmthLPF_;
    ResonantLPF toneLPF_;
    ResonantLPF crushLPF_;
    
    // Crush state (sample-and-hold for downsampling)
    uint8_t crushCounter_ = 0;
    float crushHold_ = 0;
    
    // Noise state
    uint32_t noiseState_ = 0x12345678;
    float pinkB0_ = 0, pinkB1_ = 0, pinkB2_ = 0, pinkB3_ = 0, pinkB4_ = 0, pinkB5_ = 0, pinkB6_ = 0;

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

    float generatePinkNoise();
    float readDelayInterpolated(float delaySamples);
};
