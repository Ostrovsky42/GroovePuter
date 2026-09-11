#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "src/state/working_material_storage.h"

namespace {

SynthPattern makePattern() {
  SynthPattern pattern{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    auto& step = pattern.steps[i];
    step.note = static_cast<int8_t>(24 + i);
    step.slide = (i % 2) != 0;
    step.accent = (i % 3) == 0;
    step.ghost = (i % 4) == 0;
    step.velocity = static_cast<uint8_t>(61 + i);
    step.timing = static_cast<int8_t>(i - 8);
    step.fx = static_cast<uint8_t>(i % 3);
    step.fxParam = static_cast<uint8_t>(17 + i * 5);
    step.probability = static_cast<uint8_t>(55 + i);
  }
  return pattern;
}

PhraseRuntime::RuntimeSynthEventBuffer makeMelody() {
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar * 4u;
  melody.count = 3;
  melody.events[0] = {0, 192, 48, 81, 97, PhraseRuntime::kEventAccent, 1, 17};
  melody.events[1] = {384, 288, 55, 73, 83, PhraseRuntime::kEventSlide, 2, 29};
  melody.events[2] = {1152, 96, 67, 64, 71, PhraseRuntime::kEventGhost, 0, 0};
  return melody;
}

}  // namespace

int main() {
  using GroovePuterMaterial::WorkingMaterialStorage;
  using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

  static_assert(std::is_trivially_copyable<SynthPattern>::value,
                "Working Pattern must remain a fixed lossless value");
  static_assert(std::is_trivially_copyable<Buffer>::value,
                "Working Melody must remain a fixed bounded value");
  static_assert(sizeof(SynthPattern) <= sizeof(Buffer),
                "Pattern must fit inside the already-paid Melody footprint");
  static_assert(sizeof(WorkingMaterialStorage) <= sizeof(Buffer),
                "M-WORKING may not increase per-voice material storage");

  WorkingMaterialStorage storage;

  const SynthPattern pattern = makePattern();
  storage.storePattern(pattern);
  assert(std::memcmp(&storage.pattern(), &pattern, sizeof(pattern)) == 0);

  const Buffer melody = makeMelody();
  storage.storeMelody(melody);
  assert(std::memcmp(&storage.melody(), &melody, sizeof(melody)) == 0);

  std::printf("M-WORKING storage size: %zu <= %zu: PASS\n",
              sizeof(WorkingMaterialStorage), sizeof(Buffer));
  return 0;
}
