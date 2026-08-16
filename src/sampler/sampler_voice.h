#pragma once
#include "sample_store.h"
#include <atomic>
#include <cstdint>

// SamplerVoice manages the playback state of a single sample instance.
// Designed for the audio thread. Resident samples keep the pointer-fast path;
// streamed samples consume only already-published fixed-cache pages.
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

  // Audio Thread: Start playback. Never performs filesystem I/O.
  void trigger(const Params& params, ISampleStore& store);

  // Audio Thread: Stop playback (with fade out)
  void stop();

  // Audio Thread: Render audio into a mono buffer
  void process(float* output, uint32_t numFrames, ISampleStore& store);

  // Audio Thread: Render exactly one frame after current-frame sequencer
  // dispatch. Resident sources use cached pcm_. Streamed sources use the
  // store's lock-free READY-page reader and request queue only.
  inline __attribute__((always_inline)) void processFrame(
      float& output, ISampleStore& store) {
    if (!active_) return;

    const float pos = position_;
    const int i0 = static_cast<int>(pos);
    if (i0 < static_cast<int>(startFrame_) ||
        i0 >= static_cast<int>(endFrame_)) {
      releaseHandle_(store);
      return;
    }

    int16_t sample16 = 0;
    if (streamed_) {
      const uint32_t frame = static_cast<uint32_t>(i0);
      if (!store.readFrameHandle(handle_, frame, sample16)) {
        // A miss never performs I/O here. Hold the source position and ask the
        // control loop to publish the missing page. This degrades locally to
        // silence rather than blocking the audio task.
        store.requestFrameHandle(handle_, frame);
        if (!starving_) {
          starving_ = true;
          starvationFrames_ = 0;
          store.noteStreamStarve();
        }
        ++starvationFrames_;
        if (starvationFrames_ >= kStreamDropFrames) {
          store.noteStreamDrop();
          releaseHandle_(store);
        }
        return;
      }

      starving_ = false;
      starvationFrames_ = 0;
      requestStreamWindow_(store, frame);
    } else {
      if (pcm_ == nullptr) return;
      sample16 = pcm_[i0];
    }

    float sample = static_cast<float>(sample16);
    if (interpolate_) {
      int nextIndex = reverse_ ? i0 - 1 : i0 + 1;
      float next = sample;
      if (nextIndex >= static_cast<int>(startFrame_) &&
          nextIndex < static_cast<int>(endFrame_)) {
        if (streamed_) {
          int16_t next16 = 0;
          if (store.readFrameHandle(handle_, static_cast<uint32_t>(nextIndex),
                                    next16)) {
            next = static_cast<float>(next16);
          } else {
            store.requestFrameHandle(handle_, static_cast<uint32_t>(nextIndex));
          }
        } else {
          next = static_cast<float>(pcm_[nextIndex]);
        }
      }

      const float frac = pos - static_cast<float>(i0);
      if (reverse_) {
        sample += (1.0f - frac) * (next - sample);
      } else {
        sample += frac * (next - sample);
      }
    }

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

    output += sample * fadeGain * pcmGain_;
    position_ += step_;

    if (reverse_) {
      if (position_ < static_cast<float>(startFrame_)) {
        if (loop_) {
          position_ = static_cast<float>(endFrame_ - 1);
          if (streamed_) {
            lastStreamPageStart_ = kInvalidStreamPage;
            store.requestFrameHandle(handle_, endFrame_ - 1);
          }
        } else {
          releaseHandle_(store);
        }
      }
    } else if (position_ >= static_cast<float>(endFrame_)) {
      if (loop_) {
        position_ = static_cast<float>(startFrame_);
        if (streamed_) {
          lastStreamPageStart_ = kInvalidStreamPage;
          store.requestFrameHandle(handle_, startFrame_);
        }
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
  // These constants intentionally mirror the V1 RamSampleStore page contract.
  // Keeping the request horizon at two pages gives about 23 ms at 22.05 kHz
  // pitch 1 (about 11.6 ms at pitch 2), while eight physical pages bound the
  // intended four-concurrent-stream research target.
  static constexpr uint32_t kStreamPageFrames = 256;
  static constexpr uint32_t kStreamPrefetchPages = 2;
  static constexpr uint32_t kInvalidStreamPage = 0xFFFFFFFFu;
  static constexpr uint32_t kStreamDropFrames = kSampleRate / 20; // 50 ms

  SampleHandle handle_;
  const int16_t* pcm_ = nullptr;  // resident-only; pinned by handle_
  float position_ = 0.0f;
  int tag_ = -1;

  float step_ = 1.0f;
  float pcmGain_ = 1.0f / 32768.0f;
  uint32_t startFrame_ = 0;
  uint32_t endFrame_ = 0;
  bool reverse_ = false;
  bool loop_ = false;
  bool interpolate_ = false;
  bool streamed_ = false;

  bool active_ = false;

  uint32_t fadeCounter_ = 0;
  bool fadingOut_ = false;

  uint32_t starvationFrames_ = 0;
  bool starving_ = false;
  uint32_t lastStreamPageStart_ = kInvalidStreamPage;

  inline __attribute__((always_inline)) void requestStreamWindow_(
      ISampleStore& store, uint32_t frame) {
    const uint32_t pageStart =
        (frame / kStreamPageFrames) * kStreamPageFrames;
    if (pageStart == lastStreamPageStart_) return;
    lastStreamPageStart_ = pageStart;

    if (!reverse_) {
      for (uint32_t page = 1; page <= kStreamPrefetchPages; ++page) {
        const uint64_t target64 =
            static_cast<uint64_t>(pageStart) +
            static_cast<uint64_t>(page) * kStreamPageFrames;
        if (target64 >= endFrame_) break;
        store.requestFrameHandle(handle_, static_cast<uint32_t>(target64));
      }
      return;
    }

    for (uint32_t page = 1; page <= kStreamPrefetchPages; ++page) {
      const uint64_t distance =
          static_cast<uint64_t>(page - 1u) * kStreamPageFrames + 1u;
      if (static_cast<uint64_t>(pageStart) < distance) break;
      const uint32_t target =
          static_cast<uint32_t>(static_cast<uint64_t>(pageStart) - distance);
      if (target < startFrame_) break;
      store.requestFrameHandle(handle_, target);
    }
  }

  void reset();
  void releaseHandle_(ISampleStore& store);
};
