#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>

// Wrapper for SID emulation (ESP32-DASH-SID or similar)
// For now, we provide the interface, and the implementation will link to the library.

class SidSynth {
public:
    SidSynth();
    ~SidSynth();

    void init(float sampleRate);
    void reset();

    // Trigger a note (standard MIDI note)
    void startNote(uint8_t note, uint8_t velocity);
    void stopNote();
    bool isActive() const { return active_; }
    int currentMidiNote() const { return currentMidiNote_; }
    float currentFrequencyHz() const { return freqHz_; }

    // Render mono samples into the buffer
    void process(float* buffer, size_t numSamples);

    // SID Parameters
    void setPulseWidth(uint16_t pw);
    void setFilterCutoff(uint16_t cutoff);
    void setFilterResonance(uint8_t res);
    void setFilterType(uint8_t type); // 0=LP, 1=BP, 2=HP, 3=OFF
    uint16_t getPulseWidth() const { return pulseWidth_; }
    uint16_t getFilterCutoff() const { return filterCutoff_; }
    uint8_t getFilterResonance() const { return filterResonance_; }
    uint8_t getFilterType() const { return filterType_; }

    float getVolume() const { return volume_; }
    void setVolume(float vol) { volume_ = vol; }
    
    float getPeak() { float p = peak_; peak_ = 0; return p; }

private:
    float sampleRate_ = 44100.0f;
    float volume_ = 1.0f;
    bool active_ = false;
    float phase_ = 0.0f;
    float freqHz_ = 440.0f;
    float amp_ = 0.0f;
    int currentMidiNote_ = -1;
    float lpState_ = 0.0f;
    uint16_t pulseWidth_ = 2048;
    uint16_t filterCutoff_ = 4000;
    uint8_t filterResonance_ = 0;
    uint8_t filterType_ = 0;
    float peak_ = 0.0f;
};
