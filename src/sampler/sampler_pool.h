#pragma once
#include "sampler_voice.h"
#include <array>

class SamplerPool {
public:
  static constexpr int kMaxVoices = 8;

  SamplerPool();

  // Audio Thread: Trigger a new sample. Will find appropriate voice.
  // tag: optional identifier (e.g. pad index)
  void trigger(const SamplerVoice::Params& params, ISampleStore& store, int tag = -1);
  
  // Audio Thread: Render and mix all active voices.
  void process(float* output, uint32_t numFrames, ISampleStore& store);

  // Audio Thread: Render one frame after same-frame trigger dispatch.
  void processFrame(float& output, ISampleStore& store);

  // Stop all voices immediately
  void stopAll();
  
  // Stop all voices matching a specific tag
  void stopByTag(int tag);

private:
  std::array<SamplerVoice, kMaxVoices> voices_;
};
