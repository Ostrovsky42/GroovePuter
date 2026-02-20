#pragma once

#include "mono_synth_voice.h"
#include "sid_synth.h"
#include <memory>
#include <vector>

class SidSynthVoice : public IMonoSynthVoice {
public:
    explicit SidSynthVoice(float sampleRate);
    ~SidSynthVoice() override;

    // IMonoSynthVoice implementations
    void reset() override;
    void setSampleRate(float sampleRate) override;
    void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
    void release() override;
    float process() override;
    uint8_t parameterCount() const override;
    void setParameterNormalized(uint8_t index, float norm) override;
    float getParameterNormalized(uint8_t index) const override;
    const char* getEngineName() const override { return "SID"; }
    void setMode(GrooveboxMode mode) override;
    void setLoFiAmount(float amount) override;
    const Parameter& getParameter(uint8_t index) const override;

private:
    std::unique_ptr<SidSynth> sid_;
    float sampleRate_;
    Parameter params_[4];
    std::vector<float> sampleBuffer_; // Buffer for block processing conversion if needed
};
