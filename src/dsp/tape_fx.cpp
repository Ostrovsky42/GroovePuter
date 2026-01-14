#include "tape_fx.h"
#include <algorithm>
#include <cmath>

TapeFX::TapeFX() {
    // Clear delay buffer
    for (uint32_t i = 0; i < kDelaySize; ++i) {
        buffer_[i] = 0.0f;
    }
    for (uint32_t i = 0; i < kSpaceDelaySize; ++i) {
        spaceBuffer_[i] = 0.0f;
    }
    
    // Initialize LFO step values for default frequencies
    // Wow: ~0.8 Hz, Flutter: ~12 Hz
    constexpr float kPi2 = 2.0f * 3.14159265f;
    float thetaWow = kPi2 * 0.8f / static_cast<float>(kSampleRate);
    wowStepSin_ = sinf(thetaWow);
    wowStepCos_ = cosf(thetaWow);
    
    float thetaFlutter = kPi2 * 12.0f / static_cast<float>(kSampleRate);
    flutterStepSin_ = sinf(thetaFlutter);
    flutterStepCos_ = cosf(thetaFlutter);
    
    // Initialize current macro to defaults
    currentMacro_ = TapeMacro{};
    paramsDirty_ = true;
}

void TapeFX::applyMacro(const TapeMacro& macro) {
    // Only mark dirty if macro actually changed
    if (std::memcmp(&macro, &currentMacro_, sizeof(TapeMacro)) != 0) {
        currentMacro_ = macro;
        paramsDirty_ = true;
    }
}

void TapeFX::updateInternalParams() {
    const TapeMacro& m = currentMacro_;
    constexpr float kPi2 = 2.0f * 3.14159265f;
    
    // WOW (0-100) → wowDepth 0..0.012, flutterAmount at high values
    wowDepth_ = m.wow * 0.00012f;
    flutterAmount_ = (m.wow > 50) ? (m.wow - 50) * 0.02f : 0.0f;
    
    // Update LFO frequencies based on wow intensity
    // Wow: 0.5-2.5 Hz depending on intensity
    float wowHz = 0.5f + m.wow * 0.02f;
    float thetaWow = kPi2 * wowHz / static_cast<float>(kSampleRate);
    wowStepSin_ = sinf(thetaWow);
    wowStepCos_ = cosf(thetaWow);
    
    // AGE (0-100) → noiseAmount 0..0.08
    noiseAmount_ = m.age * 0.0008f;
    
    // SAT (0-100) → drive 1..8
    drive_ = 1.0f + m.sat * 0.07f;
    
    // TONE (0-100) → LPF coefficient
    // 0 = dark (coeff ~0.15), 100 = bright (coeff ~0.95)
    // One-pole LPF: y[n] = y[n-1] + coeff * (x[n] - y[n-1])
    // Higher coeff = brighter (closer to input)
    lpfCoeff_ = 0.15f + m.tone * 0.008f;
    lpfCoeff_ = std::min(lpfCoeff_, 0.95f);
    
    // CRUSH (0-3) → bits and downsample
    // 0=off (16bit), 1=8bit, 2=6bit, 3=4bit
    static const uint8_t kBitsTable[4] = { 16, 8, 6, 4 };
    static const uint8_t kDownsampleTable[4] = { 1, 2, 4, 6 };
    uint8_t crushIdx = std::min(m.crush, (uint8_t)3);
    crushBits_ = kBitsTable[crushIdx];
    crushDownsample_ = kDownsampleTable[crushIdx];
    
    paramsDirty_ = false;
}

void TapeFX::applyMinimalParams(uint8_t space, uint8_t movement, uint8_t groove) {
    spaceAmount_ = space * 0.008f;
    movementAmount_ = movement * 0.01f; // depth scaling
    movementFreq_ = 0.5f + (movement % 50) * 0.1f;
}

void TapeFX::updateLFO() {
    // Rotation matrix for wow oscillator
    float wS = wowSin_ * wowStepCos_ + wowCos_ * wowStepSin_;
    float wC = wowCos_ * wowStepCos_ - wowSin_ * wowStepSin_;
    wowSin_ = wS;
    wowCos_ = wC;
    
    // Rotation matrix for flutter oscillator
    float fS = flutterSin_ * flutterStepCos_ + flutterCos_ * flutterStepSin_;
    float fC = flutterCos_ * flutterStepCos_ - flutterSin_ * flutterStepSin_;
    flutterSin_ = fS;
    flutterCos_ = fC;
    
    // Periodic normalization to prevent drift (every update is fine since we only 
    // update every 32 samples now)
    float wowRescale = 1.0f / sqrtf(wowSin_ * wowSin_ + wowCos_ * wowCos_ + 1e-10f);
    wowSin_ *= wowRescale;
    wowCos_ *= wowRescale;
    
    float flutRescale = 1.0f / sqrtf(flutterSin_ * flutterSin_ + flutterCos_ * flutterCos_ + 1e-10f);
    flutterSin_ *= flutRescale;
    flutterCos_ *= flutRescale;
}

float TapeFX::process(float input) {
    // Update internal params if macro changed (once per dirty flag)
    if (paramsDirty_) {
        updateInternalParams();
    }
    
    // Update LFO at reduced rate (every 32 samples to save CPU)
    if (++lfoCounter_ >= kLFOUpdateRate) {
        lfoCounter_ = 0;
        updateLFO();
    }

    // 1. Calculate modulated delay time for wow/flutter
    // Wow depth: ~2-5ms at max, Flutter depth: ~0.5ms
    float wowMod = wowSin_ * wowDepth_ * 50.0f;
    float flutterMod = flutterSin_ * flutterAmount_ * 10.0f;
    float delaySamples = 100.0f + wowMod + flutterMod;
    
    // 2. Read from delay line with linear interpolation
    float readPos = static_cast<float>(writePos_) - delaySamples;
    if (readPos < 0) readPos += static_cast<float>(kDelaySize);
    
    uint32_t i0 = static_cast<uint32_t>(readPos) & kDelayMask;
    uint32_t i1 = (i0 + 1) & kDelayMask;
    float frac = readPos - floorf(readPos);
    
    float delayed = buffer_[i0] + frac * (buffer_[i1] - buffer_[i0]);

    // 3. Write input to delay buffer
    buffer_[writePos_] = input;
    writePos_ = (writePos_ + 1) & kDelayMask;

    // 4. Add noise (AGE parameter)
    float x = delayed;
    if (noiseAmount_ > 0.001f) {
        x += fastNoise() * noiseAmount_;
    }

    // 5. Apply LPF (TONE parameter) - one-pole lowpass
    lpfZ1_ += lpfCoeff_ * (x - lpfZ1_);
    x = lpfZ1_;

    // 6. Apply saturation (SAT parameter)
    if (drive_ > 1.01f) {
        x *= drive_;
        x = fastTanh(x);
        x *= 0.8f; // compensation
    }

    // 7. Apply bit crush (CRUSH parameter)
    if (crushBits_ < 16) {
        // Downsample: hold value for N samples
        if (++crushCounter_ >= crushDownsample_) {
            crushCounter_ = 0;
            
            // Bit reduction
            float levels = static_cast<float>(1 << crushBits_);
            crushHold_ = floorf(x * levels * 0.5f + 0.5f) / (levels * 0.5f);
        }
        x = crushHold_;
    }

    // 8. Minimal Techno Extensions
    // Space (Simple Feedback Delay)
    if (spaceAmount_ > 0.05f) {
        float dTime = 4000.0f; // Fixed long delay for space
        float sRead = (float)spaceWritePos_ - dTime;
        if (sRead < 0) sRead += kSpaceDelaySize;
        float spaceDelayed = spaceBuffer_[(uint32_t)sRead & (kSpaceDelaySize - 1)];
        
        spaceBuffer_[spaceWritePos_] = x + spaceDelayed * 0.7f;
        spaceWritePos_ = (spaceWritePos_ + 1) & (kSpaceDelaySize - 1);
        
        x = x * (1.0f - spaceAmount_ * 0.5f) + spaceDelayed * spaceAmount_;
    }

    // Movement (Filter modulation)
    if (movementAmount_ > 0.01f) {
        movementPhase_ += movementFreq_ / static_cast<float>(kSampleRate);
        if (movementPhase_ >= 1.0f) movementPhase_ -= 1.0f;
        
        float mod = sinf(6.2831853f * movementPhase_) * 0.5f + 0.5f;
        float coeff = 0.1f + mod * movementAmount_ * 0.8f;
        movementZ1_ += coeff * (x - movementZ1_);
        x = movementZ1_;
    }

    return x;
}
