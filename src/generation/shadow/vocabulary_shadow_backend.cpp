#include "vocabulary_shadow_backend.h"

namespace GroovePuterRhythm {
namespace {

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

bool validCompareTargets(uint8_t targets) {
  return targets != 0 && (targets & static_cast<uint8_t>(~kAllShadowTargets)) == 0;
}

}  // namespace

VocabularyShadowResult runVocabularyShadow(
    const VocabularyShadowRequest& request,
    const DrumPatternSet& legacyDrums,
    const SynthPattern& legacySynthA,
    const SynthPattern& legacySynthB) {
  VocabularyShadowResult result{};
  if (request.catalog == nullptr ||
      request.archetypeId == kNoArchetypeId ||
      !validLevel(request.level) ||
      !validCompareTargets(request.compareTargets)) {
    return result;
  }

  RhythmRealizationRequest p1Request{};
  p1Request.catalog = request.catalog;
  p1Request.archetypeId = request.archetypeId;
  p1Request.phraseBars = 1;
  p1Request.level = RealizationLevel::P1Canonical;
  p1Request.generation = request.generation;

  const RhythmRealizationResult p1 = realizeRhythmPhrase(p1Request);
  result.realizationStatus = p1.status;
  if (p1.status == RealizationStatus::InvalidConstraintSet) {
    result.status = VocabularyShadowStatus::RealizationFailed;
    return result;
  }

  RhythmRealizationResult realized = p1;
  if (request.level != RealizationLevel::P1Canonical) {
    RhythmRealizationRequest levelRequest = p1Request;
    levelRequest.level = request.level;
    levelRequest.reuseIdentity = &p1.identity;
    realized = realizeRhythmPhrase(levelRequest);
    result.realizationStatus = realized.status;
    if (realized.status == RealizationStatus::InvalidConstraintSet) {
      result.status = VocabularyShadowStatus::RealizationFailed;
      return result;
    }
  }

  PatternMaterializerBinding binding =
      standardDrumPatternBinding(request.ignoredRoles);
  MaterializedPatterns candidate{};
  result.materializationStatus = materializeRhythmPattern(
      realized.plan, binding, candidate, &result.materialization);
  if (result.materializationStatus != PatternMaterializeStatus::Ok) {
    result.status = VocabularyShadowStatus::MaterializationFailed;
    return result;
  }

  result.metrics = compareShadowPatterns(
      legacyDrums, legacySynthA, legacySynthB,
      candidate, request.compareTargets);
  result.status = VocabularyShadowStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
