#include <cassert>
#include <cstdint>
#include <cstdlib>

#include "src/audio/follow_mute_ramp.h"

namespace {
void fill(int16_t* samples, int frames, int16_t value) {
    for (int i = 0; i < frames; ++i) samples[i] = value;
}
}  // namespace

int main() {
    constexpr int kFrames = 512;
    int16_t block[kFrames];

    // Unmuted steady state leaves the render untouched.
    fill(block, kFrames, 10000);
    assert(applyFollowMuteRamp(block, kFrames, 1.0f, 1.0f) == 1.0f);
    for (int i = 0; i < kFrames; ++i) assert(block[i] == 10000);

    // Muting fades over one block instead of cutting: no step at the start,
    // monotonic, silent at the end.
    fill(block, kFrames, 10000);
    assert(applyFollowMuteRamp(block, kFrames, 1.0f, 0.0f) == 0.0f);
    assert(block[0] > 9900);
    for (int i = 1; i < kFrames; ++i) assert(block[i] <= block[i - 1]);
    assert(std::abs(block[kFrames - 1]) < 50);

    // Muted steady state is exact silence.
    fill(block, kFrames, -12345);
    assert(applyFollowMuteRamp(block, kFrames, 0.0f, 0.0f) == 0.0f);
    for (int i = 0; i < kFrames; ++i) assert(block[i] == 0);

    // Unmuting fades back in.
    fill(block, kFrames, 10000);
    assert(applyFollowMuteRamp(block, kFrames, 0.0f, 1.0f) == 1.0f);
    assert(block[0] < 100);
    for (int i = 1; i < kFrames; ++i) assert(block[i] >= block[i - 1]);
    assert(block[kFrames - 1] > 9900);

    return 0;
}
