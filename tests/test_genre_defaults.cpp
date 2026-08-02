#include "../src/dsp/genre_manager.h"

#include <cassert>
#include <cmath>

namespace {

bool inUnitRange(float value) {
  return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

}  // namespace

int main() {
  const GenerativeParams params{};

  assert(params.minNotes >= 0);
  assert(params.maxNotes >= params.minNotes);
  assert(params.maxNotes <= 16);

  assert(params.minOctave >= 0);
  assert(params.maxOctave >= params.minOctave);
  assert(params.maxOctave <= 127);

  assert(inUnitRange(params.slideProbability));
  assert(inUnitRange(params.accentProbability));
  assert(std::isfinite(params.gateLengthMultiplier));
  assert(params.gateLengthMultiplier >= 0.1f);
  assert(params.gateLengthMultiplier <= 1.0f);

  assert(std::isfinite(params.swingAmount));
  assert(params.swingAmount >= 0.0f);
  assert(params.swingAmount <= 0.66f);
  assert(inUnitRange(params.microTimingAmount));

  assert(params.velocityMin >= 1);
  assert(params.velocityMax >= params.velocityMin);
  assert(params.velocityMax <= 127);

  assert(inUnitRange(params.rootNoteBias));
  assert(inUnitRange(params.ghostProbability));
  assert(inUnitRange(params.chromaticProbability));
  assert(inUnitRange(params.fillProbability));
  assert(inUnitRange(params.drumSyncopation));
  assert(params.drumVoiceCount >= 1);
  assert(params.drumVoiceCount <= 8);

  return 0;
}
