#include "formant_synth.h"
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ─────────────────────────────────────────────────────────────
// BandpassFilter Implementation
// ─────────────────────────────────────────────────────────────

void FormantSynth::BandpassFilter::setParams(float freq, float bandwidth, float gain, float sampleRate) {
    // Clamp frequency to valid range
    freq = std::max(20.0f, std::min(freq, sampleRate * 0.45f));
    bandwidth = std::max(10.0f, bandwidth);
    
    float omega = 2.0f * M_PI * freq / sampleRate;
    float sinOmega = sinf(omega);
    float cosOmega = cosf(omega);
    
    // Q from bandwidth
    float Q = freq / bandwidth;
    float alpha = sinOmega / (2.0f * Q);
    
    // Bandpass coefficients (constant 0dB peak gain)
    float norm = 1.0f / (1.0f + alpha);
    
    a0 = alpha * gain * norm;
    a1 = 0.0f;
    a2 = -alpha * gain * norm;
    b1 = -2.0f * cosOmega * norm;
    b2 = (1.0f - alpha) * norm;
}

float FormantSynth::BandpassFilter::process(float input) {
    float output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
    
    // Shift delay line
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    
    // Prevent denormals
    if (fabsf(y1) < 1e-15f) y1 = 0.0f;
    if (fabsf(y2) < 1e-15f) y2 = 0.0f;
    
    return output;
}

void FormantSynth::BandpassFilter::reset() {
    x1 = x2 = y1 = y2 = 0.0f;
}

// ─────────────────────────────────────────────────────────────
// FormantSynth Implementation
// ─────────────────────────────────────────────────────────────

FormantSynth::FormantSynth(float sampleRate)
    : sampleRate_(sampleRate)
    , pitch_(120.0f)
    , phase_(0.0f)
    , speed_(1.0f)
    , robotness_(0.85f)
    , volume_(0.7f)
    , morphProgress_(1.0f)
    , morphDuration_(0.0f)
    , morphSamples_(0.0f)
    , active_(false)
    , speaking_(false)
    , currentText_(nullptr)
    , textPosition_(0)
    , phonemeSamplesRemaining_(0.0f)
    , vibratoPhase_(0.0f)
    , noiseState_(12345)
{
    // Initialize with silence phoneme
    currentPhoneme_ = getPhoneme(' ');
    targetPhoneme_ = currentPhoneme_;
    
    // Clear custom phrases
    for (int i = 0; i < MAX_CUSTOM_PHRASES; i++) {
        customPhrases_[i][0] = '\0';
    }
    
    updateFormants();
}

void FormantSynth::reset() {
    phase_ = 0.0f;
    vibratoPhase_ = 0.0f;
    morphProgress_ = 1.0f;
    active_ = false;
    speaking_ = false;
    currentText_ = nullptr;
    textPosition_ = 0;
    phonemeSamplesRemaining_ = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        formants_[i].reset();
    }
    
    currentPhoneme_ = getPhoneme(' ');
    targetPhoneme_ = currentPhoneme_;
}

float FormantSynth::fastRand() {
    // Fast LCG random for noise generation
    noiseState_ = noiseState_ * 1664525 + 1013904223;
    return (float)(noiseState_ & 0x7FFFFFFF) / (float)0x7FFFFFFF * 2.0f - 1.0f;
}

Phoneme FormantSynth::getPhoneme(char symbol) const {
    // First check vowels
    for (const auto& p : VOWEL_PHONEMES) {
        if (p.symbol == symbol) return p;
    }
    
    // Then consonants
    for (const auto& p : CONSONANT_PHONEMES) {
        if (p.symbol == symbol) return p;
    }
    
    // Handle uppercase vowels
    char lower = (symbol >= 'A' && symbol <= 'Z') ? symbol + 32 : symbol;
    for (const auto& p : VOWEL_PHONEMES) {
        if (p.symbol == lower) return p;
    }
    
    // Default to silence
    return CONSONANT_PHONEMES[17]; // ' ' (silence)
}

void FormantSynth::updateFormants() {
    // Interpolate between current and target phoneme
    float t = morphProgress_;
    
    for (int i = 0; i < 3; i++) {
        float freq = currentPhoneme_.formant.freq[i] + 
                     (targetPhoneme_.formant.freq[i] - currentPhoneme_.formant.freq[i]) * t;
        float amp = currentPhoneme_.formant.amp[i] + 
                    (targetPhoneme_.formant.amp[i] - currentPhoneme_.formant.amp[i]) * t;
        float bw = currentPhoneme_.formant.bw[i] + 
                   (targetPhoneme_.formant.bw[i] - currentPhoneme_.formant.bw[i]) * t;
        
        formants_[i].setParams(freq, bw, amp, sampleRate_);
    }
}

float FormantSynth::generateExcitation(bool voiced) {
    float excitation = 0.0f;
    
    if (voiced) {
        // Vibrato LFO (subtle pitch modulation for natural feel)
        vibratoPhase_ += 5.5f / sampleRate_;
        if (vibratoPhase_ >= 1.0f) vibratoPhase_ -= 1.0f;
        
        float vibrato = sinf(vibratoPhase_ * 2.0f * M_PI);
        float vibratoAmount = (1.0f - robotness_) * 0.02f; // Max 2% pitch deviation
        
        // Pulse train with optional vibrato
        float currentPitch = pitch_ * (1.0f + vibrato * vibratoAmount);
        phase_ += currentPitch / sampleRate_;
        
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
            // Impulse with slight randomization for more natural sound
            excitation = 1.0f - (1.0f - robotness_) * fastRand() * 0.1f;
        } else {
            // Very slight residual noise for voiced sounds
            excitation = fastRand() * 0.02f * (1.0f - robotness_);
        }
    } else {
        // Pure noise for unvoiced consonants
        excitation = fastRand() * 0.5f;
    }
    
    return excitation;
}

float FormantSynth::process() {
    if (!active_) return 0.0f;
    
    // Advance text if speaking
    if (speaking_) {
        advanceText();
    }
    
    // Update morphing
    if (morphProgress_ < 1.0f && morphSamples_ > 0.0f) {
        morphProgress_ += 1.0f / morphSamples_;
        if (morphProgress_ >= 1.0f) {
            morphProgress_ = 1.0f;
            currentPhoneme_ = targetPhoneme_;
        }
        updateFormants();
    }
    
    // Determine if current sound is voiced
    bool voiced = currentPhoneme_.voiced || 
                  (morphProgress_ < 1.0f && targetPhoneme_.voiced);
    
    // Generate excitation signal
    float excitation = generateExcitation(voiced);
    
    // Apply formant filters in parallel
    float output = 0.0f;
    for (int i = 0; i < 3; i++) {
        output += formants_[i].process(excitation);
    }
    
    // Soft saturation for warmer sound
    output = tanhf(output * 1.5f);
    
    return output * volume_;
}

void FormantSynth::render(float* buffer, size_t numSamples) {
    for (size_t i = 0; i < numSamples; i++) {
        buffer[i] = process();
    }
}

void FormantSynth::render(int16_t* buffer, size_t numSamples, float gain) {
    for (size_t i = 0; i < numSamples; i++) {
        float sample = process() * gain;
        // Soft clip
        sample = std::max(-1.0f, std::min(1.0f, sample));
        buffer[i] = static_cast<int16_t>(sample * 32767.0f);
    }
}

void FormantSynth::setPhoneme(char symbol, float morphTimeMs) {
    targetPhoneme_ = getPhoneme(symbol);
    morphProgress_ = 0.0f;
    morphDuration_ = morphTimeMs;
    morphSamples_ = (morphTimeMs / 1000.0f) * sampleRate_;
    active_ = true;
}

void FormantSynth::advanceText() {
    if (!currentText_ || currentText_[textPosition_] == '\0') {
        speaking_ = false;
        active_ = false;
        return;
    }
    
    phonemeSamplesRemaining_ -= 1.0f;
    
    if (phonemeSamplesRemaining_ <= 0.0f) {
        // Move to next character
        textPosition_++;
        
        if (currentText_[textPosition_] == '\0') {
            speaking_ = false;
            // Keep active for a short fade-out
            setPhoneme(' ', 50.0f);
            return;
        }
        
        char next = currentText_[textPosition_];
        Phoneme nextPhoneme = getPhoneme(next);
        
        // Calculate morph time based on speed
        float morphMs = 30.0f / speed_;
        setPhoneme(next, morphMs);
        
        // Set duration for this phoneme
        phonemeSamplesRemaining_ = (nextPhoneme.duration / speed_ / 1000.0f) * sampleRate_;
    }
}

void FormantSynth::speak(const char* text) {
    if (!text || text[0] == '\0') {
        stop();
        return;
    }
    
    currentText_ = text;
    textPosition_ = 0;
    speaking_ = true;
    active_ = true;
    
    // Start with first phoneme
    char first = text[0];
    Phoneme firstPhoneme = getPhoneme(first);
    setPhoneme(first, 20.0f);
    phonemeSamplesRemaining_ = (firstPhoneme.duration / speed_ / 1000.0f) * sampleRate_;
}

void FormantSynth::stop() {
    speaking_ = false;
    currentText_ = nullptr;
    textPosition_ = 0;
    setPhoneme(' ', 30.0f);
}

void FormantSynth::setPitch(float hz) {
    pitch_ = std::max(60.0f, std::min(400.0f, hz));
}

void FormantSynth::setSpeed(float multiplier) {
    speed_ = std::max(0.3f, std::min(3.0f, multiplier));
}

void FormantSynth::setRobotness(float amount) {
    robotness_ = std::max(0.0f, std::min(1.0f, amount));
}

void FormantSynth::setVolume(float vol) {
    volume_ = std::max(0.0f, std::min(1.0f, vol));
}

// ─────────────────────────────────────────────────────────────
// Custom Phrase Management
// ─────────────────────────────────────────────────────────────

void FormantSynth::setCustomPhrase(int index, const char* phrase) {
    if (index < 0 || index >= MAX_CUSTOM_PHRASES) return;
    if (!phrase) {
        customPhrases_[index][0] = '\0';
        return;
    }
    
    size_t len = strlen(phrase);
    if (len >= MAX_PHRASE_LENGTH) len = MAX_PHRASE_LENGTH - 1;
    
    memcpy(customPhrases_[index], phrase, len);
    customPhrases_[index][len] = '\0';
}

const char* FormantSynth::getCustomPhrase(int index) const {
    if (index < 0 || index >= MAX_CUSTOM_PHRASES) return "";
    return customPhrases_[index];
}

void FormantSynth::speakCustomPhrase(int index) {
    if (index < 0 || index >= MAX_CUSTOM_PHRASES) return;
    if (customPhrases_[index][0] == '\0') return;
    
    speak(customPhrases_[index]);
}
