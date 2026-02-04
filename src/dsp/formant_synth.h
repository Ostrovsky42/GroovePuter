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

// Vowel phonemes (High Intelligibility)
constexpr Phoneme VOWEL_PHONEMES[] = {
    // Symbol, {{F1, F2, F3}, {A1, A2, A3}, {BW1, BW2, BW3}}, duration, voiced
    {'a', {{730, 1090, 2440}, {1.0f, 0.5f, 0.2f}, {80,  90, 120}}, 120, true},  // "ah"
    {'e', {{530, 1840, 2480}, {1.0f, 0.6f, 0.3f}, {60,  90, 120}}, 100, true},  // "eh"
    {'i', {{350, 2300, 3010}, {1.0f, 0.5f, 0.3f}, {60,  90, 100}}, 90, true},   // "ee"
    {'o', {{570,  840, 2410}, {1.0f, 0.7f, 0.3f}, {70,  80, 100}}, 120, true},  // "oh"
    {'u', {{440, 1020, 2240}, {1.0f, 0.5f, 0.3f}, {70,  80, 100}}, 100, true},  // "oo"
    {'@', {{520, 1550, 2550}, {1.0f, 0.5f, 0.3f}, {60,  90, 120}}, 60, true},   // schwa
    {'A', {{660, 1720, 2410}, {1.0f, 0.6f, 0.2f}, {80,  90, 120}}, 100, true},  // "ae"
    {'O', {{610, 920, 2580}, {1.0f, 0.5f, 0.3f}, {70, 80, 100}}, 110, true},    // "aw"
};

// Consonant phonemes (For Clarity)
constexpr Phoneme CONSONANT_PHONEMES[] = {
    {'s', {{4000, 6000, 8000}, {0.3f, 0.4f, 0.5f}, {200, 300, 400}}, 120, false},
    {'z', {{3500, 5500, 7500}, {0.3f, 0.4f, 0.5f}, {200, 300, 400}}, 100, true},
    {'f', {{1200, 4000, 6000}, {0.2f, 0.3f, 0.2f}, {300, 400, 500}}, 100, false},
    {'v', {{1100, 3800, 5800}, {0.2f, 0.3f, 0.2f}, {300, 400, 500}}, 90, true},
    {'t', {{3000, 5000, 7000}, {0.5f, 0.3f, 0.2f}, {150, 200, 300}}, 40, false},
    {'d', {{2000, 3500, 5000}, {0.6f, 0.4f, 0.2f}, {150, 200, 300}}, 50, true},
    {'k', {{2500, 4000, 6000}, {0.4f, 0.3f, 0.2f}, {200, 250, 350}}, 50, false},
    {'g', {{2400, 3800, 5800}, {0.4f, 0.3f, 0.2f}, {200, 250, 350}}, 60, true},
    {'n', {{250, 1700, 2600}, {0.7f, 0.5f, 0.3f}, {100, 120, 150}}, 80, true},
    {'m', {{250, 900, 2200}, {0.8f, 0.4f, 0.2f}, {100, 100, 150}}, 80, true},
    {'l', {{400, 1200, 2800}, {0.6f, 0.5f, 0.3f}, {80, 100, 120}}, 70, true},
    {'r', {{400, 1200, 1800}, {0.6f, 0.5f, 0.3f}, {80, 100, 120}}, 70, true},
    {'p', {{2000, 4500, 7000}, {0.5f, 0.3f, 0.2f}, {150, 200, 300}}, 30, false},
    {'b', {{1800, 4200, 6800}, {0.5f, 0.3f, 0.2f}, {150, 200, 300}}, 40, true},
    {'w', {{380, 840, 2200}, {0.6f, 0.4f, 0.3f}, {70, 80, 100}}, 60, true},
    {'y', {{350, 2300, 3010}, {0.6f, 0.5f, 0.4f}, {60, 90, 100}}, 50, true},
    {'h', {{500, 1500, 2500}, {0.1f, 0.1f, 0.1f}, {200, 300, 400}}, 60, false},
    {' ', {{500, 1500, 2500}, {0.0f, 0.0f, 0.0f}, {100, 100, 100}}, 80, false},
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
    float getCurrentLevel() const { return currentLevel_.load(); }
    
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
    std::atomic<float> currentLevel_{0.0f};
    
    // Helpers
    Phoneme getPhoneme(char symbol) const;
    void updateFormants();
    float generateExcitation(bool voiced);
    void advanceText();
    float fastRand();
};
