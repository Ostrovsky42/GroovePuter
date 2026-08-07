#include "swappable_synth_voice.h"

#include <cmath>
#include <cctype>

namespace {
float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}
}  // namespace

SynthEngineType SwappableSynthVoice::normalizeEngineType(SynthEngineType type) {
    switch (type) {
        case SynthEngineType::TB303:
        case SynthEngineType::SID:
        case SynthEngineType::AY:
        case SynthEngineType::SH101:
        case SynthEngineType::SN76489:
        case SynthEngineType::WAVEMORPH:
            return type;
        case SynthEngineType::OPL2:
        default:
            return SynthEngineType::TB303;
    }
}

SynthEngineType SwappableSynthVoice::parseEngineName(const std::string& name) {
    std::string upper = name;
    for (char& character : upper) {
        character = static_cast<char>(
            std::toupper(static_cast<unsigned char>(character)));
    }
    if (upper.find("WAVEMORPH") != std::string::npos ||
        upper.find("WAVE MORPH") != std::string::npos) {
        return SynthEngineType::WAVEMORPH;
    }
    if (upper.find("SH101") != std::string::npos ||
        upper.find("SH-101") != std::string::npos ||
        upper.find("MC202") != std::string::npos ||
        upper.find("MC-202") != std::string::npos) {
        return SynthEngineType::SH101;
    }
    if (upper.find("SN76489") != std::string::npos ||
        upper.find("SEGA") != std::string::npos) {
        return SynthEngineType::SN76489;
    }
    if (upper.find("SID") != std::string::npos) return SynthEngineType::SID;
    if (upper.find("OPL2") != std::string::npos ||
        upper.find("FM") != std::string::npos ||
        upper.find("YM3812") != std::string::npos) {
        return SynthEngineType::TB303;
    }
    if (upper.find("AY") != std::string::npos ||
        upper.find("YM2149") != std::string::npos ||
        upper.find("PSG") != std::string::npos) {
        return SynthEngineType::AY;
    }
    return SynthEngineType::TB303;
}

SwappableSynthVoice::SwappableSynthVoice(float sampleRate,
                                         SynthEngineType initialType)
    : sampleRate_(sampleRate),
      type_(normalizeEngineType(initialType)),
      pendingType_(normalizeEngineType(initialType)) {
    if (sampleRate_ <= 0.0f) sampleRate_ = 44100.0f;
    current_ = createVoice(type_, sampleRate_);
    if (current_) {
        current_->setMode(mode_);
        current_->setLoFiAmount(loFi_);
    }
}

std::unique_ptr<IMonoSynthVoice> SwappableSynthVoice::createVoice(
    SynthEngineType type, float sampleRate) {
    switch (normalizeEngineType(type)) {
        case SynthEngineType::SID:
            return std::make_unique<SidSynthVoice>(sampleRate);
        case SynthEngineType::AY:
            return std::make_unique<AySynthVoice>(sampleRate);
        case SynthEngineType::SH101:
            return std::make_unique<Sh101SynthVoice>(sampleRate);
        case SynthEngineType::SN76489:
            return std::make_unique<Sn76489SynthVoice>(sampleRate);
        case SynthEngineType::WAVEMORPH:
            return std::make_unique<WaveMorphSynthVoice>(sampleRate);
        case SynthEngineType::TB303:
        case SynthEngineType::OPL2:
        default:
            return std::make_unique<TB303Voice>(sampleRate);
    }
}

void SwappableSynthVoice::setEngineType(SynthEngineType type) {
    type = normalizeEngineType(type);
    if (type == type_ && !switching_) return;

    pendingType_ = type;
    next_ = createVoice(pendingType_, sampleRate_);
    if (!next_) return;

    next_->setMode(mode_);
    next_->setLoFiAmount(loFi_);
    if (noteHeld_) {
        next_->startNote(lastFreqHz_, lastAccent_, lastSlide_, lastVelocity_);
    }

    constexpr float kCrossfadeMs = 10.0f;
    xfadeTotal_ = static_cast<uint32_t>(std::max(
        16.0f, (sampleRate_ * kCrossfadeMs) / 1000.0f));
    xfadePos_ = 0;
    switching_ = true;
}

void SwappableSynthVoice::setEngineName(const std::string& name) {
    setEngineType(parseEngineName(name));
}

SynthVoiceState SwappableSynthVoice::getState() const {
    SynthVoiceState state;
    const IMonoSynthVoice* voice = switching_ && next_
        ? next_.get() : current_.get();
    state.engineType = switching_ && next_ ? pendingType_ : type_;
    if (!voice) return state;
    const uint8_t count = std::min<uint8_t>(
        voice->parameterCount(), static_cast<uint8_t>(state.params.size()));
    state.paramCount = count;
    for (uint8_t i = 0; i < count; ++i) {
        state.params[i] = voice->getParameterNormalized(i);
    }
    return state;
}

void SwappableSynthVoice::setState(const SynthVoiceState& state) {
    switching_ = false;
    xfadeTotal_ = 0;
    xfadePos_ = 0;

    type_ = normalizeEngineType(state.engineType);
    pendingType_ = type_;
    current_ = createVoice(type_, sampleRate_);
    next_.reset();
    if (!current_) return;

    current_->setMode(mode_);
    current_->setLoFiAmount(loFi_);
    const uint8_t count = std::min<uint8_t>(
        state.paramCount, current_->parameterCount());
    for (uint8_t i = 0; i < count; ++i) {
        current_->setParameterNormalized(i, clamp01(state.params[i]));
    }
}

void SwappableSynthVoice::reset() {
    noteHeld_ = false;
    lastFreqHz_ = 0.0f;
    lastAccent_ = false;
    lastSlide_ = false;
    lastVelocity_ = 0;
    switching_ = false;
    xfadeTotal_ = 0;
    xfadePos_ = 0;
    next_.reset();
    if (current_) current_->reset();
}

void SwappableSynthVoice::setSampleRate(float sampleRate) {
    sampleRate_ = sampleRate;
    if (current_) current_->setSampleRate(sampleRate_);
    if (next_) next_->setSampleRate(sampleRate_);
}

void SwappableSynthVoice::startNote(float freqHz,
                                    bool accent,
                                    bool slideFlag,
                                    uint8_t velocity) {
    noteHeld_ = true;
    lastFreqHz_ = freqHz;
    lastAccent_ = accent;
    lastSlide_ = slideFlag;
    lastVelocity_ = velocity;
    if (current_) current_->startNote(freqHz, accent, slideFlag, velocity);
    if (switching_ && next_) {
        next_->startNote(freqHz, accent, slideFlag, lastVelocity_);
    }
}

void SwappableSynthVoice::release() {
    noteHeld_ = false;
    if (current_) current_->release();
    if (switching_ && next_) next_->release();
}

float SwappableSynthVoice::process() {
    const float current = current_ ? current_->process() : 0.0f;
    if (!switching_ || !next_) return current;

    const float next = next_->process();
    const float t = xfadeTotal_ > 0
        ? static_cast<float>(xfadePos_) / static_cast<float>(xfadeTotal_)
        : 1.0f;
    constexpr float kHalfPi = 1.57079632679f;
    const float mix = clamp01(t);
    const float output = current * std::cos(mix * kHalfPi) +
                         next * std::cos((1.0f - mix) * kHalfPi);

    if (xfadePos_ < xfadeTotal_) ++xfadePos_;
    if (xfadePos_ >= xfadeTotal_) {
        current_ = std::move(next_);
        next_.reset();
        type_ = pendingType_;
        switching_ = false;
        xfadeTotal_ = 0;
        xfadePos_ = 0;
    }
    return output;
}

uint8_t SwappableSynthVoice::parameterCount() const {
    const IMonoSynthVoice* voice = switching_ && next_
        ? next_.get() : current_.get();
    return voice ? voice->parameterCount() : 0;
}

void SwappableSynthVoice::setParameterNormalized(uint8_t index, float norm) {
    IMonoSynthVoice* voice = switching_ && next_ ? next_.get() : current_.get();
    if (voice) voice->setParameterNormalized(index, clamp01(norm));
}

float SwappableSynthVoice::getParameterNormalized(uint8_t index) const {
    const IMonoSynthVoice* voice = switching_ && next_
        ? next_.get() : current_.get();
    return voice ? voice->getParameterNormalized(index) : 0.0f;
}

const Parameter& SwappableSynthVoice::getParameter(uint8_t index) const {
    static Parameter dummy("dummy", "", 0.0f, 1.0f, 0.0f, 1.0f);
    const IMonoSynthVoice* voice = switching_ && next_
        ? next_.get() : current_.get();
    return voice ? voice->getParameter(index) : dummy;
}

void SwappableSynthVoice::setMode(GrooveboxMode mode) {
    mode_ = mode;
    if (current_) current_->setMode(mode_);
    if (next_) next_->setMode(mode_);
}

void SwappableSynthVoice::setLoFiAmount(float amount) {
    loFi_ = amount;
    if (current_) current_->setLoFiAmount(loFi_);
    if (next_) next_->setLoFiAmount(loFi_);
}

const char* SwappableSynthVoice::getEngineName() const {
    const IMonoSynthVoice* voice = switching_ && next_
        ? next_.get() : current_.get();
    return voice ? voice->getEngineName() : "none";
}
