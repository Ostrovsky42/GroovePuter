#include "sid_synth.h"

#include <cmath>
#include <algorithm>

SidSynth::SidSynth() = default;
SidSynth::~SidSynth() = default;

void SidSynth::init(float sampleRate) {
    sampleRate_ = sampleRate;
    reset();
}

void SidSynth::reset() {
    active_ = false;
    phase_ = 0.0f;
    currentMidiNote_ = -1;

    lpState_ = 0.0f;
    peak_ = 0.0f;

    // параметры оставляем как “патч” (не сбрасываем), как обычно в синтах
}

void SidSynth::startNote(uint8_t note, uint8_t velocity) {
    active_ = true;
    currentMidiNote_ = static_cast<int>(note);

    freqHz_ = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
    const float v = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
    amp_ = std::clamp(v, 0.05f, 1.0f);

    // ретриггер фазы — примитивно, но предсказуемо
    phase_ = 0.0f;
}

void SidSynth::stopNote() {
    active_ = false;
    currentMidiNote_ = -1;
}

void SidSynth::process(float* buffer, size_t numSamples) {
    if (!active_ || !buffer || numSamples == 0) return;

    const float nyq = sampleRate_ * 0.5f;
    const float cutoffHz = std::clamp(static_cast<float>(filterCutoff_), 20.0f, nyq * 0.99f);
    const float cutoffNorm = std::clamp(cutoffHz / nyq, 0.001f, 0.99f);
    const float resNorm = std::clamp(static_cast<float>(filterResonance_) / 255.0f, 0.0f, 1.0f);

    // “честный” SID тут не моделируем — это заглушка.
    const float alpha = std::clamp(cutoffNorm * (0.20f + (1.0f - resNorm) * 0.80f), 0.001f, 0.50f);
    const float duty = std::clamp(static_cast<float>(pulseWidth_) / 4095.0f, 0.02f, 0.98f);

    for (size_t i = 0; i < numSamples; ++i) {
        phase_ += freqHz_ / sampleRate_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        const float osc = (phase_ < duty) ? 1.0f : -1.0f;

        lpState_ += alpha * (osc - lpState_);
        const float hp = osc - lpState_;

        float shaped = osc;
        switch (filterType_) {
            case 0: shaped = lpState_; break;                 // LP
            case 1: shaped = (osc + hp) * 0.5f; break;        // BP-ish
            case 2: shaped = hp; break;                       // HP
            case 3: default: shaped = osc; break;             // OFF
        }

        const float out = shaped * amp_ * volume_ * 0.25f;
        peak_ = std::max(peak_, std::fabs(out));
        buffer[i] += out;
    }
}

void SidSynth::setPulseWidth(uint16_t pw) {
    pulseWidth_ = std::clamp<uint16_t>(pw, 64, 4095);
}

void SidSynth::setFilterCutoff(uint16_t cutoffHz) {
    filterCutoff_ = std::clamp<uint16_t>(cutoffHz, 20, 12000);
}

void SidSynth::setFilterResonance(uint8_t res) {
    filterResonance_ = std::min<uint8_t>(res, 255);
}

void SidSynth::setFilterType(uint8_t type) {
    filterType_ = std::min<uint8_t>(type, 3);
}
