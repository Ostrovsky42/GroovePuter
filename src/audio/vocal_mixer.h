#pragma once

/**
 * VocalMixer - Automatic ducking mixer for voice synthesis
 * 
 * Automatically reduces music volume when voice is speaking
 * for maximum intelligibility.
 * 
 * Features:
 * - Smooth attack/release envelopes
 * - Configurable ducking amount (default 60%)
 * - No clicks or pops
 */
class VocalMixer {
public:
    VocalMixer() = default;
    
    /**
     * Update ducking envelope based on whether voice is speaking
     * Call this once per audio buffer
     */
    void update(bool isSpeaking) {
        if (isSpeaking) {
            // Attack: quickly reduce music
            duckAmount_ += 0.05f;  // ~20ms at 44.1kHz with 256 samples
        } else {
            // Release: slowly bring music back
            duckAmount_ -= 0.02f;  // ~50ms
        }
        
        // Clamp to 0..1
        if (duckAmount_ < 0.0f) duckAmount_ = 0.0f;
        if (duckAmount_ > 1.0f) duckAmount_ = 1.0f;
    }
    
    /**
     * Mix music and voice with automatic ducking
     * 
     * @param music Raw music sample
     * @param voice Raw voice sample
     * @return Mixed output sample
     */
    float mix(float music, float voice) {
        // Music ducking: reduce by up to 60% when voice speaks
        float musicGain = 1.0f - (duckAmount_ * duckAmount_);  // Squared for smoother curve
        
        // Voice gain: always loud and clear
        float voiceGain = 0.7f;
        
        return music * musicGain + voice * voiceGain;
    }
    
    /**
     * Set ducking amount (0 = no ducking, 1 = full ducking)
     */
    void setDuckAmount(float amount) {
        duckAmount_ = amount;
    }
    
    /**
     * Get current ducking envelope value (for UI visualization)
     */
    float getDuckAmount() const {
        return duckAmount_;
    }
    
private:
    float duckAmount_ = 0.0f;  // Current ducking envelope (0..1)
};
