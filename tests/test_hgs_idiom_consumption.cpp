#include <cassert>
#include <cstdint>

#include "src/generation/migration/graph_harmony_pattern_projector.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"

using namespace GroovePuterHarmony;
using namespace GroovePuterRhythm;

namespace {
ResolvedHarmonicEvent makeEvent(QualityId qualityId, uint8_t rootPc,
                                uint8_t timingSlot) {
  const auto* quality = qualityDefinition(qualityId);
  assert(quality != nullptr);
  const auto* scale = scaleDefinition(0);
  assert(scale != nullptr);
  ResolvedHarmonicEvent event{};
  event.state = {qualityId, transposePitchSet(quality->intervalMask, rootPc),
                 rootPc, kNoBassPc};
  event.tonal = {0, 0, scale->intervalMask, scale->intervalMask, 0, 0};
  event.timingSlotOrdinal = timingSlot;
  return event;
}
}  // namespace

int main() {
  PreparedGraphHarmony graph{};
  graph.materialId = 1;
  graph.publicationGeneration = 1;
  graph.modelVersion = 1;
  graph.authority = HarmonicAuthority::Graph;
  graph.eventCount = 2;
  graph.events[0] = makeEvent(kQualityDominant7, 7, 4);
  graph.events[1] = makeEvent(kQualityMinor, 5, 5);

  HarmonicRhythmPlan rhythm{};
  rhythm.onsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
  rhythm.eventCount = 2;
  rhythm.phraseHarmonicPosition = 4;
  // The event vector is phrase-global. This bar begins at entries 4 and 5.
  graph.eventCount = 6;
  graph.events[4] = makeEvent(kQualityDominant7, 7, 8);
  graph.events[5] = makeEvent(kQualityMinor, 5, 10);

  SynthPattern bass{};
  bass.steps[2].note = 60;  // C moves to the nearest G7 tone, B.
  bass.steps[2].velocity = 91;
  bass.steps[2].accent = 1;
  bass.steps[7].note = 60;
  bass.steps[8].note = 60;  // Tie crosses into the next chord; retain B.
  bass.steps[8].slide = 1;
  bass.steps[10].note = 64;  // E moves to the nearest F-minor chord tone, F.
  bass.steps[10].slide = 1;
  bass.steps[10].timing = -2;
  const SynthPattern originalBass = bass;
  assert(projectPatternToGraphHarmony(bass, rhythm, graph, 4,
                                      HarmonicRole::Bass) == HarmonyStatus::Ready);
  assert(bass.steps[2].note == 59);
  assert(bass.steps[7].note == 59 && bass.steps[8].note == 59);
  assert(bass.steps[10].note == 65);
  assert(bass.steps[2].velocity == originalBass.steps[2].velocity);
  assert(bass.steps[2].accent == originalBass.steps[2].accent);
  assert(bass.steps[10].slide == originalBass.steps[10].slide);
  assert(bass.steps[10].timing == originalBass.steps[10].timing);
  assert(bass.steps[0].note == originalBass.steps[0].note);

  SynthPattern unchanged = originalBass;
  graph.eventCount = 5;
  assert(projectPatternToGraphHarmony(unchanged, rhythm, graph, 4,
                                      HarmonicRole::Bass) == HarmonyStatus::Stale);
  assert(unchanged.steps[2].note == originalBass.steps[2].note);
  assert(unchanged.steps[10].note == originalBass.steps[10].note);
}
