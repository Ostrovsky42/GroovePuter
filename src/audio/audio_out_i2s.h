#pragma once

#include <cstdint>
#include <cstddef>
#include "driver/i2s_std.h"

// Direct I2S audio output wrapper (New ESP-IDF Driver)
// Replaces M5Cardputer.Speaker.playRaw() with continuous DMA streaming
// Eliminates gaps/crackling caused by polling-based buffer submission

class AudioOutI2S {
public:
  AudioOutI2S();
  ~AudioOutI2S();
  
  // Initialize I2S driver
  // sampleRate: audio sample rate (e.g., 22050)
  // bufferFrames: frames per write call (e.g., 512)
  bool begin(uint32_t sampleRate, size_t bufferFrames);
  
  // Write mono audio buffer (will be duplicated to stereo L=R)
  // Returns true on success
  bool writeMono16(const int16_t* monoBuffer, size_t frames);
  
  // Stop and cleanup I2S driver
  void end();
  
private:
  uint32_t sampleRate_;
  size_t bufferFrames_;
  int16_t* stereoBuffer_;  // Internal stereo conversion buffer
  i2s_chan_handle_t tx_handle_; // New driver handle
};
