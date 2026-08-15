#include "sampler_voice.h"
#include <cmath>
#include <algorithm>

SamplerVoice::SamplerVoice() {
  reset();
}

void SamplerVoice::reset() {
  handle_ = SampleHandle::invalid();
  pcm_ = nullptr;
  position_ = 0;
  step_ = 1.0;
  active_ = false;
  fadingOut_ = false;
  fadeCounter_ = 0;
}

void SamplerVoice::trigger(const Params& params, ISampleStore& store) {
  if (handle_.valid()) releaseHandle_(store);

  handle_ = store.acquireHandle(params.id);
  if (!handle_.valid()) {
    pcm_ = nullptr;
    active_ = false;
    return;
  }

  const SampleView view = store.viewHandle(handle_);
  if (view.empty()) {
    releaseHandle_(store);
    return;
  }

  const uint32_t actualEnd =
      (params.endFrame == 0 || params.endFrame > view.frames)
          ? view.frames
          : params.endFrame;
  const uint32_t actualStart =
      (params.startFrame < actualEnd) ? params.startFrame : 0;

  if (actualEnd <= actualStart) {
    releaseHandle_(store);
    return;
  }

  pcm_ = view.pcm;
  startFrame_ = actualStart;
  endFrame_ = actualEnd;
  reverse_ = params.reverse;
  loop_ = params.loop;
  playbackRate_ = std::max(0.0f, params.pitch);
  step_ = playbackRate_ *
          (static_cast<double>(view.sampleRate) /
           static_cast<double>(kSampleRate));
  if (reverse_) step_ = -step_;
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

  for (uint32_t i = 0; i < numFrames; ++i) {
    processFrame(output[i], store);
    if (!active_) break;
  }
}

void SamplerVoice::processFrame(float& output, ISampleStore& store) {
  if (!active_ || pcm_ == nullptr) return;

  const double pos = position_;
  const int i0 = static_cast<int>(std::floor(pos));
  const int i1 = i0 + 1;

  if (i0 < static_cast<int>(startFrame_) ||
      i0 >= static_cast<int>(endFrame_)) {
    releaseHandle_(store);
    return;
  }

  const float s0 = static_cast<float>(pcm_[i0]) / 32768.0f;
  float s1 = s0;
  if (i1 < static_cast<int>(endFrame_)) {
    s1 = static_cast<float>(pcm_[i1]) / 32768.0f;
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
    if (position_ < static_cast<double>(startFrame_)) {
      if (loop_) {
        position_ = static_cast<double>(endFrame_ - 1);
      } else {
        releaseHandle_(store);
      }
    }
  } else if (position_ >= static_cast<double>(endFrame_)) {
    if (loop_) {
      position_ = static_cast<double>(startFrame_);
    } else {
      releaseHandle_(store);
    }
  }
}

void SamplerVoice::releaseHandle_(ISampleStore& store) {
  if (handle_.valid()) store.releaseHandle(handle_);
  handle_ = SampleHandle::invalid();
  pcm_ = nullptr;
  active_ = false;
}
