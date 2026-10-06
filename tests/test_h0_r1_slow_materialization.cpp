#include <cassert>
#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/generation/migration/phrase_execution.h"

using namespace GroovePuterRhythm;

namespace {

void setChordProgression(PreparedPhraseExecution& prepared) {
  prepared.selection.composition.progression = ProgressionId::PopCycle;
  // Keep bass attacks on offbeats so the chord's existing step-0 attack is
  // audible in this pitch-root acceptance fixture.
  prepared.selection.composition.bassRhythm = BassRhythmId::OffbeatPush;
  prepared.selection.composition.chordRhythm = ChordRhythmId::HalfBarChange;
  prepared.selection.composition.secondaryRole = CompositionSecondaryRole::Chord;
  prepared.selection.composition.harmonicRhythmPolicy =
      PhraseHarmonicPolicyId::Slow;
  prepared.progressionSource = ChordProgressionSource{};
  prepared.progressionSource.id = ProgressionId::PopCycle;
  prepared.progressionSource.period = 4;
  prepared.progressionSource.events[0] = {0, ChordQuality::Triad, 0};
  prepared.progressionSource.events[1] = {4, ChordQuality::Triad, 0};
  prepared.progressionSource.events[2] = {5, ChordQuality::Triad, 0};
  prepared.progressionSource.events[3] = {3, ChordQuality::Triad, 0};
}

int8_t materializeBarStartRoot(PreparedPhraseExecution& prepared, uint8_t bar,
                        uint8_t expectedPitchClass) {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  const StrongRhythmMigrationResult result = materializePreparedPhraseBar(
      prepared, bar, bar, drums, synthA, synthB);
  assert(result.status == StrongRhythmMigrationStatus::Applied);
  const int8_t note = synthB.steps[0].note;
  assert(note >= 0);
  assert(static_cast<uint8_t>(note % 12) == expectedPitchClass);
  return note;
}

void testSlowFourBarProgressionProducesExpectedNoteRoots() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::House);
  settings.recipe = kBaseRecipeId;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;

  PhraseExecutionMaterializationSettings materialization{};
  materialization.level = RealizationLevel::P2Variation;
  materialization.feelProfile = FeelProfileId::Straight;
  materialization.tonalMaterializationEnabled = true;
  materialization.rootPitchClass = 0;
  materialization.scaleTypeValue = kScaleMajor;

  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution prepared{};
  assert(preparePhraseExecution(settings, materialization, 0x51A0u, 4,
                                scratch, prepared) ==
         PhraseExecutionStatus::Ready);
  const auto slow = projectPhraseHarmonicClock(
      4, ProgressionId::PopCycle, PhraseHarmonicPolicyId::Slow);
  assert(slow.status == PhraseHarmonicClockProjectionStatus::Ok);
  prepared.harmonicTimeline = slow.timeline;
  setChordProgression(prepared);

  constexpr uint16_t barTicks = 384;
  constexpr uint16_t expectedTicks[] = {0, 384, 768, 1152, 1536};
  constexpr uint8_t expectedRoots[] = {0, 7, 9, 5, 0};
  for (uint8_t event = 0; event < 5; ++event) {
    const uint8_t bar = static_cast<uint8_t>(event % 4u);
    assert(expectedTicks[event] == event * barTicks);
    const int8_t note = materializeBarStartRoot(
        prepared, bar, expectedRoots[event]);
    // The produced Synth B note at local step zero is the physical event at
    // this bar's absolute tick in the looping phrase.
    const uint16_t producedTick = static_cast<uint16_t>(
        (event / 4u) * 4u * barTicks + bar * barTicks);
    assert(producedTick == expectedTicks[event]);
    assert(static_cast<uint8_t>(note % 12) == expectedRoots[event]);
  }
  std::puts("H0-R1 SLOW produced-note roots C/G/A/F/C: PASS");
}

}  // namespace

int main() {
  testSlowFourBarProgressionProducesExpectedNoteRoots();
  return 0;
}
