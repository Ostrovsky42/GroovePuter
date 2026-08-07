#pragma once

#include <cstddef>
#include <cstdint>

class SidSynth {
public:
    SidSynth();
    ~SidSynth();

    void init(float sampleRate);
    void reset();

    // Legacy MIDI-note entrypoint kept for source compatibility.
    void startNote(uint8_t note, uint8_t velocity);
    void startNoteFrequency(float frequencyHz,
                            uint8_t velocity,
                            bool accent,
                            bool slideFlag);
    void stopNote();

    bool isActive() const { return active_; }

    void process(float* buffer, size_t numSamples);

    void setPulseWidth(uint16_t pw);
    void setFilterCutoff(uint16_t cutoffHz);
    void setFilterResonance(uint8_t res);
    void setFilterType(uint8_t type); // 0=LP,1=EDGE,2=HP,3=RAW

private:
    void configureTimeConstants_();
    void hardSilence_();

    float sampleRate_{44100.0f};

    bool active_{false};
    bool releasing_{false};
    float phase_{0.0f};
    int currentMidiNote_{-1};

    float freqHz_{440.0f};
    float targetFreqHz_{440.0f};
    float glideStepHz_{0.0f};
    uint32_t glideSamplesRemaining_{0};

    float velocityGain_{0.0f};
    float envelope_{0.0f};
    float attackStep_{1.0f};
    float releaseStep_{1.0f};

    float volume_{1.0f};
    float peak_{0.0f};

    float lpState_{0.0f};
    float dcInputHistory_{0.0f};
    float dcOutputHistory_{0.0f};
    float dcBlockR_{0.995f};

    uint16_t pulseWidth_{2048};     // 0..4095
    uint16_t filterCutoff_{4000};   // Hz
    uint8_t filterResonance_{0};    // damping / character, 0..255
    uint8_t filterType_{0};         // 0 LP default
};
