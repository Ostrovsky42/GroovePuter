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
  // Release previous handle if active
  if (active_ && handle_.valid()) {
    store.releaseHandle(handle_);
  }
  
  // Acquire new handle (binds us to a specific slot)
  handle_ = store.acquireHandle(params.id);
  if (!handle_.valid()) {
    active_ = false;
    return;
  }
  
  startFrame_ = params.startFrame;
  endFrame_ = params.endFrame;
  reverse_ = params.reverse;
  loop_ = params.loop;
  playbackRate_ = params.pitch;
  gain_ = params.gain;
  
  if (reverse_) {
    position_ = (endFrame_ > 0) ? (double)endFrame_ - 1.0 : 0; 
  } else {
    position_ = (double)startFrame_;
  }
  
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
  if (!active_) return;

  // O(1) view via handle - no search
  SampleView view = store.viewHandle(handle_);
  if (view.empty()) {
    if (handle_.valid()) store.releaseHandle(handle_);
    handle_ = SampleHandle::invalid();
    active_ = false;
    return;
  }

  const int16_t* pcm = view.pcm;
  uint32_t totalFrames = view.frames;
  
  uint32_t actualEnd = (endFrame_ == 0 || endFrame_ > totalFrames) ? totalFrames : endFrame_;
  uint32_t actualStart = (startFrame_ >= actualEnd) ? 0 : startFrame_;

  float srScale = (float)view.sampleRate / (float)kSampleRate;
  double step = playbackRate_ * srScale;
  if (reverse_) step = -step;

  for (uint32_t i = 0; i < numFrames; ++i) {
    double pos = position_;
    int i0 = (int)pos;
    int i1 = i0 + 1;
    
    if (i0 < 0 || i0 >= (int)totalFrames) {
      if (handle_.valid()) store.releaseHandle(handle_);
      handle_ = SampleHandle::invalid();
      active_ = false; 
      break;
    }
    
    float s0 = (float)pcm[i0] / 32768.0f;
    float s1 = s0;
    if (i1 < (int)totalFrames) {
      s1 = (float)pcm[i1] / 32768.0f;
    }
    
    float frac = (float)(pos - i0);
    float sample = s0 + frac * (s1 - s0);

    float gain = 1.0f;
    if (fadingOut_) {
      gain = (float)fadeCounter_ / (float)kFadeFrames;
      if (fadeCounter_ > 0) fadeCounter_--;
      else {
        if (handle_.valid()) store.releaseHandle(handle_);
        handle_ = SampleHandle::invalid();
        active_ = false;
        break;
      }
    } else if (fadeCounter_ > 0) {
      gain = 1.0f - ((float)fadeCounter_ / (float)kFadeFrames);
      fadeCounter_--;
    }

    output[i] += sample * gain * gain_;
    position_ += step;

    bool finished = false;
    if (reverse_) {
      if (position_ < (double)actualStart) {
        if (loop_) position_ = (double)actualEnd - 1.0;
        else finished = true;
      }
    } else {
      if (position_ >= (double)actualEnd - 1.0) {
        if (loop_) position_ = (double)actualStart;
        else finished = true;
      }
    }

    if (finished) {
      if (handle_.valid()) store.releaseHandle(handle_);
      handle_ = SampleHandle::invalid();
      active_ = false;
      break;
    }
  }
}
