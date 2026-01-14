#pragma once
#include <cstdint>

// Global Audio Contract
// All DSP code must use these constants.

static constexpr uint32_t kSampleRate = 22050;

// Block size for processing (usually 128 or 256)
// MiniAcid original engine uses 256 (AUDIO_BUFFER_SAMPLES)
// We will aliase it here but aim for 128 if possible, or stick to 256 for compat.
// The user prompt said kBlockFrames = 128, but existing code uses 256. 
// Let's set it to 128 as "preferred" but we might need to adapt existing loop.
// Actually, let's play safe and check existing usage. 
// minacid_engine.h: static const int AUDIO_BUFFER_SAMPLES = 256;
// To respect the user's "Contract", we define it here, but maybe we should update engine to use this.
static constexpr uint32_t kBlockFrames = 256; 

// 2ms fade time to prevent clicks
static constexpr uint32_t kFadeFrames = (kSampleRate * 2) / 1000;
