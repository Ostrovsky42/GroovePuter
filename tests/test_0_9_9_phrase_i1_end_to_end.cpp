#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/dsp/generated_phrase_p1r_materializer.h"

using namespace GroovePuterRhythm;

namespace {

GenreSettings loFi() {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  value.recipe = kBaseRecipeId;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  value.rhythmArchetypeId = kNoArchetypeId;
  return value;
}

PhraseExecutionMaterializationSettings materialization() {
  PhraseExecutionMaterializationSettings value{};
  value.level = RealizationLevel::P2Variation;
  value.generationAttemptOrdinal = 0;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  value.tonalMaterializationEnabled = true;
  value.rootPitchClass = 0;
  value.scaleTypeValue = kScaleDorian;
  return value;
}

PhraseGenerator::PhraseBar pitchSource() {
  PhraseGenerator::PhraseBar value{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    value.synthA.steps[step].note = static_cast<int8_t>(36 + step % 12);
    value.synthB.steps[step].note = static_cast<int8_t>(60 + step % 12);
    value.synthA.steps[step].velocity = 100;
    value.synthB.steps[step].velocity = 100;
    value.synthA.steps[step].probability = 100;
    value.synthB.steps[step].probability = 100;
  }
  return value;
}

bool sameSynth(const SynthPattern& left, const SynthPattern& right) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& a = left.steps[step];
    const SynthStep& b = right.steps[step];
    if (a.note != b.note || a.slide != b.slide || a.accent != b.accent ||
        a.ghost != b.ghost || a.velocity != b.velocity ||
        a.timing != b.timing || a.fx != b.fx || a.fxParam != b.fxParam ||
        a.probability != b.probability) return false;
  }
  return true;
}

bool sameDrums(const DrumPatternSet& left, const DrumPatternSet& right) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& a = left.voices[voice].steps[step];
      const DrumStep& b = right.voices[voice].steps[step];
      if (a.hit != b.hit || a.accent != b.accent ||
          a.velocity != b.velocity || a.timing != b.timing ||
          a.fx != b.fx || a.fxParam != b.fxParam ||
          a.probability != b.probability) return false;
    }
  }
  return left.groove.swing == right.groove.swing &&
         left.groove.humanize == right.groove.humanize;
}

bool sameBar(const PhraseGenerator::PhraseBar& left,
             const PhraseGenerator::PhraseBar& right) {
  return sameSynth(left.synthA, right.synthA) &&
         sameSynth(left.synthB, right.synthB) &&
         sameDrums(left.drums, right.drums);
}

void testLogicalIdentityDomain() {
  static_assert(GeneratedPhraseP1R::kLogicalPhraseAttemptChannel == 0xFFFF,
                "I1 logical attempt channel drift");
  assert(GeneratedPhraseP1R::phraseIdentityForAttempt(0) == 0);
  assert(GeneratedPhraseP1R::phraseIdentityForAttempt(65534) == 65534);
  assert(GeneratedPhraseP1R::phraseIdentityForAttempt(65535) == 0);
  for (uint32_t ordinal : {0u, 1u, 17u, 65534u, 65535u, 131071u}) {
    assert(GeneratedPhraseP1R::phraseIdentityForAttempt(ordinal) !=
           kUnspecifiedPhraseGenerationIdentity);
  }
  std::puts("I1 logical identity channel: OK");
}

void testPhysicalDestinationInvariance() {
  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution execution{};
  const PhraseExecutionStatus status = preparePhraseExecution(
      loFi(), materialization(), 37, 8, scratch, execution);
  assert(status == PhraseExecutionStatus::Ready);
  assert(execution.semantic.status == PhraseSemanticContractStatus::Ready);
  assert(execution.length.effectivePhraseBars == 8);

  std::array<PhraseGenerator::PhraseBar, 8> first{};
  std::array<PhraseGenerator::PhraseBar, 8> relocated{};
  GeneratedPhraseP1R::PreparationEvidence a{};
  GeneratedPhraseP1R::PreparationEvidence b{};
  const PhraseGenerator::PhraseBar seed = pitchSource();

  assert(GeneratedPhraseP1R::materializePreparedBars(
      execution, seed, 0, 0, first, a));
  assert(GeneratedPhraseP1R::materializePreparedBars(
      execution, seed, 0, 8, relocated, b));
  assert(a.materializationStatus == StrongRhythmMigrationStatus::Applied);
  assert(b.materializationStatus == StrongRhythmMigrationStatus::Applied);
  for (int bar = 0; bar < 8; ++bar) assert(sameBar(first[bar], relocated[bar]));

  assert(execution.semantic.harmonicTimeline.totalEventPositions > 0);
  assert(execution.semantic.harmonicTimeline.totalEventPositions <= 16);
  assert(execution.progressionSource.period > 0);
  std::printf(
      "I1 destination invariance: 8 bars harmonic_positions=%u progression_period=%u\n",
      static_cast<unsigned>(execution.semantic.harmonicTimeline.totalEventPositions),
      static_cast<unsigned>(execution.progressionSource.period));
}

void testTypedRejectRemainsVisible() {
  GenreSettings rave{};
  rave.generativeMode = static_cast<uint8_t>(GenerativeMode::Rave);
  rave.recipe = kBaseRecipeId;
  rave.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  rave.rhythmArchetypeId = kNoArchetypeId;

  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution execution{};
  const PhraseExecutionStatus status = preparePhraseExecution(
      rave, materialization(), 5, 8, scratch, execution);
  assert(status == PhraseExecutionStatus::Rejected);
  assert(execution.length.status == PhraseLengthRequestStatus::Rejected);
  assert(execution.length.rejectReason ==
         PhraseLengthRejectReason::NoAdmissibleLawForRequestedLength);
  std::puts("I1 typed 8-bar reject remains fail-closed: OK");
}

}  // namespace

int main() {
  testLogicalIdentityDomain();
  testPhysicalDestinationInvariance();
  testTypedRejectRemainsVisible();
  std::puts("0.9.9-PHRASE-I1 semantic-to-physical integration: OK");
  return 0;
}
