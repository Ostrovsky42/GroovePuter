#pragma once

#include <memory>
#include <vector>
#include <cstdint>

#include "mono_synth_voice.h"
#include "sid_synth.h"
#include "mini_dsp_params.h"

class SidSynthVoice final : public IMonoSynthVoice {
public:
    explicit SidSynthVoice(float sampleRate);
    ~SidSynthVoice() override;

    void reset() override;
    void setSampleRate(float sampleRate) override;

    void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) override;
    void release() override;

    float process() override;

    uint8_t parameterCount() const override;
    void setParameterNormalized(uint8_t index, float norm) override;
    float getParameterNormalized(uint8_t index) const override;
    const Parameter& getParameter(uint8_t index) const override;

    void setMode(GrooveboxMode mode) override;
    void setLoFiAmount(float amount) override;

    const char* getEngineName() const override { return "SID"; }

private:
    std::unique_ptr<SidSynth> sid_;
    float sampleRate_{44100.0f};

    Parameter params_[4];
    std::vector<float> sampleBuffer_;
};
