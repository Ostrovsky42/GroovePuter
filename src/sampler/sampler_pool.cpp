#include "sampler_pool.h"

SamplerPool::SamplerPool() {
}

void SamplerPool::trigger(const SamplerVoice::Params& params, ISampleStore& store, int tag) {
  // Find first inactive voice
  for (auto& voice : voices_) {
    if (!voice.isActive()) {
      voice.setTag(tag);
      voice.trigger(params, store);
      return;
    }
  }
  
  // If no inactive voice, steal the first one
  voices_[0].stop(); 
  voices_[0].setTag(tag);
  voices_[0].trigger(params, store);
}

void SamplerPool::process(float* output, uint32_t numFrames, ISampleStore& store) {
  for (auto& voice : voices_) {
    if (voice.isActive()) {
      voice.process(output, numFrames, store);
    }
  }
}

void SamplerPool::stopAll() {
  for (auto& voice : voices_) {
      if (voice.isActive()) {
          voice.stop();
      }
  }
}

void SamplerPool::stopByTag(int tag) {
  if (tag < 0) return;
  for (auto& voice : voices_) {
    if (voice.isActive() && voice.tag() == tag) {
      voice.stop();
    }
  }
}
