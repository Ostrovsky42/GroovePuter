#pragma once

#include <memory>
#include <cstdint>
#include <array>
#include <algorithm>
#include <string>

#include "mono_synth_voice.h"
#include "mini_tb303.h"
#include "sid_synth_voice.h"
#include "ay_synth_voice.h"
#include "opl2_synth_voice.h"

enum class SynthEngineType : uint8_t {
    TB303 = 0,
    SID   = 1,
    AY    = 2,
    OPL2  = 3
};

struct SynthVoiceState {
    SynthEngineType engineType{SynthEngineType::TB303};
    std::array<float, 16> params{};
    uint8_t paramCount{0};
};

class SwappableSynthVoice final : public IMonoSynthVoice {
public:
    SwappableSynthVoice(float sampleRate, SynthEngineType initialType);
    ~SwappableSynthVoice() override = default;

    void setEngineType(SynthEngineType type);
    SynthEngineType engineType() const { return type_; }
    
    // Compatibility helpers
    void setEngineName(const std::string& name);
    IMonoSynthVoice* activeVoice() { return current_ ? current_.get() : nullptr; }
    const IMonoSynthVoice* activeVoice() const { return current_ ? current_.get() : nullptr; }

    SynthVoiceState getState() const;
    void setState(const SynthVoiceState& st);

    // IMonoSynthVoice
    void reset() override;
    void setSampleRate(float sampleRate) override;

    void startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity = 100) override;
    void release() override;
    float process() override;

    uint8_t parameterCount() const override;
    void setParameterNormalized(uint8_t index, float norm) override;
    float getParameterNormalized(uint8_t index) const override;
    const Parameter& getParameter(uint8_t index) const override;

    void setMode(GrooveboxMode mode) override;
    void setLoFiAmount(float amount) override;
    const char* getEngineName() const override;

private:
    static std::unique_ptr<IMonoSynthVoice> createVoice(SynthEngineType type, float sampleRate);
    static SynthEngineType parseEngineName(const std::string& name);

    float sampleRate_{44100.0f};

    SynthEngineType type_{SynthEngineType::TB303};
    SynthEngineType pendingType_{SynthEngineType::TB303};

    std::unique_ptr<IMonoSynthVoice> current_{};
    std::unique_ptr<IMonoSynthVoice> next_{};

    // click-free switching
    bool switching_{false};
    uint32_t xfadeTotal_{0};
    uint32_t xfadePos_{0};

    // last note context (to keep sound continuous across swap)
    bool noteHeld_{false};
    float lastFreqHz_{0.0f};
    bool lastAccent_{false};
    bool lastSlide_{false};
    uint8_t lastVelocity_{0};

    // forward mode/lofi to engines
    GrooveboxMode mode_{GrooveboxMode::Acid};
    float loFi_{0.0f};
};
