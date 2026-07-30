#include "sampler_voice.h"
#include <cmath>
#include <algorithm>

SamplerVoice::SamplerVoice() {
  reset();
}

void SamplerVoice::reset() {
  handle_ = SampleHandle::invalid();
  position_ = 0;
  active_ = false;
  fadingOut_ = false;
  fadeCounter_ = 0;
}

void SamplerVoice::trigger(const Params& params, ISampleStore& store) {
  if (active_ && handle_.valid()) {
    store.releaseHandle(handle_);
  }

  handle_ = store.acquireHandle(params.id);
  if (!handle_.valid()) {
    active_ = false;
    return;
  }

  const SampleView view = store.viewHandle(handle_);
  if (view.empty()) {
    store.releaseHandle(handle_);
    handle_ = SampleHandle::invalid();
    active_ = false;
    return;
  }

  const uint32_t actualEnd =
      (params.endFrame == 0 || params.endFrame > view.frames)
          ? view.frames
          : params.endFrame;
  const uint32_t actualStart =
      (params.startFrame < actualEnd) ? params.startFrame : 0;

  if (actualEnd <= actualStart) {
    store.releaseHandle(handle_);
    handle_ = SampleHandle::invalid();
    active_ = false;
    return;
  }

  startFrame_ = actualStart;
  endFrame_ = actualEnd;
  reverse_ = params.reverse;
  loop_ = params.loop;
  playbackRate_ = std::max(0.0f, params.pitch);
  gain_ = params.gain;

  position_ = reverse_
      ? static_cast<double>(actualEnd - 1)
      : static_cast<double>(actualStart);

  active_ = true;
  fadingOut_ = false;
  fadeCounter_ = kFadeFrames;
}

void SamplerVoice::stop() {
  if (active_ && !fadingOut_) {
    fadingOut_ = true;
    fadeCounter_ = kFadeFrames;
  }
}

void SamplerVoice::process(float* output, uint32_t numFrames, ISampleStore& store) {
  if (!active_ || !output || numFrames == 0) return;

  const SampleView view = store.viewHandle(handle_);
  if (view.empty()) {
    if (handle_.valid()) store.releaseHandle(handle_);
    handle_ = SampleHandle::invalid();
    active_ = false;
    return;
  }

  const int16_t* pcm = view.pcm;
  const uint32_t totalFrames = view.frames;
  const uint32_t actualEnd =
      (endFrame_ == 0 || endFrame_ > totalFrames) ? totalFrames : endFrame_;
  const uint32_t actualStart = (startFrame_ < actualEnd) ? startFrame_ : 0;

  if (actualEnd <= actualStart) {
    store.releaseHandle(handle_);
    handle_ = SampleHandle::invalid();
    active_ = false;
    return;
  }

  const float srScale = static_cast<float>(view.sampleRate) /
                        static_cast<float>(kSampleRate);
  double step = playbackRate_ * srScale;
  if (reverse_) step = -step;

  for (uint32_t i = 0; i < numFrames; ++i) {
    const double pos = position_;
    const int i0 = static_cast<int>(std::floor(pos));
    const int i1 = i0 + 1;

    if (i0 < static_cast<int>(actualStart) ||
        i0 >= static_cast<int>(actualEnd)) {
      store.releaseHandle(handle_);
      handle_ = SampleHandle::invalid();
      active_ = false;
      break;
    }

    const float s0 = static_cast<float>(pcm[i0]) / 32768.0f;
    float s1 = s0;
    if (i1 < static_cast<int>(actualEnd)) {
      s1 = static_cast<float>(pcm[i1]) / 32768.0f;
    }

    const float frac = static_cast<float>(pos - static_cast<double>(i0));
    const float sample = s0 + frac * (s1 - s0);

    float fadeGain = 1.0f;
    if (fadingOut_) {
      fadeGain = static_cast<float>(fadeCounter_) /
                 static_cast<float>(kFadeFrames);
      if (fadeCounter_ > 0) {
        --fadeCounter_;
      } else {
        store.releaseHandle(handle_);
        handle_ = SampleHandle::invalid();
        active_ = false;
        break;
      }
    } else if (fadeCounter_ > 0) {
      fadeGain = 1.0f -
                 (static_cast<float>(fadeCounter_) /
                  static_cast<float>(kFadeFrames));
      --fadeCounter_;
    }

    output[i] += sample * fadeGain * gain_;
    position_ += step;

    bool finished = false;
    if (reverse_) {
      if (position_ < static_cast<double>(actualStart)) {
        if (loop_) {
          position_ = static_cast<double>(actualEnd - 1);
        } else {
          finished = true;
        }
      }
    } else if (position_ >= static_cast<double>(actualEnd)) {
      if (loop_) {
        position_ = static_cast<double>(actualStart);
      } else {
        finished = true;
      }
    }

    if (finished) {
      store.releaseHandle(handle_);
      handle_ = SampleHandle::invalid();
      active_ = false;
      break;
    }
  }
}
