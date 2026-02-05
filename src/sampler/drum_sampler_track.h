#pragma once
#include "sampler_pool.h"
#include <array>

struct SamplerPad {
  SampleId id{0};
  float volume = 1.0f;     // Pad specific volume
  float pitch = 1.0f;      // Pad specific pitch
  uint32_t startFrame = 0;
  uint32_t endFrame = 0;   // 0 = end
  uint8_t chokeGroup = 0;  // 0 = none, 1-15 = groups
  bool reverse = false;
  bool loop = false;
};

class DrumSamplerTrack {
public:
  static constexpr int kNumPads = 16;
  
  DrumSamplerTrack();

  // Audio Thread: Trigger a pad
  void triggerPad(int padIndex, float velocity, ISampleStore& store, bool forceReverse = false);
  
  // Audio Thread: Stop a pad
  void stopPad(int padIndex);

  // Audio Thread: Process audio loop
  void process(float* output, uint32_t numFrames, ISampleStore& store);
  
  SamplerPad& pad(int index) { return pads_[index]; }
  const SamplerPad& pad(int index) const { return pads_[index]; }

private:
  std::array<SamplerPad, kNumPads> pads_;
  SamplerPool pool_;
};
