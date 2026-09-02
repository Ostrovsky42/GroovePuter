#include "../src/sampler/sampler_voice.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

class FakeSampleStore final : public ISampleStore {
public:
  explicit FakeSampleStore(std::vector<int16_t> pcm)
      : pcm_(std::move(pcm)) {}

  bool preload(SampleId) override { return true; }

  SampleHandle acquireHandle(SampleId id) override {
    if (id.value != sampleId_.value || acquired_) return SampleHandle::invalid();
    acquired_ = true;
    ++acquireCount_;
    return {0, id};
  }

  void releaseHandle(SampleHandle h) override {
    assert(h.valid());
    assert(acquired_);
    acquired_ = false;
    ++releaseCount_;
  }

  SampleView viewHandle(SampleHandle h) const override {
    if (!h.valid() || !acquired_) return {nullptr, 0, 0};
    return {pcm_.data(), static_cast<uint32_t>(pcm_.size()), kSampleRate};
  }

  void acquire(SampleId) override {}
  void release(SampleId) override {}
  SampleView view(SampleId) const override { return {nullptr, 0, 0}; }
  void evictLRU() override {}
  std::size_t freePoolBytes() const override { return 0; }
  void setPoolSize(std::size_t) override {}

  int acquireCount() const { return acquireCount_; }
  int releaseCount() const { return releaseCount_; }
  bool acquired() const { return acquired_; }

private:
  const SampleId sampleId_{42};
  std::vector<int16_t> pcm_;
  bool acquired_ = false;
  int acquireCount_ = 0;
  int releaseCount_ = 0;
};

void testReverseDefaultEndStartsAtLastFrame() {
  FakeSampleStore store({1000, 2000, 3000, 4000});
  SamplerVoice voice;

  SamplerVoice::Params params{};
  params.id = {42};
  params.reverse = true;
  params.loop = true;
  params.pitch = 1.0f;
  params.gain = 1.0f;
  params.endFrame = 0;  // Must mean the real end of the sample.

  voice.trigger(params, store);
  assert(voice.isActive());
  assert(store.acquireCount() == 1);

  std::array<float, kFadeFrames + 4> output{};
  voice.process(output.data(), output.size(), store);

  const float expectedLastFrame = 4000.0f / 32768.0f;
  assert(std::fabs(output[kFadeFrames] - expectedLastFrame) < 0.0001f);
  assert(store.releaseCount() == 0);
  assert(store.acquired());
}

void testReverseOneShotReleasesHandle() {
  FakeSampleStore store({1000, 2000, 3000, 4000});
  SamplerVoice voice;

  SamplerVoice::Params params{};
  params.id = {42};
  params.reverse = true;
  params.loop = false;
  params.pitch = 1.0f;
  params.gain = 1.0f;

  voice.trigger(params, store);
  std::array<float, 16> output{};
  voice.process(output.data(), output.size(), store);

  assert(!voice.isActive());
  assert(store.acquireCount() == 1);
  assert(store.releaseCount() == 1);
  assert(!store.acquired());
}

void testFrameRendererMatchesBlockRenderer() {
  const std::vector<int16_t> pcm = {1000, 2000, 3000, 4000, 3000, 2000, 1000};
  FakeSampleStore blockStore(pcm);
  FakeSampleStore frameStore(pcm);
  SamplerVoice blockVoice;
  SamplerVoice frameVoice;

  SamplerVoice::Params params{};
  params.id = {42};
  params.pitch = 0.75f;
  params.gain = 0.8f;

  blockVoice.trigger(params, blockStore);
  frameVoice.trigger(params, frameStore);

  std::array<float, 16> blockOutput{};
  std::array<float, 16> frameOutput{};
  blockVoice.process(blockOutput.data(), blockOutput.size(), blockStore);
  for (float& sample : frameOutput) {
    frameVoice.processFrame(sample, frameStore);
  }

  for (std::size_t i = 0; i < blockOutput.size(); ++i) {
    assert(std::fabs(blockOutput[i] - frameOutput[i]) < 0.000001f);
  }
  assert(blockVoice.isActive() == frameVoice.isActive());
  assert(blockStore.acquireCount() == frameStore.acquireCount());
  assert(blockStore.releaseCount() == frameStore.releaseCount());
}

int main() {
  testReverseDefaultEndStartsAtLastFrame();
  testReverseOneShotReleasesHandle();
  testFrameRendererMatchesBlockRenderer();
  return 0;
}
