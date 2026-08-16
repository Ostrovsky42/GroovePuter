#include <array>
#include <cstddef>
#include <cstdio>

#include "scenes.h"
#include "src/sampler/drum_sampler_track.h"
#include "src/sampler/ram_sample_store.h"
#include "src/sampler/sample_index.h"
#include "src/sampler/sampler_pool.h"
#include "src/sampler/sampler_voice.h"
#include "src/ui/pages/sampler_page.h"

namespace {

template <typename T>
void printSize(const char* name) {
  std::printf("%-32s %zu\n", name, sizeof(T));
}

}  // namespace

int main() {
  static_assert(kMaxSampleSlots == 64,
                "S1 measures the current 64-slot baseline; do not optimize it here");
  static_assert(SamplerPool::kMaxVoices == 8,
                "S1 must not reduce logical sampler polyphony");
  static_assert(DrumSamplerTrack::kNumPads == 16,
                "S1 must not reduce internal sampler pad count");

  std::printf("SAMPLER_MEMORY_LAYOUT_BEGIN\n");
  std::printf("host_pointer_bytes               %zu\n", sizeof(void*));
  std::printf("host_size_t_bytes                %zu\n", sizeof(std::size_t));

  printSize<SampleSlot>("SampleSlot");
  std::printf("%-32s %zu\n", "SampleSlot[64]",
              sizeof(std::array<SampleSlot, kMaxSampleSlots>));
  printSize<RamSampleStore>("RamSampleStore");
  printSize<SamplerVoice>("SamplerVoice");
  printSize<SamplerPool>("SamplerPool");
  printSize<SamplerPad>("SamplerPad");
  printSize<DrumSamplerTrack>("DrumSamplerTrack");
  printSize<SampleIndex>("SampleIndex");
  printSize<SampleFileInfo>("SampleFileInfo");
  printSize<SamplerPadState>("SamplerPadState");
  std::printf("%-32s %zu\n", "SamplerPadState[16]",
              sizeof(SamplerPadState) * 16u);
  printSize<SamplerPage>("SamplerPage");

  std::printf("SAMPLER_MEMORY_LAYOUT_END\n");
  return 0;
}
