#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "src/dsp/mini_drumvoices.h"

template <typename Voice>
void exerciseKick() {
    Voice voice(22050.0f);
    voice.triggerKick(true, 120);
    double energy = 0.0;
    for (int i = 0; i < 22050; ++i) {
        const float sample = voice.processKick();
        assert(std::isfinite(sample));
        energy += std::fabs(sample);
    }
    assert(energy > 1.0);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    if (std::strcmp(argv[1], "808") == 0) exerciseKick<TR808DrumSynthVoice>();
    else if (std::strcmp(argv[1], "909") == 0) exerciseKick<TR909DrumSynthVoice>();
    else return 2;
    std::puts("drum phase wrap: PASS");
}
