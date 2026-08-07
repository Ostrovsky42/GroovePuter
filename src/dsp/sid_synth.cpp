#include "sid_synth.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kAttackSeconds = 0.0015f;
constexpr float kReleaseSeconds = 0.070f;
constexpr float kGlideSeconds = 0.045f;
constexpr float kAccentGain = 1.15f;
constexpr float kDcBlockHz = 12.0f;
constexpr float kTwoPi = 6.28318530717958647692f;
}

SidSynth::SidSynth() = default;
SidSynth::~SidSynth() = default;

void SidSynth::init(float sampleRate) {
    sampleRate_ = sampleRate > 1000.0f ? sampleRate : 44100.0f;
    configureTimeConstants_();
    reset();
}

void SidSynth::configureTimeConstants_() {
    const float attackSamples = std::max(1.0f, sampleRate_ * kAttackSeconds);
    const float releaseSamples = std::max(1.0f, sampleRate_ * kReleaseSeconds);
    attackStep_ = 1.0f / attackSamples;
    releaseStep_ = 1.0f / releaseSamples;
    dcBlockR_ = std::exp(-kTwoPi * kDcBlockHz / sampleRate_);
}

void SidSynth::hardSilence_() {
    active_ = false;
    releasing_ = false;
    currentMidiNote_ = -1;
    velocityGain_ = 0.0f;
    envelope_ = 0.0f;
    glideSamplesRemaining_ = 0;
    glideStepHz_ = 0.0f;
}

void SidSynth::reset() {
    hardSilence_();
    phase_ = 0.0f;
    freqHz_ = 440.0f;
    targetFreqHz_ = 440.0f;
    lpState_ = 0.0f;
    dcInputHistory_ = 0.0f;
    dcOutputHistory_ = 0.0f;
    peak_ = 0.0f;
}

void SidSynth::startNote(uint8_t note, uint8_t velocity) {
    const float frequency =
        440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
    startNoteFrequency(frequency, velocity, false, false);
    currentMidiNote_ = static_cast<int>(note);
}

void SidSynth::startNoteFrequency(float frequencyHz,
                                  uint8_t velocity,
                                  bool accent,
                                  bool slideFlag) {
    if (!(frequencyHz > 0.0f) || !std::isfinite(frequencyHz)) return;

    if (velocity == 0) {
        hardSilence_();
        return;
    }

    const bool legatoSlide = slideFlag && active_ && !releasing_ && envelope_ > 0.0f;
    targetFreqHz_ = frequencyHz;
    const float baseGain = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
    velocityGain_ = std::min(1.0f, baseGain * (accent ? kAccentGain : 1.0f));

    if (legatoSlide) {
        const uint32_t glideSamples = static_cast<uint32_t>(
            std::max(1.0f, std::round(sampleRate_ * kGlideSeconds)));
        glideSamplesRemaining_ = glideSamples;
        glideStepHz_ = (targetFreqHz_ - freqHz_) /
                       static_cast<float>(glideSamples);
        return;
    }

    active_ = true;
    releasing_ = false;
    freqHz_ = targetFreqHz_;
    glideSamplesRemaining_ = 0;
    glideStepHz_ = 0.0f;
    phase_ = 0.0f;
    envelope_ = 0.0f;
}

void SidSynth::stopNote() {
    if (!active_) return;
    releasing_ = true;
    currentMidiNote_ = -1;
}

void SidSynth::process(float* buffer, size_t numSamples) {
    if (!active_ || !buffer || numSamples == 0) return;

    const float nyq = sampleRate_ * 0.5f;
    const float cutoffHz = std::clamp(
        static_cast<float>(filterCutoff_), 20.0f, nyq * 0.99f);
    const float cutoffNorm = std::clamp(cutoffHz / nyq, 0.001f, 0.99f);
    const float dampNorm = std::clamp(
        static_cast<float>(filterResonance_) / 255.0f, 0.0f, 1.0f);
    const float alpha = std::clamp(
        cutoffNorm * (0.20f + (1.0f - dampNorm) * 0.80f),
        0.001f,
        0.50f);
    const float duty = std::clamp(
        static_cast<float>(pulseWidth_) / 4095.0f, 0.02f, 0.98f);

    for (size_t i = 0; i < numSamples; ++i) {
        if (glideSamplesRemaining_ > 0) {
            freqHz_ += glideStepHz_;
            --glideSamplesRemaining_;
            if (glideSamplesRemaining_ == 0) {
                freqHz_ = targetFreqHz_;
                glideStepHz_ = 0.0f;
            }
        }

        if (releasing_) {
            envelope_ = std::max(0.0f, envelope_ - releaseStep_);
            if (envelope_ <= 0.0f) {
                hardSilence_();
                break;
            }
        } else {
            envelope_ = std::min(1.0f, envelope_ + attackStep_);
        }

        phase_ += freqHz_ / sampleRate_;
        if (phase_ >= 1.0f) phase_ -= std::floor(phase_);

        const float osc = (phase_ < duty) ? 1.0f : -1.0f;

        lpState_ += alpha * (osc - lpState_);
        const float hp = osc - lpState_;

        float shaped = osc;
        switch (filterType_) {
            case 0: shaped = lpState_; break;                  // LP
            case 1: shaped = (osc + hp) * 0.5f; break;         // EDGE
            case 2: shaped = hp; break;                        // HP
            case 3: default: shaped = osc; break;              // RAW
        }

        const float preDc = shaped * velocityGain_ * envelope_ * volume_ * 0.25f;
        const float dcBlocked =
            preDc - dcInputHistory_ + dcBlockR_ * dcOutputHistory_;
        dcInputHistory_ = preDc;
        dcOutputHistory_ = dcBlocked;

        peak_ = std::max(peak_, std::fabs(dcBlocked));
        buffer[i] += dcBlocked;
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
