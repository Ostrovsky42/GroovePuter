#include "sid_synth_voice.h"
#include <cmath>

SidSynthVoice::SidSynthVoice(float sampleRate)
    : sid_(std::make_unique<SidSynth>()), sampleRate_(sampleRate) {
    sampleBuffer_.resize(1, 0.0f);
    
    // Larger steps so encoder/drag on SID page feels responsive.
    params_[0] = Parameter("Cutoff", "Hz", 0.0f, 12000.0f, 4000.0f, 40.0f);
    params_[1] = Parameter("Reso", "", 0.0f, 255.0f, 0.0f, 2.0f);
    params_[2] = Parameter("P-Width", "", 0.0f, 4095.0f, 2048.0f, 16.0f);
    static const char* const kFilterTypes[] = {"LP", "BP", "HP", "OFF"};
    params_[3] = Parameter("F-Mode", "", kFilterTypes, 4, 0);

    setSampleRate(sampleRate);

    // Ensure DSP side gets deterministic defaults before first process().
    setParameterNormalized(0, params_[0].normalized());
    setParameterNormalized(1, params_[1].normalized());
    setParameterNormalized(2, params_[2].normalized());
    setParameterNormalized(3, params_[3].normalized());
}

SidSynthVoice::~SidSynthVoice() {}

void SidSynthVoice::reset() {
    if (sid_) sid_->reset();
}

void SidSynthVoice::setSampleRate(float sampleRate) {
    sampleRate_ = sampleRate;
    if (sid_) sid_->init(sampleRate);
}

void SidSynthVoice::startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) {
    if (!sid_) return;
    if (freqHz <= 0.0f) return;
    
    // Convert frequency back to MIDI note for the SID placeholder
    float midiNoteF = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
    uint8_t note = static_cast<uint8_t>(std::lround(midiNoteF));
    
    sid_->startNote(note, velocity);
}

void SidSynthVoice::release() {
    if (sid_) sid_->stopNote();
}

float SidSynthVoice::process() {
    if (!sid_ || !sid_->isActive()) return 0.0f;
    
    // Process a single sample
    sampleBuffer_[0] = 0.0f;
    sid_->process(sampleBuffer_.data(), 1);
    return sampleBuffer_[0];
}

uint8_t SidSynthVoice::parameterCount() const {
    return 4;
}

void SidSynthVoice::setParameterNormalized(uint8_t index, float norm) {
    if (index >= 4) return;
    params_[index].setNormalized(norm);
    if (!sid_) return;
    
    float val = params_[index].value();
    switch (index) {
        case 0: sid_->setFilterCutoff(static_cast<uint16_t>(val)); break;
        case 1: sid_->setFilterResonance(static_cast<uint8_t>(val)); break;
        case 2: sid_->setPulseWidth(static_cast<uint16_t>(val)); break;
        case 3: sid_->setFilterType(static_cast<uint8_t>(params_[index].optionIndex())); break;
        default: break;
    }
}

float SidSynthVoice::getParameterNormalized(uint8_t index) const {
    if (index >= 4) return 0.0f;
    return params_[index].normalized();
}

const Parameter& SidSynthVoice::getParameter(uint8_t index) const {
    if (index >= 4) return params_[0];
    return params_[index];
}

void SidSynthVoice::setMode(GrooveboxMode mode) {
    // SID doesn't currently care about groovebox mode, but could adjust internal mix
}

void SidSynthVoice::setLoFiAmount(float amount) {
    // SID is already lofi! Could add extra bitcrushing if desired.
}
