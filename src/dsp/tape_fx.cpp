#include "tape_fx.h"
#include <algorithm>
#include <cmath>
#include "audio_wavetables.h"

// Fast conversion factor: 0..1 float phase -> 0..UINT32_MAX fixed phase
static constexpr float kPhaseToUint32 = 4294967296.0f;

TapeFX::TapeFX() {
    // Clear delay buffer
    for (uint32_t i = 0; i < kDelaySize; ++i) {
        buffer_[i] = 0.0f;
    }
    for (uint32_t i = 0; i < kSpaceDelaySize; ++i) {
        spaceBuffer_[i] = 0.0f;
    }

    warmthLPF_.reset();
    toneLPF_.reset();
    crushLPF_.reset();
    
    // Initialize LFO state
    wowSin_ = 0; wowCos_ = 1.0f;
    flutterSin_ = 0; flutterCos_ = 1.0f;
    
    // Initialize current macro to defaults
    currentMacro_ = TapeMacro{};
    paramsDirty_ = true;
    
    // Noise state
    noiseState_ = 0x12345678;
    pinkB0_ = pinkB1_ = pinkB2_ = pinkB3_ = pinkB4_ = pinkB5_ = pinkB6_ = 0;
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
    
    // WOW: 0..0.006 max depth
    wowDepth_ = (m.wow / 100.0f) * 0.006f;
    
    // Wow freq: 0.3 - 1.5 Hz
    float wowHz = 0.3f + (m.wow / 100.0f) * 1.2f;
    float thetaWow = kPi2 * wowHz / static_cast<float>(kSampleRate);
    wowStepSin_ = sinf(thetaWow);
    wowStepCos_ = cosf(thetaWow);
    
    // Flutter: 4.0 - 8.0 Hz (only if wow > 50)
    // Flutter depth is implicitly tied to wowDepth in process(), but we can tune ratio here
    if (m.wow > 50) {
        flutterRatio_ = (m.wow - 50) / 50.0f;
        // Cap flutter ratio contribution
        if(flutterRatio_ > 0.3f) flutterRatio_ = 0.3f; 
        
        float flutterHz = 4.0f + ((m.wow - 50) / 50.0f) * 4.0f;
        float thetaFlutter = kPi2 * flutterHz / static_cast<float>(kSampleRate);
        flutterStepSin_ = sinf(thetaFlutter);
        flutterStepCos_ = cosf(thetaFlutter);
    } else {
        flutterRatio_ = 0;
    }
    
    // AGE: Pink Noise + Warmth
    ageAmount_ = m.age / 100.0f;
    noiseAmount_ = ageAmount_ * 0.0002f; // Max 0.0002
    
    // Warmth LPF: Starts at 8kHz, drops to 2kHz
    float warmthCutoffHz = 8000.0f - (ageAmount_ * 6000.0f);
    warmthCutoffNorm_ = warmthCutoffHz / kSampleRate;
    
    // SAT: Drive 1.0 .. 2.5, Mix 0.3 .. 0.7
    drive_ = 1.0f + (m.sat / 100.0f) * 1.5f;
    satMix_ = 0.3f + (m.sat / 100.0f) * 0.4f;
    
    // TONE: Cutoff 0.3 .. 0.95, Res 0.1 .. 0.3
    lpfCutoff_ = 0.3f + (m.tone / 100.0f) * 0.65f;
    lpfResonance_ = 0.1f + (m.tone / 100.0f) * 0.2f;
    
    // CRUSH: Off by default
    if (m.crush == 0) {
        crushBits_ = 16;
        crushDownsample_ = 1;
    } else {
        switch(m.crush) {
            case 1: crushBits_ = 12; crushDownsample_ = 1; break; 
            case 2: crushBits_ = 10; crushDownsample_ = 2; break; 
            case 3: crushBits_ = 8;  crushDownsample_ = 3; break; 
            default: crushBits_ = 16; crushDownsample_ = 1; break;
        }
    }
    
    paramsDirty_ = false;
}

void TapeFX::applyMinimalParams(uint8_t space, uint8_t movement, uint8_t groove) {
    spaceAmount_ = space * 0.1f; // Boosted from 0.008f for audible delay/reverb
    if (spaceAmount_ > 0.8f) spaceAmount_ = 0.8f; // avoid runaway wet mix

    movementAmount_ = movement * 0.01f; // depth scaling
    if (movementAmount_ > 1.0f) movementAmount_ = 1.0f;

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
    if (!enabled_) return input;

    if (paramsDirty_) updateInternalParams();
    
    if (++lfoCounter_ >= kLFOUpdateRate) {
        lfoCounter_ = 0;
        updateLFO();
    }

    float output = input;

    // 1. WOW/FLUTTER
    if (wowDepth_ > 0) {
        float mod = wowSin_ * wowDepth_;
        if (flutterRatio_ > 0) {
            mod += flutterSin_ * wowDepth_ * 0.3f * flutterRatio_;
        }
        float delaySmp = 100.0f + mod * kSampleRate;
        output = readDelayInterpolated(delaySmp);
    }
    
    buffer_[writePos_] = input;
    writePos_ = (writePos_ + 1) & kDelayMask;

    // 2. WARMTH (Pink Noise + LPF)
    if (ageAmount_ > 0) {
        output += generatePinkNoise() * noiseAmount_;
        output = warmthLPF_.process(output, warmthCutoffNorm_, 0.1f);
    }

    // 3. SATURATION (Soft Mix)
    if (drive_ > 1.0f) {
        float driven = output * drive_;
        float saturated = fastTanh(driven);
        output = output * (1.0f - satMix_) + saturated * satMix_;
    }

    // 4. TONE (Resonant)
    output = toneLPF_.process(output, lpfCutoff_, lpfResonance_);

    // 5. CRUSH (Anti-aliased)
    if (crushBits_ < 16) {
        if (++crushCounter_ >= crushDownsample_) {
            crushCounter_ = 0;
            float filtered = crushLPF_.process(output, 0.3f, 0.1f);
            float levels = static_cast<float>(1 << (crushBits_ - 1));
            crushHold_ = floorf(filtered * levels + 0.5f) / levels;
        }
        output = crushHold_;
    }

    // 6. Minimal Extensions
    if (spaceAmount_ > 0.05f) {
        float dTime = 4000.0f;
        float sRead = (float)spaceWritePos_ - dTime;
        if (sRead < 0) sRead += kSpaceDelaySize;
        float spaceDelayed = spaceBuffer_[(uint32_t)sRead & (kSpaceDelaySize - 1)];
        spaceBuffer_[spaceWritePos_] = output + spaceDelayed * 0.7f;
        spaceWritePos_ = (spaceWritePos_ + 1) & (kSpaceDelaySize - 1);
        output = output * (1.0f - spaceAmount_ * 0.5f) + spaceDelayed * spaceAmount_;
    }

    if (movementAmount_ > 0.01f) {
        movementPhase_ += movementFreq_ / static_cast<float>(kSampleRate);
        if (movementPhase_ >= 1.0f) movementPhase_ -= 1.0f;
        float mod = Wavetable::lookupSine((uint32_t)(movementPhase_ * kPhaseToUint32)) * 0.5f + 0.5f;
        float fc = 0.1f + mod * movementAmount_ * 0.8f;
        movementZ1_ += fc * (output - movementZ1_);
        output = movementZ1_;
    }

    return output;
}

float TapeFX::generatePinkNoise() {
    float white = fastNoise();
    pinkB0_ = 0.99886f * pinkB0_ + white * 0.0555179f;
    pinkB1_ = 0.99332f * pinkB1_ + white * 0.0750759f;
    pinkB2_ = 0.96900f * pinkB2_ + white * 0.1538520f;
    pinkB3_ = 0.86650f * pinkB3_ + white * 0.3104856f;
    pinkB4_ = 0.55000f * pinkB4_ + white * 0.5329522f;
    pinkB5_ = -0.7616f * pinkB5_ - white * 0.0168980f;
    float pink = pinkB0_ + pinkB1_ + pinkB2_ + pinkB3_ + pinkB4_ + pinkB5_ + pinkB6_ + white * 0.5362f;
    pinkB6_ = white * 0.115926f;
    return pink * 0.11f;
}

float TapeFX::readDelayInterpolated(float delaySamples) {
    float readPos = static_cast<float>(writePos_) - delaySamples;
    while (readPos < 0) readPos += static_cast<float>(kDelaySize);
    uint32_t i0 = static_cast<uint32_t>(readPos) & kDelayMask;
    uint32_t i1 = (i0 + 1) & kDelayMask;
    float frac = readPos - floorf(readPos);
    return buffer_[i0] + frac * (buffer_[i1] - buffer_[i0]);
}
