#include "sampler_voice.h"
#include <algorithm>

SamplerVoice::SamplerVoice() {
  reset();
}

void SamplerVoice::reset() {
  handle_ = SampleHandle::invalid();
  pcm_ = nullptr;
  position_ = 0.0f;
  step_ = 1.0f;
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
  step_ = std::max(0.0f, params.pitch) *
          (static_cast<float>(view.sampleRate) /
           static_cast<float>(kSampleRate));
  if (reverse_) step_ = -step_;
  const float absStep = step_ < 0.0f ? -step_ : step_;
  interpolate_ = absStep < 0.99999f || absStep > 1.00001f;
  pcmGain_ = params.gain * (1.0f / 32768.0f);

  position_ = reverse_
      ? static_cast<float>(actualEnd - 1)
      : static_cast<float>(actualStart);

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

void SamplerVoice::releaseHandle_(ISampleStore& store) {
  if (handle_.valid()) store.releaseHandle(handle_);
  handle_ = SampleHandle::invalid();
  pcm_ = nullptr;
  active_ = false;
}
