#include <cassert>
#include <cmath>
#include <iostream>

#include "src/dsp/mini_drumvoices.h"

namespace {
constexpr float kSampleRate = 22050.0f;

void requireFinite(float sample) {
    assert(std::isfinite(sample));
    assert(std::fabs(sample) < 4.0f);
}

void testHatClockDoesNotRequireKickProcessing() {
    TR606DrumSynthVoice voice(kSampleRate);
    voice.triggerHat(false, 100);

    float energy = 0.0f;
    float motion = 0.0f;
    float previous = 0.0f;
    for (int i = 0; i < 2048; ++i) {
        voice.beginSample();
        const float sample = voice.processHat();
        requireFinite(sample);
        energy += std::fabs(sample);
        motion += std::fabs(sample - previous);
        previous = sample;
    }

    assert(energy > 0.5f);
    assert(motion > 0.5f);
}

void testCymbalLaneIsBoundedWithoutKickProcessing() {
    TR606DrumSynthVoice voice(kSampleRate);
    voice.triggerRim(true, 100);  // Generic RS lane carries 606 CY.

    float energy = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < 8192; ++i) {
        voice.beginSample();
        const float sample = voice.processRim();
        requireFinite(sample);
        energy += std::fabs(sample);
        peak = std::max(peak, std::fabs(sample));
    }

    assert(energy > 1.0f);
    assert(peak < 1.5f);
}
}  // namespace

int main() {
    testHatClockDoesNotRequireKickProcessing();
    testCymbalLaneIsBoundedWithoutKickProcessing();
    std::cout << "TR-606 drum voice tests: PASS\n";
    return 0;
}
