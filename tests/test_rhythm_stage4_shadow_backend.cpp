#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/shadow/vocabulary_shadow_backend.h"

using namespace GroovePuterRhythm;

namespace {

bool sameDrumStep(const DrumStep& a, const DrumStep& b) {
  return a.hit == b.hit && a.accent == b.accent &&
         a.velocity == b.velocity && a.timing == b.timing &&
         a.fx == b.fx && a.fxParam == b.fxParam &&
         a.probability == b.probability;
}

bool sameDrums(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (!sameDrumStep(a.voices[voice].steps[step],
                        b.voices[voice].steps[step])) {
        return false;
      }
    }
  }
  return a.groove.swing == b.groove.swing &&
         a.groove.humanize == b.groove.humanize;
}

bool sameSynth(const SynthPattern& a, const SynthPattern& b) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& x = a.steps[step];
    const SynthStep& y = b.steps[step];
    if (x.note != y.note || x.slide != y.slide ||
        x.accent != y.accent || x.ghost != y.ghost ||
        x.velocity != y.velocity || x.timing != y.timing ||
        x.fx != y.fx || x.fxParam != y.fxParam ||
        x.probability != y.probability) {
      return false;
    }
  }
  return true;
}

bool sameMetrics(const ShadowPatternMetrics& a,
                 const ShadowPatternMetrics& b) {
  return a.legacyOnsets == b.legacyOnsets &&
         a.vocabularyOnsets == b.vocabularyOnsets &&
         a.sharedOnsets == b.sharedOnsets &&
         a.differingSlots == b.differingSlots &&
         a.legacyAccents == b.legacyAccents &&
         a.vocabularyAccents == b.vocabularyAccents &&
         a.sharedAccents == b.sharedAccents;
}

bool sameMaterializationDiagnostics(
    const PatternMaterializationDiagnostics& a,
    const PatternMaterializationDiagnostics& b) {
  return a.structuralEvents == b.structuralEvents &&
         a.secondaryEvents == b.secondaryEvents &&
         a.ghostEvents == b.ghostEvents &&
         a.accentEvents == b.accentEvents &&
         a.ignoredEvents == b.ignoredEvents &&
         a.deferredGateEvents == b.deferredGateEvents &&
         a.emittedRoles == b.emittedRoles &&
         a.ignoredRoles == b.ignoredRoles;
}

DrumPatternSet makeLegacyDrums() {
  DrumPatternSet drums{};
  drums.voices[KICK].steps[0].hit = true;
  drums.voices[KICK].steps[0].velocity = 118;
  drums.voices[SNARE].steps[4].hit = true;
  drums.voices[SNARE].steps[4].accent = true;
  drums.voices[CLOSED_HAT].steps[2].hit = true;
  drums.voices[OPEN_HAT].steps[6].hit = true;
  drums.groove.swing = -1.0f;
  drums.groove.humanize = -1.0f;
  return drums;
}

SynthPattern makeLegacySynth(int8_t root) {
  SynthPattern synth{};
  synth.steps[0].note = root;
  synth.steps[0].velocity = 103;
  synth.steps[8].note = static_cast<int8_t>(root + 7);
  synth.steps[8].accent = true;
  return synth;
}

void test_all_reference_archetypes_shadow_without_mutation() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::catalog();
  const DrumPatternSet legacyDrums = makeLegacyDrums();
  const SynthPattern legacyA = makeLegacySynth(36);
  const SynthPattern legacyB = makeLegacySynth(60);
  const DrumPatternSet legacyDrumsBefore = legacyDrums;
  const SynthPattern legacyABefore = legacyA;
  const SynthPattern legacyBBefore = legacyB;

  assert(ReferenceVocabulary::definitionCount() == 20);
  for (uint8_t index = 0; index < ReferenceVocabulary::definitionCount(); ++index) {
    const ReferenceVocabulary::Definition& definition =
        ReferenceVocabulary::definition(index);

    for (uint32_t seed = 1; seed <= 16; ++seed) {
      for (uint8_t levelIndex = 0;
           levelIndex < static_cast<uint8_t>(RealizationLevel::Count);
           ++levelIndex) {
        VocabularyShadowRequest request{};
        request.catalog = &catalog;
        request.archetypeId = definition.archetypeId;
        request.level = static_cast<RealizationLevel>(levelIndex);
        request.generation.projectSeed = seed;
        request.generation.phraseOrdinal = 3;

        const VocabularyShadowResult first = runVocabularyShadow(
            request, legacyDrums, legacyA, legacyB);
        const VocabularyShadowResult second = runVocabularyShadow(
            request, legacyDrums, legacyA, legacyB);

        assert(first.status == VocabularyShadowStatus::Ok);
        assert(first.realizationStatus != RealizationStatus::InvalidConstraintSet);
        assert(first.materializationStatus == PatternMaterializeStatus::Ok);
        assert(first.metrics.vocabularyOnsets > 0);
        assert(first.metrics.legacyOnsets == 4);
        assert(first.metrics.sharedOnsets <= first.metrics.legacyOnsets);
        assert(first.metrics.sharedOnsets <= first.metrics.vocabularyOnsets);
        assert(sameMetrics(first.metrics, second.metrics));
        assert(sameMaterializationDiagnostics(
            first.materialization, second.materialization));
      }
    }
  }

  assert(sameDrums(legacyDrums, legacyDrumsBefore));
  assert(sameSynth(legacyA, legacyABefore));
  assert(sameSynth(legacyB, legacyBBefore));
}

void test_invalid_request_fails_closed() {
  const DrumPatternSet legacyDrums = makeLegacyDrums();
  const SynthPattern legacyA = makeLegacySynth(36);
  const SynthPattern legacyB = makeLegacySynth(60);

  VocabularyShadowRequest request{};
  const VocabularyShadowResult missingCatalog = runVocabularyShadow(
      request, legacyDrums, legacyA, legacyB);
  assert(missingCatalog.status == VocabularyShadowStatus::InvalidRequest);

  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = ReferenceVocabulary::definition(0).archetypeId;
  request.compareTargets = 0;
  const VocabularyShadowResult missingTargets = runVocabularyShadow(
      request, legacyDrums, legacyA, legacyB);
  assert(missingTargets.status == VocabularyShadowStatus::InvalidRequest);
}

void test_shadow_can_measure_synth_without_claiming_synth_ownership() {
  const DrumPatternSet legacyDrums = makeLegacyDrums();
  const SynthPattern legacyA = makeLegacySynth(36);
  const SynthPattern legacyB = makeLegacySynth(60);

  VocabularyShadowRequest request{};
  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = ReferenceVocabulary::definition(0).archetypeId;
  request.generation.projectSeed = 7;
  request.compareTargets = static_cast<uint8_t>(
      ShadowDrums | ShadowSynthA | ShadowSynthB);

  const VocabularyShadowResult result = runVocabularyShadow(
      request, legacyDrums, legacyA, legacyB);
  assert(result.status == VocabularyShadowStatus::Ok);
  // Bass/Chord/Melodic are explicitly ignored in the default shadow request,
  // so legacy synth onsets are observed against empty Vocabulary synth output.
  assert(result.metrics.legacyOnsets >= 8);
  assert((result.materialization.ignoredRoles &
          rhythmRoleBit(RhythmRole::BassRhythm)) != 0 ||
         result.materialization.ignoredEvents == 0);
}

}  // namespace

int main() {
  test_all_reference_archetypes_shadow_without_mutation();
  test_invalid_request_fails_closed();
  test_shadow_can_measure_synth_without_claiming_synth_ownership();
  return 0;
}
