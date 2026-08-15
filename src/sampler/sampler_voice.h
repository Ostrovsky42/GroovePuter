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

  // Audio Thread: Render exactly one frame after current-frame sequencer
  // dispatch. Uses metadata cached by trigger(), so it performs no store lookup.
  inline __attribute__((always_inline)) void processFrame(
      float& output, ISampleStore& store) {
    if (!active_ || pcm_ == nullptr) return;

    // The admitted mono pool is at most 16,384 frames, so float retains ample
    // playhead precision while avoiding software double/floor in this hot path.
    const float pos = position_;
    const int i0 = static_cast<int>(pos);
    const int i1 = i0 + 1;

    if (i0 < static_cast<int>(startFrame_) ||
        i0 >= static_cast<int>(endFrame_)) {
      releaseHandle_(store);
      return;
    }

    const float s0 = static_cast<float>(pcm_[i0]) * (1.0f / 32768.0f);
    float s1 = s0;
    if (i1 < static_cast<int>(endFrame_)) {
      s1 = static_cast<float>(pcm_[i1]) * (1.0f / 32768.0f);
    }
    const float sample = s0 + (pos - static_cast<float>(i0)) * (s1 - s0);

    float fadeGain = 1.0f;
    if (fadingOut_) {
      fadeGain = static_cast<float>(fadeCounter_) /
                 static_cast<float>(kFadeFrames);
      if (fadeCounter_ > 0) {
        --fadeCounter_;
      } else {
        releaseHandle_(store);
        return;
      }
    } else if (fadeCounter_ > 0) {
      fadeGain = 1.0f -
                 (static_cast<float>(fadeCounter_) /
                  static_cast<float>(kFadeFrames));
      --fadeCounter_;
    }

    output += sample * fadeGain * gain_;
    position_ += step_;

    if (reverse_) {
      if (position_ < static_cast<float>(startFrame_)) {
        if (loop_) {
          position_ = static_cast<float>(endFrame_ - 1);
        } else {
          releaseHandle_(store);
        }
      }
    } else if (position_ >= static_cast<float>(endFrame_)) {
      if (loop_) {
        position_ = static_cast<float>(startFrame_);
      } else {
        releaseHandle_(store);
      }
    }
  }

  bool isActive() const { return active_; }
  
  // Tag used for choke groups or identifying the source (e.g. pad index)
  int tag() const { return tag_; }
  void setTag(int t) { tag_ = t; }

private:
  SampleHandle handle_;  // Handle to acquired slot
  const int16_t* pcm_ = nullptr;  // Pinned by handle_ while the voice is active.
  float position_ = 0.0f;
  int tag_ = -1;
  
  // Internal playback state
  float step_ = 1.0f;
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
  void releaseHandle_(ISampleStore& store);
};
