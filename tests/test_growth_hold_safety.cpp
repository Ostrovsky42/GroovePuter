#include <cassert>

#include "src/dsp/musical_development.h"

namespace {

PhraseRuntime::RuntimeSynthEventBuffer phrase(uint16_t bars, uint16_t duration) {
  PhraseRuntime::RuntimeSynthEventBuffer result{};
  result.lengthTicks = static_cast<uint16_t>(bars * PhraseRuntime::kTicksPerBar);
  result.count = 1;
  result.events[0].startTick = 0;
  result.events[0].durationSubticks = duration;
  result.events[0].note = 60;
  result.events[0].velocity = 100;
  result.events[0].probability = 100;
  return result;
}

void testRepeatTilesTheWholeSourcePhrase() {
  const auto source = phrase(2, 16);
  GroovePuterDevelopment::DevelopmentRequest request{};
  const auto result = GroovePuterDevelopment::growMaterial(
      source, 8, GroovePuterDevelopment::GrowthMode::Repeat, request);

  assert(result.success);
  assert(result.candidate.count == 4);
  assert(result.candidate.events[0].startTick == 0);
  assert(result.candidate.events[1].startTick == 2 * PhraseRuntime::kTicksPerBar);
  assert(result.candidate.events[2].startTick == 4 * PhraseRuntime::kTicksPerBar);
  assert(result.candidate.events[3].startTick == 6 * PhraseRuntime::kTicksPerBar);
}

void testHoldSaturatesAtPhraseBoundary() {
  auto source = phrase(8, 24576);
  source.count = 2;
  source.events[1] = source.events[0];
  source.events[1].note = 64;
  GroovePuterDevelopment::DevelopmentRequest request{};
  request.transformation = GroovePuterDevelopment::TransformationKind::Hold;
  const auto result = GroovePuterDevelopment::developCandidate(source, request);

  assert(result.success);
  assert(result.candidate.events[0].durationSubticks ==
         8 * PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick);
  assert(result.candidate.events[1].durationSubticks ==
         8 * PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick);
  assert(result.evidence.bass.durationDeltaSubticks == INT16_MAX);
}

}  // namespace

int main() {
  testRepeatTilesTheWholeSourcePhrase();
  testHoldSaturatesAtPhraseBoundary();
  return 0;
}
