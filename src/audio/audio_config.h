#pragma once
#include <cstdint>

// Global Audio Contract
// All DSP code must use these constants.

// Stable hardware default for Cardputer DRAM-only builds.
// 44.1 kHz can be re-enabled later, but it noticeably increases CPU pressure.
static constexpr uint32_t kSampleRate = 22050;
// static constexpr uint32_t kSampleRate = 44100; // Optional high-quality mode

// Block size for processing (usually 128 or 256)
// 512 keeps audio robust while avoiding the "sluggish controls" feel of 1024.
static constexpr uint32_t kBlockFrames = 512;
// static constexpr uint32_t kBlockFrames = 1024; // Optional extra underrun headroom

// 2ms fade time to prevent clicks
static constexpr uint32_t kFadeFrames = (kSampleRate * 2) / 1000;
