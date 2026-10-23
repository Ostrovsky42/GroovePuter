#pragma once

#include <array>
#include <cstdint>

#include "mono_synth_voice.h"

class WaveMorphSynthVoice final : public IMonoSynthVoice {
public:
    explicit WaveMorphSynthVoice(float sampleRate);

    void reset() override;
    void setSampleRate(float sampleRate) override;
    void startNote(float freqHz,
                   bool accent,
                   bool slideFlag,
                   uint8_t velocity = 100) override;
    void release() override;
    float process() override;

    uint8_t parameterCount() const override { return 6; }
    void setParameterNormalized(uint8_t index, float norm) override;
    float getParameterNormalized(uint8_t index) const override;
    const Parameter& getParameter(uint8_t index) const override;

    void setMode(GrooveboxMode mode) override { mode_ = mode; }
    void setLoFiAmount(float amount) override;
    const char* getEngineName() const override { return "WAVEMORPH"; }

    static constexpr std::size_t kTableSize = 128;
    static constexpr std::size_t kTableCount = 8;

private:

    static float clamp01(float value);
    static float fastSaturate(float value);
    static float tableSample(std::size_t tableIndex, float phase);

    void updateEnvelopeCoefficients();
    void updateFilterCoefficients();
    void advancePhase(float& phase, float increment);

    std::array<Parameter, 6> params_{};

    float sampleRate_{44100.0f};
    float invSampleRate_{1.0f / 44100.0f};
    float phase_{0.0f};
    float subPhase_{0.0f};
    float currentFreqHz_{110.0f};
    float targetFreqHz_{110.0f};
    float ampEnv_{0.0f};
    float velocityGain_{1.0f};
    float decayCoef_{0.999f};
    float releaseCoef_{0.995f};
    float filterAlpha_{0.2f};
    float filterFeedback_{0.0f};
    float filter1_{0.0f};
    float filter2_{0.0f};
    float dcInput_{0.0f};
    float dcOutput_{0.0f};
    bool gate_{false};
    bool attack_{false};
    bool slide_{false};
    GrooveboxMode mode_{GrooveboxMode::Acid};
    float loFiAmount_{0.0f};
};
