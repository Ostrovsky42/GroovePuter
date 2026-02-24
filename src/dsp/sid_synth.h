#pragma once

#include <cstdint>
#include <cstddef>

class SidSynth {
public:
    SidSynth();
    ~SidSynth();

    void init(float sampleRate);
    void reset();

    void startNote(uint8_t note, uint8_t velocity);
    void stopNote();

    bool isActive() const { return active_; }

    void process(float* buffer, size_t numSamples);

    void setPulseWidth(uint16_t pw);
    void setFilterCutoff(uint16_t cutoffHz);
    void setFilterResonance(uint8_t res);
    void setFilterType(uint8_t type); // 0=LP,1=BP,2=HP,3=OFF

private:
    float sampleRate_{44100.0f};

    bool  active_{false};
    float phase_{0.0f};
    int   currentMidiNote_{-1};

    float freqHz_{440.0f};
    float amp_{0.0f};

    float volume_{1.0f};
    float peak_{0.0f};

    float lpState_{0.0f};

    uint16_t pulseWidth_{2048};     // 0..4095
    uint16_t filterCutoff_{4000};   // Hz
    uint8_t  filterResonance_{0};   // 0..255
    uint8_t  filterType_{0};        // 0 LP default
};
