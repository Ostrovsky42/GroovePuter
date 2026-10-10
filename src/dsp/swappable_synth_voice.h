#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "ay_synth_voice.h"
#include "mini_tb303.h"
#include "mono_synth_voice.h"
#include "sh101_synth_voice.h"
#include "sid_synth_voice.h"
#include "sn76489_synth_voice.h"
#include "wave_morph_synth_voice.h"

enum class SynthEngineType : uint8_t {
    TB303     = 0,
    SID       = 1,
    AY        = 2,
    // Numeric slot 3 is retained only so older persisted scenes remain
    // decodable. Runtime requests for OPL2 are normalized to TB303.
    OPL2      = 3,
    SH101     = 4,
    SN76489   = 5,
    WAVEMORPH = 6,
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
    // The engine the voice is becoming during a switch (the pending one), so
    // the type and the object below always describe the same engine.
    SynthEngineType engineType() const {
        return switching_ && next_ ? pendingType_ : type_;
    }

    void setEngineName(const std::string& name);
    IMonoSynthVoice* activeVoice() {
        return switching_ && next_ ? next_.get() : current_.get();
    }
    const IMonoSynthVoice* activeVoice() const {
        return switching_ && next_ ? next_.get() : current_.get();
    }

    // 0.9.19 S1: the audio thread never replaces or frees an engine. When the
    // crossfade ends it only marks the switch settled and keeps rendering the
    // new engine; the control thread commits it (frees the old engine) under
    // the audio mutation gate. All three are control-thread calls.
    bool switchPending() const { return switching_; }
    bool switchSettled() const {
        return switching_ && fadeDone_.load(std::memory_order_acquire);
    }
    void commitSwitch();

    SynthVoiceState getState() const;
    void setState(const SynthVoiceState& state);

    void reset() override;
    void setSampleRate(float sampleRate) override;
    void startNote(float freqHz,
                   bool accent,
                   bool slideFlag,
                   uint8_t velocity = 100) override;
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
    static std::unique_ptr<IMonoSynthVoice> createVoice(
        SynthEngineType type, float sampleRate);
    static SynthEngineType parseEngineName(const std::string& name);
    static SynthEngineType normalizeEngineType(SynthEngineType type);

    float sampleRate_{44100.0f};
    SynthEngineType type_{SynthEngineType::TB303};
    SynthEngineType pendingType_{SynthEngineType::TB303};
    std::unique_ptr<IMonoSynthVoice> current_{};
    std::unique_ptr<IMonoSynthVoice> next_{};

    bool switching_{false};
    std::atomic<bool> fadeDone_{false};  // set by the audio thread
    uint32_t xfadeTotal_{0};
    uint32_t xfadePos_{0};

    bool noteHeld_{false};
    float lastFreqHz_{0.0f};
    bool lastAccent_{false};
    bool lastSlide_{false};
    uint8_t lastVelocity_{0};

    GrooveboxMode mode_{GrooveboxMode::Acid};
    float loFi_{0.0f};
};
