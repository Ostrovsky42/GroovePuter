#pragma once

#include <stdint.h>
#include <cmath>
#include <atomic>

// Maximum samples for phoneme rendering per frame
constexpr int FORMANT_BUFFER_SIZE = 256;

// Maximum custom phrases that can be stored
constexpr int MAX_CUSTOM_PHRASES = 16;
constexpr int MAX_PHRASE_LENGTH = 32;

/**
 * Formant Vocal Synthesizer
 * 
 * Compact formant-based speech synthesis for robotic 80s-style vocals.
 * Inspired by Kraftwerk, Daft Punk, and classic vocoder sounds.
 * 
 * Uses three bandpass filters (F1, F2, F3) to shape an excitation signal
 * (pulse train for voiced sounds, noise for unvoiced).
 * 
 * Memory budget: ~4KB total (very compact!)
 */

struct Formant {
    float freq[3];    // F1, F2, F3 frequencies (Hz)
    float amp[3];     // Amplitude for each formant (0-1)
    float bw[3];      // Bandwidth in Hz
};

struct Phoneme {
    char symbol;      // ASCII symbol for the phoneme
    Formant formant;  // Formant data
    float duration;   // Typical duration in ms
    bool voiced;      // True for vowels/voiced consonants
};

// Vowel phonemes (English approximation)
constexpr Phoneme VOWEL_PHONEMES[] = {
    // Optimized for small speakers (slightly higher formants)
    {'a', {{750, 1150, 2500}, {1.0f, 0.5f, 0.3f}, {80, 90, 120}}, 120, true},   // "ah" 
    {'e', {{550, 1900, 2550}, {1.0f, 0.6f, 0.4f}, {60, 90, 120}}, 100, true},   // "eh" 
    {'i', {{300, 2350, 3100}, {1.0f, 0.7f, 0.5f}, {40, 110, 170}}, 90, true},   // "ee"
    {'o', {{600, 900, 2450}, {1.0f, 0.5f, 0.3f}, {70, 80, 100}}, 120, true},    // "oh"
    {'u', {{450, 1050, 2300}, {1.0f, 0.4f, 0.3f}, {70, 80, 100}}, 100, true},   // "oo"
    {'@', {{520, 1550, 2550}, {1.0f, 0.5f, 0.3f}, {60, 90, 120}}, 60, true},    // schwa
    {'A', {{680, 1750, 2450}, {1.0f, 0.6f, 0.4f}, {80, 90, 120}}, 100, true},   // "ae"
    {'O', {{610, 920, 2580}, {1.0f, 0.5f, 0.3f}, {70, 80, 100}}, 110, true},    // "aw"
};

// Consonant phonemes
constexpr Phoneme CONSONANT_PHONEMES[] = {
    {'s', {{4000, 8000, 10000}, {0.3f, 0.5f, 0.4f}, {500, 1000, 1500}}, 120, false},  // sibilant
    {'z', {{3500, 7500, 9500}, {0.3f, 0.5f, 0.4f}, {400, 900, 1400}}, 100, true},     // voiced sibilant
    {'f', {{1200, 6000, 8000}, {0.2f, 0.4f, 0.3f}, {600, 1200, 1600}}, 100, false},   // fricative
    {'v', {{1100, 5500, 7500}, {0.2f, 0.4f, 0.3f}, {500, 1100, 1500}}, 90, true},     // voiced fricative
    {'t', {{2500, 5000, 7500}, {0.1f, 0.3f, 0.2f}, {800, 1500, 2000}}, 40, false},    // stop
    {'d', {{2300, 4800, 7200}, {0.1f, 0.3f, 0.2f}, {700, 1400, 1900}}, 50, true},     // voiced stop
    {'k', {{2800, 5500, 8000}, {0.1f, 0.3f, 0.2f}, {900, 1600, 2100}}, 50, false},    // velar stop
    {'g', {{2600, 5300, 7800}, {0.1f, 0.3f, 0.2f}, {800, 1500, 2000}}, 60, true},     // voiced velar
    {'n', {{280, 2250, 3000}, {0.8f, 0.4f, 0.3f}, {50, 100, 150}}, 80, true},         // nasal
    {'m', {{280, 1300, 2500}, {0.8f, 0.4f, 0.3f}, {50, 90, 140}}, 80, true},          // bilabial nasal
    {'l', {{360, 1300, 2900}, {0.7f, 0.5f, 0.4f}, {60, 100, 150}}, 70, true},         // lateral
    {'r', {{420, 1300, 1600}, {0.7f, 0.5f, 0.4f}, {70, 100, 140}}, 70, true},         // rhotic
    {'p', {{2000, 4500, 7000}, {0.05f, 0.2f, 0.15f}, {900, 1600, 2200}}, 30, false},  // bilabial stop
    {'b', {{1800, 4200, 6800}, {0.05f, 0.2f, 0.15f}, {800, 1500, 2100}}, 40, true},   // voiced bilabial
    {'w', {{300, 870, 2200}, {0.6f, 0.4f, 0.3f}, {60, 90, 120}}, 60, true},           // glide
    {'y', {{280, 2250, 3000}, {0.6f, 0.5f, 0.4f}, {50, 100, 150}}, 50, true},         // palatal glide
    {'h', {{500, 1500, 2500}, {0.1f, 0.1f, 0.1f}, {200, 300, 400}}, 60, false},       // aspirate
    {' ', {{500, 1500, 2500}, {0.0f, 0.0f, 0.0f}, {100, 100, 100}}, 80, false},       // silence
};

class FormantSynth {
public:
    FormantSynth(float sampleRate = 22050.0f);
    
    // Reset state
    void reset();
    
    // Render a single sample
    float process();
    
    // Render multiple samples into buffer
    void render(float* buffer, size_t numSamples);
    void render(int16_t* buffer, size_t numSamples, float gain = 0.8f);
    
    // Set current phoneme with optional morph time
    void setPhoneme(char symbol, float morphTimeMs = 50.0f);
    
    // Speak a text string (converts to phonemes internally)
    void speak(const char* text);
    
    // Stop speaking immediately
    void stop();
    
    // Control parameters
    void setPitch(float hz);           // Fundamental frequency (60-400 Hz)
    void setSpeed(float multiplier);   // Speech rate (0.5-2.0)
    void setRobotness(float amount);   // 0=natural vibrato, 1=pure monotone
    void setVolume(float vol);         // 0-1
    
    // Getters
    float pitch() const { return pitch_; }
    float speed() const { return speed_; }
    float robotness() const { return robotness_; }
    float volume() const { return volume_; }
    bool isActive() const { return active_; }
    bool isSpeaking() const { return speaking_; }
    
    // Custom phrase management (for scene persistence)
    void setCustomPhrase(int index, const char* phrase);
    const char* getCustomPhrase(int index) const;
    void speakCustomPhrase(int index);
    
private:
    // Biquad bandpass filter for formant
    struct BandpassFilter {
        float x1 = 0, x2 = 0;
        float y1 = 0, y2 = 0;
        float a0 = 0, a1 = 0, a2 = 0;
        float b1 = 0, b2 = 0;
        
        void setParams(float freq, float bandwidth, float gain, float sampleRate);
        float process(float input);
        void reset();
    };
    
    float sampleRate_;
    float pitch_;              // Fundamental frequency (Hz)
    float phase_;              // Oscillator phase (0-1)
    float speed_;              // Speech rate multiplier
    float robotness_;          // Robot effect (0=vibrato, 1=monotone)
    float volume_;             // Output volume
    
    // Current and target phoneme for morphing
    Phoneme currentPhoneme_;
    Phoneme targetPhoneme_;
    float morphProgress_;      // 0..1
    float morphDuration_;      // samples
    float morphSamples_;       // samples remaining
    
    // Formant filters (F1, F2, F3)
    BandpassFilter formants_[3];
    
    // Speech state
    bool active_;
    bool speaking_;
    const char* currentText_;
    int textPosition_;
    float phonemeSamplesRemaining_;
    
    // Vibrato LFO
    float vibratoPhase_;
    
    // Custom phrases storage
    char customPhrases_[MAX_CUSTOM_PHRASES][MAX_PHRASE_LENGTH];
    
    // Random state for noise generation
    uint32_t noiseState_;
    
    // Helpers
    Phoneme getPhoneme(char symbol) const;
    void updateFormants();
    float generateExcitation(bool voiced);
    void advanceText();
    float fastRand();
};
