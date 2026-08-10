#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/roles/bass_rhythm.h"
#include "src/generation/roles/semantic_pattern_projector.h"

using namespace GroovePuterRhythm;

namespace {

BassRhythmRequest requestFor(BassRhythmId id) {
  BassRhythmRequest request{};
  request.requestedId = id;
  request.family = RhythmFamily::HipHopBackbeat;
  request.archetypeId = 416;
  request.kickOnsets = static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(9) | stepBit(12));
  request.generation.projectSeed = 0xBA559001u;
  request.generation.phraseOrdinal = 7;
  return request;
}

SynthPattern pitchSource(int transpose) {
  SynthPattern pattern{};
  pattern.steps[1].note = static_cast<int8_t>(36 + transpose);
  pattern.steps[1].velocity = 81;
  pattern.steps[5].note = static_cast<int8_t>(43 + transpose);
  pattern.steps[5].accent = true;
  pattern.steps[11].note = static_cast<int8_t>(46 + transpose);
  pattern.steps[11].ghost = true;
  return pattern;
}

StepMask noteMask(const SynthPattern& pattern) {
  StepMask result = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0 && !pattern.steps[step].slide) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

void testCatalogAndRelationshipSemantics() {
  StepMask fingerprints[static_cast<uint8_t>(BassRhythmId::Count)]{};
  for (uint8_t value = static_cast<uint8_t>(BassRhythmId::RootPulse);
       value < static_cast<uint8_t>(BassRhythmId::Count); ++value) {
    const BassRhythmId id = static_cast<BassRhythmId>(value);
    const BassRhythmResult result = realizeBassRhythm(requestFor(id));
    assert(result.status == BassRhythmStatus::Ok);
    assert(result.plan.id == id);
    assert(std::strcmp(bassRhythmName(id), "INVALID") != 0);
    fingerprints[value] = result.plan.onsets;
  }
  assert(fingerprints[static_cast<uint8_t>(BassRhythmId::KickLock)] ==
         requestFor(BassRhythmId::KickLock).kickOnsets);
  assert((fingerprints[static_cast<uint8_t>(BassRhythmId::KickAnswer)] &
          requestFor(BassRhythmId::KickAnswer).kickOnsets) == 0);
  assert((fingerprints[static_cast<uint8_t>(BassRhythmId::GapFill)] &
          requestFor(BassRhythmId::GapFill).kickOnsets) == 0);
  assert(fingerprints[static_cast<uint8_t>(BassRhythmId::RollingDrive)] !=
         fingerprints[static_cast<uint8_t>(BassRhythmId::SparseAnchor)]);
}

void testAutoIsDeterministicAndFamilyBounded() {
  for (uint8_t family = 0;
       family < static_cast<uint8_t>(RhythmFamily::Count); ++family) {
    BassRhythmRequest request = requestFor(BassRhythmId::Auto);
    request.family = static_cast<RhythmFamily>(family);
    for (uint16_t ordinal = 0; ordinal < 64; ++ordinal) {
      request.generation.phraseOrdinal = ordinal;
      const BassRhythmResult first = realizeBassRhythm(request);
      const BassRhythmResult second = realizeBassRhythm(request);
      assert(first.status == second.status);
      assert(std::memcmp(&first.plan, &second.plan, sizeof(first.plan)) == 0);
      assert(first.plan.id != BassRhythmId::Auto);
    }
  }
}

void testPitchIsIndependentFromBassRhythm() {
  const BassRhythmResult bass =
      realizeBassRhythm(requestFor(BassRhythmId::SyncopatedHook));
  assert(bass.status == BassRhythmStatus::Ok);
  SynthPattern low{};
  SynthPattern high{};
  assert(projectLegacyPitchPattern(pitchSource(0), bass.plan.onsets,
                                   bass.plan.continuations, low) ==
         SemanticPatternProjectStatus::Ok);
  assert(projectLegacyPitchPattern(pitchSource(12), bass.plan.onsets,
                                   bass.plan.continuations, high) ==
         SemanticPatternProjectStatus::Ok);
  assert(noteMask(low) == bass.plan.onsets);
  assert(noteMask(high) == bass.plan.onsets);
  assert(std::memcmp(&low, &high, sizeof(low)) != 0);
}

void testStage8FeelReachesBassRoleOnly() {
  const BassRhythmResult bass =
      realizeBassRhythm(requestFor(BassRhythmId::SyncopatedHook));
  SynthPattern straight{};
  SynthPattern laidBack{};
  assert(projectLegacyPitchPattern(pitchSource(0), bass.plan.onsets,
                                   bass.plan.continuations, straight) ==
         SemanticPatternProjectStatus::Ok);
  laidBack = straight;
  GenerationContext generation{};
  generation.projectSeed = 99;
  assert(applyFeelToSemanticPattern(
             RhythmRole::BassRhythm, bass.plan.onsets,
             FeelProfileId::Straight, 100, generation, straight) ==
         FeelInterpretStatus::Ok);
  assert(applyFeelToSemanticPattern(
             RhythmRole::BassRhythm, bass.plan.onsets,
             FeelProfileId::LaidBack, 100, generation, laidBack) ==
         FeelInterpretStatus::Ok);
  bool delayed = false;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if ((bass.plan.onsets & stepBit(step)) == 0) continue;
    assert(straight.steps[step].timing == 0);
    if (laidBack.steps[step].timing > 0) delayed = true;
  }
  assert(delayed);
}

void testSustainAndIntentionalEmptyBar() {
  BassRhythmRequest sustain = requestFor(BassRhythmId::SustainAndDrop);
  const BassRhythmResult held = realizeBassRhythm(sustain);
  assert(held.status == BassRhythmStatus::Ok);
  assert(held.plan.continuations != 0);
  SynthPattern projected{};
  assert(projectLegacyPitchPattern(pitchSource(0), held.plan.onsets,
                                   held.plan.continuations, projected) ==
         SemanticPatternProjectStatus::Ok);
  bool foundTie = false;
  for (const SynthStep& step : projected.steps) {
    if (step.note >= 0 && step.slide) foundTie = true;
  }
  assert(foundTie);

  sustain.allowEmptyBar = true;
  sustain.barOrdinal = 1;
  const BassRhythmResult empty = realizeBassRhythm(sustain);
  assert(empty.status == BassRhythmStatus::ValidButEmpty);
  assert(empty.plan.onsets == 0 && empty.plan.continuations == 0);
  SynthPattern cleared = pitchSource(0);
  assert(projectLegacyPitchPattern(pitchSource(0), 0, 0, cleared) ==
         SemanticPatternProjectStatus::Ok);
  assert(noteMask(cleared) == 0);
}

void testTransactionalProjectionFailure() {
  SynthPattern destination = pitchSource(7);
  const SynthPattern before = destination;
  const StepMask invalid = stepBit(0);
  assert(projectLegacyPitchPattern(SynthPattern{}, invalid, 0, destination) ==
         SemanticPatternProjectStatus::MissingPitchSource);
  assert(std::memcmp(&destination, &before, sizeof(destination)) == 0);
  assert(projectLegacyPitchPattern(pitchSource(0), invalid, invalid,
                                   destination) ==
         SemanticPatternProjectStatus::InvalidPlan);
  assert(std::memcmp(&destination, &before, sizeof(destination)) == 0);
}

}  // namespace

int main() {
  testCatalogAndRelationshipSemantics();
  testAutoIsDeterministicAndFamilyBounded();
  testPitchIsIndependentFromBassRhythm();
  testStage8FeelReachesBassRoleOnly();
  testSustainAndIntentionalEmptyBar();
  testTransactionalProjectionFailure();
  return 0;
}
