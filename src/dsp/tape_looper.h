#pragma once
#include <cstdint>
#include "../audio/audio_config.h"

// TapeLooper provides an 8-second mono ring buffer for looping the final mix.
// - Fixed RAM allocation (approx 344KB)
// - Simple Record/Play/Overdub
class TapeLooper {
public:
  static constexpr uint32_t kMaxSeconds = 8;
  static constexpr uint32_t kMaxSamples = kMaxSeconds * kSampleRate;

  TapeLooper();
  ~TapeLooper();

  // Initialize looper with dynamic memory.
  // Attempts to allocate 'maxSeconds' long buffer.
  // Priority: PSRAM, fallback: DRAM.
  // Returns true if any buffer was allocated.
  bool init(uint32_t maxSeconds);

  void setRecording(bool on) { recording_ = on; if(on) firstRecord_ = (length_ == 0); }
  void setPlaying(bool on) { playing_ = on; }
  void setOverdub(bool on) { overdub_ = on; }
  void setVolume(float v) { volume_ = v; }
  void clear();

  // Process a block
  // input: signal to be potentially recorded.
  // loopPart: output of the looper to be mixed into final signal.
  void process(float input, float* loopPart);

private:
  // Using a static/permanent buffer for reliability
  // Since we are in Agnetic mode and this is C++, we will use a unique_ptr or static array.
  // Given "static memory" requirement, a static array is safest if we can afford the BSS.
  // 176400 * 2 = 352800 bytes. Cardputer has enough PSRAM/RAM?
  // Actually, M5Stack Cardputer (ESP32-S3) has 2MB/8MB PSRAM.
  // We will assume it's okay for now.
  int16_t* buffer_ = nullptr;
  uint32_t maxSamples_ = 0;
  
  uint32_t length_ = 0;       // Current loop length in samples
  uint32_t pos_ = 0;          // Current play/record position
  
  bool recording_ = false;
  bool playing_ = false;
  bool overdub_ = false;
  bool firstRecord_ = false;
  float volume_ = 1.0f;
  
  // Fade/Smooth for loop points
  static constexpr uint32_t kCrossfadeFrames = 256;
};
