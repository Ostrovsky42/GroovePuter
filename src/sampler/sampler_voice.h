#pragma once
#include "sample_store.h"
#include <atomic>

// SamplerVoice manages the playback state of a single sample instance.
// Designed for the audio thread.
class SamplerVoice {
public:
  struct Params {
    SampleId id;
    uint32_t startFrame = 0;
    uint32_t endFrame = 0; // 0 = end of sample
    float pitch = 1.0f;
    float gain = 1.0f;
    bool reverse = false;
    bool loop = false;
  };

  SamplerVoice();

  // Audio Thread: Start playback. Note: will call store.acquire(id)
  void trigger(const Params& params, ISampleStore& store);
  
  // Audio Thread: Stop playback (with fade out)
  void stop();

  // Audio Thread: Render audio into a mono buffer
  // Note: will call store.release(id) when playback finishes.
  void process(float* output, uint32_t numFrames, ISampleStore& store);

  bool isActive() const { return active_; }
  
  // Tag used for choke groups or identifying the source (e.g. pad index)
  int tag() const { return tag_; }
  void setTag(int t) { tag_ = t; }

private:
  SampleHandle handle_;  // Handle to acquired slot
  double position_ = 0;
  int tag_ = -1;
  
  // Internal playback state
  double playbackRate_ = 1.0;
  float gain_ = 1.0f;
  uint32_t startFrame_ = 0;
  uint32_t endFrame_ = 0;
  bool reverse_ = false;
  bool loop_ = false;
  
  bool active_ = false;
  
  // Fade to prevent clicks
  uint32_t fadeCounter_ = 0;
  bool fadingOut_ = false;
  
  void reset();
};
