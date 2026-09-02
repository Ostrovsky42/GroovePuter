#include "pattern_shadow_metrics.h"

namespace GroovePuterRhythm {
namespace {

void accountSlot(bool legacyOnset,
                 bool vocabularyOnset,
                 bool legacyAccent,
                 bool vocabularyAccent,
                 ShadowPatternMetrics& metrics) {
  if (legacyOnset) ++metrics.legacyOnsets;
  if (vocabularyOnset) ++metrics.vocabularyOnsets;
  if (legacyOnset && vocabularyOnset) ++metrics.sharedOnsets;
  if (legacyOnset != vocabularyOnset) ++metrics.differingSlots;

  if (legacyAccent) ++metrics.legacyAccents;
  if (vocabularyAccent) ++metrics.vocabularyAccents;
  if (legacyAccent && vocabularyAccent) ++metrics.sharedAccents;
}

void compareDrums(const DrumPatternSet& legacy,
                  const DrumPatternSet& vocabulary,
                  ShadowPatternMetrics& metrics) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& legacyStep = legacy.voices[voice].steps[step];
      const DrumStep& vocabularyStep = vocabulary.voices[voice].steps[step];
      accountSlot(legacyStep.hit,
                  vocabularyStep.hit,
                  legacyStep.hit && legacyStep.accent,
                  vocabularyStep.hit && vocabularyStep.accent,
                  metrics);
    }
  }
}

void compareSynth(const SynthPattern& legacy,
                  const SynthPattern& vocabulary,
                  ShadowPatternMetrics& metrics) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& legacyStep = legacy.steps[step];
    const SynthStep& vocabularyStep = vocabulary.steps[step];
    const bool legacyOnset = legacyStep.note >= 0;
    const bool vocabularyOnset = vocabularyStep.note >= 0;
    accountSlot(legacyOnset,
                vocabularyOnset,
                legacyOnset && legacyStep.accent,
                vocabularyOnset && vocabularyStep.accent,
                metrics);
  }
}

}  // namespace

ShadowPatternMetrics compareShadowPatterns(
    const DrumPatternSet& legacyDrums,
    const SynthPattern& legacySynthA,
    const SynthPattern& legacySynthB,
    const MaterializedPatterns& vocabulary,
    uint8_t targetFlags) {
  ShadowPatternMetrics metrics{};
  if ((targetFlags & ShadowDrums) != 0) {
    compareDrums(legacyDrums, vocabulary.drums, metrics);
  }
  if ((targetFlags & ShadowSynthA) != 0) {
    compareSynth(legacySynthA, vocabulary.synthA, metrics);
  }
  if ((targetFlags & ShadowSynthB) != 0) {
    compareSynth(legacySynthB, vocabulary.synthB, metrics);
  }
  return metrics;
}

}  // namespace GroovePuterRhythm
