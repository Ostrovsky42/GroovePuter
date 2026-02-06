#pragma once
#include <cstdint>

// Global Audio Contract
// All DSP code must use these constants.

static constexpr uint32_t kSampleRate = 22050;

// Block size for processing (usually 128 or 256)
// Increased to 1024 (46ms) to handle heavy DSP load (tape/reverb) and prevent underruns.
static constexpr uint32_t kBlockFrames = 1024; 

// 2ms fade time to prevent clicks
static constexpr uint32_t kFadeFrames = (kSampleRate * 2) / 1000;
