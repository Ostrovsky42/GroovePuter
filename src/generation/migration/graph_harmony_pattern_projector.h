#ifndef GROOVEPUTER_GENERATION_MIGRATION_GRAPH_HARMONY_PATTERN_PROJECTOR_H
#define GROOVEPUTER_GENERATION_MIGRATION_GRAPH_HARMONY_PATTERN_PROJECTOR_H

#include "../../../scenes.h"
#include "../harmony/harmonic_role_policy.h"
#include "../harmony/harmonic_source.h"
#include "../roles/harmonic_rhythm.h"

namespace GroovePuterRhythm {

namespace GraphHarmonyPatternProjectorDetail {

inline uint8_t nearestMidiNote(uint8_t source,
                               GroovePuterHarmony::PitchSet12 allowed) {
  uint8_t best = source;
  uint8_t bestDistance = 128;
  for (uint16_t note = 0; note <= 127; ++note) {
    if ((allowed & static_cast<GroovePuterHarmony::PitchSet12>(
                       1u << (note % 12u))) == 0) continue;
    const uint8_t distance = static_cast<uint8_t>(
        note > source ? note - source : source - note);
    if (distance < bestDistance ||
        (distance == bestDistance && note < best)) {
      best = static_cast<uint8_t>(note);
      bestDistance = distance;
    }
  }
  return best;
}

inline uint8_t onsetCount(StepMask mask) {
  uint8_t count = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((mask & stepBit(step)) != 0) ++count;
  }
  return count;
}

}  // namespace GraphHarmonyPatternProjectorDetail

// Re-pitch only the already-authored note attacks against a prepared Graph
// route. ChordRhythm remains the WHEN owner. A failure leaves every Pattern
// field untouched.
inline GroovePuterHarmony::HarmonyStatus projectPatternToGraphHarmony(
    SynthPattern& pattern, const HarmonicRhythmPlan& rhythm,
    const GroovePuterHarmony::PreparedGraphHarmony& graph,
    uint16_t firstGlobalHarmonicOrdinal,
    GroovePuterHarmony::HarmonicRole role,
    GroovePuterHarmony::PitchSet12 forbidden = 0) {
  using GroovePuterHarmony::HarmonyStatus;
  if (graph.materialId == 0 || graph.publicationGeneration == 0 ||
      graph.modelVersion == 0 ||
      graph.authority != GroovePuterHarmony::HarmonicAuthority::Graph ||
      graph.eventCount == 0 || graph.eventCount > 32 ||
      rhythm.eventCount == 0 || rhythm.eventCount > kMaxHarmonicEvents ||
      GraphHarmonyPatternProjectorDetail::onsetCount(rhythm.onsets) !=
          rhythm.eventCount ||
      rhythm.phraseHarmonicPosition != firstGlobalHarmonicOrdinal) {
    return HarmonyStatus::Invalid;
  }

  const GroovePuterHarmony::ResolvedHarmonicEvent* events[kMaxHarmonicEvents]{};
  for (uint8_t i = 0; i < rhythm.eventCount; ++i) {
    const uint16_t ordinal =
        static_cast<uint16_t>(firstGlobalHarmonicOrdinal + i);
    if (ordinal >= graph.eventCount) return HarmonyStatus::Stale;
    events[i] = &graph.events[ordinal];
  }

  SynthPattern projected = pattern;
  uint8_t activeEvent = 0;
  uint8_t eventsStarted = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if ((rhythm.onsets & stepBit(step)) != 0) {
      if (eventsStarted >= rhythm.eventCount) return HarmonyStatus::Invalid;
      activeEvent = eventsStarted++;
    }
    const int8_t note = projected.steps[step].note;
    if (note < 0) continue;
    // A same-pitch slide continuation is one held voice. Keep its projected
    // pitch across a chord boundary instead of turning the tie into a new
    // attack or a discontinuous glide.
    if (step > 0 && pattern.steps[step].slide &&
        pattern.steps[step - 1].note == note) {
      projected.steps[step].note = projected.steps[step - 1].note;
      continue;
    }

    GroovePuterHarmony::RolePitchPolicy policy{};
    const auto status = GroovePuterHarmony::rolePitchPolicy(
        events[activeEvent]->state, events[activeEvent]->tonal, role,
        forbidden, policy);
    if (status != HarmonyStatus::Ready) return status;
    if (policy.allowed == 0) return HarmonyStatus::Forbidden;
    projected.steps[step].note = static_cast<int8_t>(
        GraphHarmonyPatternProjectorDetail::nearestMidiNote(
            static_cast<uint8_t>(note), policy.allowed));
  }
  if (eventsStarted != rhythm.eventCount) return HarmonyStatus::Invalid;
  pattern = projected;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_GRAPH_HARMONY_PATTERN_PROJECTOR_H
