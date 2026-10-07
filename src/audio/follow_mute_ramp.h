#pragma once

#include <cstddef>
#include <cstdint>

// Silences or restores GroovePuter's rendered block without a click: the gain
// moves linearly from `fromGain` to `toGain` across the block. Rendering itself
// keeps running, so the sequencer stays in phase with an external master.
// Returns the gain reached at the end of the block.
inline float applyFollowMuteRamp(int16_t* samples,
                                 std::size_t frames,
                                 float fromGain,
                                 float toGain) {
    if (frames == 0) return toGain;
    if (fromGain >= 1.0f && toGain >= 1.0f) return 1.0f;
    if (fromGain <= 0.0f && toGain <= 0.0f) {
        for (std::size_t i = 0; i < frames; ++i) samples[i] = 0;
        return 0.0f;
    }
    const float step = (toGain - fromGain) / static_cast<float>(frames);
    float gain = fromGain;
    for (std::size_t i = 0; i < frames; ++i) {
        gain += step;
        samples[i] = static_cast<int16_t>(static_cast<float>(samples[i]) * gain);
    }
    return toGain;
}
