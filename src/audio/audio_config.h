#pragma once
#include <cstdint>

// Global Audio Contract
// All DSP code must use these constants.

static constexpr uint32_t kSampleRate = 22050;

// Block size for processing (usually 128 or 256)
// We are increasing this to 512 to give the DSP more time per buffer and reduce overhead.
static constexpr uint32_t kBlockFrames = 512; 

// 2ms fade time to prevent clicks
static constexpr uint32_t kFadeFrames = (kSampleRate * 2) / 1000;
