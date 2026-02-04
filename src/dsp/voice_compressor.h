#pragma once

#include <cmath>

/**
 * VoiceCompressor - Dynamic range compressor for vocal synthesis
 * 
 * Makes voice "punch through" the mix by reducing dynamic range
 * and applying make-up gain.
 * 
 * Features:
 * - Fast attack, slow release
 * - 4:1 compression ratio
 * - Threshold at -10dB
 * - Make-up gain for loudness
 */
class VoiceCompressor {
public:
    VoiceCompressor() {
        // Initialize filter states
        hpfX1_ = 0.0f;
        hpfY1_ = 0.0f;
        lpfY1_ = 0.0f;
        shelfY1_ = 0.0f;
    }
    
    /**
     * Process a single sample
     * 
     * @param input Voice sample (-1.0 to +1.0)
     * @return Compressed sample
     */
    float process(float input) {
        // STAGE 1: High-pass filter @ 150Hz (removes mud/rumble)
        // Butterworth 1-pole HPF: y[n] = a * (y[n-1] + x[n] - x[n-1])
        // Cutoff = 150 Hz @ 44.1kHz → a ≈ 0.9786
        const float a = 0.9786f;
        float hpfOut = a * (hpfY1_ + input - hpfX1_);
        hpfX1_ = input;
        hpfY1_ = hpfOut;
        
        // STAGE 2: Compression
        float absInput = std::abs(hpfOut);
        
        if (absInput > envelope_) {
            envelope_ += (absInput - envelope_) * 0.3f;  // Fast attack
        } else {
            envelope_ += (absInput - envelope_) * 0.05f; // Slow release
        }
        
        float gain = 1.0f;
        if (envelope_ > threshold_) {
            float excess = envelope_ - threshold_;
            float reduction = excess * (1.0f - 1.0f / ratio_);
            gain = threshold_ / (threshold_ + reduction);
        }
        
        float compressed = hpfOut * gain;
        
        // STAGE 3: Make-up gain (+9dB)
        compressed *= makeupGain_;
        
        // STAGE 4: Presence boost (optional, +3dB @ 2kHz shelf)
        if (presenceBoost_ > 0.001f) {
            // Simple shelf: y = x + boost * (x - shelf_state)
            // Approx 2kHz shelf @ 44.1kHz
            float shelfCoef = 0.72f; // ~2kHz
            float boosted = compressed + presenceBoost_ * (compressed - shelfY1_);
            shelfY1_ = shelfY1_ * shelfCoef + compressed * (1.0f - shelfCoef);
            compressed = boosted;
        }
        
        // STAGE 5: Soft clipper (tanh-style)
        // tanh() is expensive, use cubic approximation
        float x = compressed * 0.5f; // Pre-scale for gentler curve
        if (x > 1.0f) x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        
        // Cubic soft clip: y = x - (x^3)/3
        float x3 = x * x * x;
        compressed = (x - x3 * 0.333f) * 2.0f; // Post-scale
        
        // STAGE 6: Low-pass filter (Final smoothing to remove hiss)
        // Simple 1-pole LPF: y[n] = x[n] * alpha + y[n-1] * (1 - alpha)
        // Cutoff ~8kHz @ 44.1kHz -> alpha ≈ 0.6
        const float lpfAlpha = 0.6f;
        float finalSample = compressed * lpfAlpha + lpfY1_ * (1.0f - lpfAlpha);
        lpfY1_ = finalSample;
        
        return finalSample;
    }
    
    /**
     * Set compression threshold (0.0 to 1.0)
     */
    void setThreshold(float thresh) {
        threshold_ = thresh;
    }
    
    /**
     * Set compression ratio (1.0 = no compression, 10.0 = brick wall)
     */
    void setRatio(float r) {
        ratio_ = r;
    }
    
    /**
     * Set make-up gain (1.0 = unity, 2.0 = +6dB)
     */
    void setMakeupGain(float gain) {
        makeupGain_ = gain;
    }
    
    /**
     * Set presence boost (0.0 = off, 0.5 = +3dB @ 2kHz)
     */
    void setPresenceBoost(float boost) {
        presenceBoost_ = boost;
    }
    
    /**
     * Reset envelope state
     */
    void reset() {
        envelope_ = 0.0f;
        hpfX1_ = 0.0f;
        hpfY1_ = 0.0f;
        lpfY1_ = 0.0f;
        shelfY1_ = 0.0f;
    }
    
private:
    // Compression
    float envelope_ = 0.0f;
    float threshold_ = 0.3f;      // -10dB
    float ratio_ = 4.0f;          // 4:1 compression
    float makeupGain_ = 2.8f;     // +9dB boost (was 2.0 = +6dB)
    
    // HPF state (150 Hz)
    float hpfX1_ = 0.0f;
    float hpfY1_ = 0.0f;
    
    // LPF state (Hiss reduction)
    float lpfY1_ = 0.0f;
    
    // Presence shelf state
    float shelfY1_ = 0.0f;
    float presenceBoost_ = 0.5f;  // +3dB @ 2kHz (0.0 = off)
};
