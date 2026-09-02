#pragma once
#ifndef GROOVEPUTER_OUTPUT_OWNED_SYNTH_VOICE_H
#define GROOVEPUTER_OUTPUT_OWNED_SYNTH_VOICE_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "output_ownership.h"
#include "../dsp/swappable_synth_voice.h"

// Pointer-compatible owner used only for the two MiniAcid Synth A/B slots.
// Existing DSP code keeps its current call sites. Ownership is checked only
// when a new local note is started; process()/release() stay free of per-sample
// mode reads and always advance/clean up an already-owned envelope.
class OutputOwnedSynthVoiceSlot {
public:
    explicit OutputOwnedSynthVoiceSlot(GroovePuterOutput::Track track)
        : track_(track) {}

    OutputOwnedSynthVoiceSlot(const OutputOwnedSynthVoiceSlot&) = delete;
    OutputOwnedSynthVoiceSlot& operator=(const OutputOwnedSynthVoiceSlot&) = delete;
    OutputOwnedSynthVoiceSlot(OutputOwnedSynthVoiceSlot&&) = default;
    OutputOwnedSynthVoiceSlot& operator=(OutputOwnedSynthVoiceSlot&&) = default;

    OutputOwnedSynthVoiceSlot& operator=(
        std::unique_ptr<SwappableSynthVoice>&& voice) {
        voice_ = std::move(voice);
        return *this;
    }

    explicit operator bool() const { return static_cast<bool>(voice_); }
    OutputOwnedSynthVoiceSlot* operator->() { return this; }
    const OutputOwnedSynthVoiceSlot* operator->() const { return this; }

    SwappableSynthVoice* get() { return voice_.get(); }
    const SwappableSynthVoice* get() const { return voice_.get(); }

    void setEngineType(SynthEngineType type) {
        if (voice_) voice_->setEngineType(type);
    }
    SynthEngineType engineType() const {
        return voice_ ? voice_->engineType() : SynthEngineType::TB303;
    }
    void setEngineName(const std::string& name) {
        if (voice_) voice_->setEngineName(name);
    }
    IMonoSynthVoice* activeVoice() {
        return voice_ ? voice_->activeVoice() : nullptr;
    }
    const IMonoSynthVoice* activeVoice() const {
        return voice_ ? voice_->activeVoice() : nullptr;
    }
    SynthVoiceState getState() const {
        return voice_ ? voice_->getState() : SynthVoiceState{};
    }
    void setState(const SynthVoiceState& state) {
        if (voice_) voice_->setState(state);
    }
    void reset() {
        if (voice_) voice_->reset();
    }
    void setSampleRate(float sampleRate) {
        if (voice_) voice_->setSampleRate(sampleRate);
    }
    void startNote(float freqHz,
                   bool accent,
                   bool slideFlag,
                   uint8_t velocity = 100) {
        if (!voice_ ||
            !GroovePuterOutput::allowsInternal(
                track_, GroovePuterOutput::SourceClass::Pattern)) {
            return;
        }
        voice_->startNote(freqHz, accent, slideFlag, velocity);
    }
    void release() {
        // Cleanup must never be filtered by the current mode.
        if (voice_) voice_->release();
    }
    float process() {
        // No ownership read in the sample loop. Stage E explicitly releases
        // an active owner when a live mode transition removes the internal side.
        return voice_ ? voice_->process() : 0.0f;
    }
    uint8_t parameterCount() const {
        return voice_ ? voice_->parameterCount() : 0;
    }
    void setParameterNormalized(uint8_t index, float norm) {
        if (voice_) voice_->setParameterNormalized(index, norm);
    }
    float getParameterNormalized(uint8_t index) const {
        return voice_ ? voice_->getParameterNormalized(index) : 0.0f;
    }
    const Parameter& getParameter(uint8_t index) const {
        return voice_->getParameter(index);
    }
    void setMode(GrooveboxMode mode) {
        if (voice_) voice_->setMode(mode);
    }
    void setLoFiAmount(float amount) {
        if (voice_) voice_->setLoFiAmount(amount);
    }
    const char* getEngineName() const {
        return voice_ ? voice_->getEngineName() : "";
    }

    GroovePuterOutput::Track track() const { return track_; }

private:
    GroovePuterOutput::Track track_;
    std::unique_ptr<SwappableSynthVoice> voice_;
};

#endif  // GROOVEPUTER_OUTPUT_OWNED_SYNTH_VOICE_H
