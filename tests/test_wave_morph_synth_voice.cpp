#include <cassert>
#include <cmath>
#include <string>

#include "src/dsp/wave_morph_synth_voice.h"

namespace {
float renderEnergy(WaveMorphSynthVoice& voice, int samples) {
    float energy = 0.0f;
    for (int i = 0; i < samples; ++i) {
        const float value = voice.process();
        assert(std::isfinite(value));
        assert(value >= -1.0001f && value <= 1.0001f);
        energy += std::fabs(value);
    }
    return energy;
}
}  // namespace

int main() {
    WaveMorphSynthVoice voice(22050.0f);
    assert(voice.parameterCount() == 6);
    assert(std::string(voice.getEngineName()) == "WAVEMORPH");

    voice.startNote(110.0f, false, false, 100);
    const float sawEnergy = renderEnergy(voice, 4096);
    assert(sawEnergy > 10.0f);

    voice.setParameterNormalized(0, 6.0f / 7.0f);
    voice.setParameterNormalized(1, 0.85f);
    voice.startNote(220.0f, true, true, 127);
    const float morphEnergy = renderEnergy(voice, 4096);
    assert(morphEnergy > 10.0f);
    assert(std::fabs(morphEnergy - sawEnergy) > 1.0f);

    voice.release();
    renderEnergy(voice, 12000);
    float tail = 0.0f;
    for (int i = 0; i < 512; ++i) tail += std::fabs(voice.process());
    assert(tail < 0.5f);

    voice.reset();
    assert(renderEnergy(voice, 256) == 0.0f);
    return 0;
}
