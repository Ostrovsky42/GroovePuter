#include "../src/dsp/genre_manager.h"
#include "../src/dsp/phrase_generator.h"

#include <cassert>
#include <cmath>

namespace {

bool inUnitRange(float value) {
  return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

int countSynthNotes(const SynthPattern& pattern) {
  int count = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0) ++count;
  }
  return count;
}

int countDrumHits(const DrumPatternSet& drums, int startStep = 0) {
  int count = 0;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = startStep; step < DrumPattern::kSteps; ++step) {
      if (drums.voices[voice].steps[step].hit) ++count;
    }
  }
  return count;
}

void generateTestBase(PhraseGenerator::PhraseBar& bar) {
  bar.synthA.steps[0].note = 36;
  bar.synthA.steps[4].note = 38;
  bar.synthA.steps[8].note = 41;
  bar.synthA.steps[12].note = 43;
  bar.synthB.steps[2].note = 48;
  bar.synthB.steps[6].note = 50;
  bar.synthB.steps[10].note = 53;
  bar.synthB.steps[14].note = 55;

  bar.drums.voices[0].steps[0].hit = 1;
  bar.drums.voices[0].steps[8].hit = 1;
  bar.drums.voices[1].steps[4].hit = 1;
  bar.drums.voices[1].steps[12].hit = 1;
  for (int step = 0; step < DrumPattern::kSteps; step += 2) {
    bar.drums.voices[2].steps[step].hit = 1;
  }
}

void testPhrasePlanningAndCommit() {
  Scene scene{};
  scene.activeSongSlot = 0;
  scene.feel.patternBars = 4;

  PhraseGenerator::PhraseRequest request{};
  request.bars = 4;
  request.songStart = 3;
  request.pageIndex = 2;
  request.seed = 12345;

  const PhraseGenerator::PhraseResult result =
      PhraseGenerator::generateToSong(scene, request, generateTestBase);

  assert(result);
  assert(result.firstLocalSlot == 0);
  assert(result.songStart == 3);
  assert(result.bars == 4);
  assert(scene.feel.patternBars == 1);
  assert(scene.songs[0].length == 7);

  for (int bar = 0; bar < 4; ++bar) {
    const int expectedGlobal = songPatternFromPageBankIndex(2, 0, bar);
    const SongPosition& position = scene.songs[0].positions[3 + bar];
    assert(position.patterns[static_cast<int>(SongTrack::SynthA)] == expectedGlobal);
    assert(position.patterns[static_cast<int>(SongTrack::SynthB)] == expectedGlobal);
    assert(position.patterns[static_cast<int>(SongTrack::Drums)] == expectedGlobal);
  }

  const SynthPattern& baseA = scene.synthABanks[0].patterns[0];
  const SynthPattern& variationA = scene.synthABanks[0].patterns[1];
  const SynthPattern& returnA = scene.synthABanks[0].patterns[2];
  const DrumPatternSet& baseDrums = scene.drumBanks[0].patterns[0];
  const DrumPatternSet& fillDrums = scene.drumBanks[0].patterns[3];

  assert(countSynthNotes(baseA) == 4);
  assert(countSynthNotes(variationA) == countSynthNotes(baseA));
  assert(countSynthNotes(returnA) == countSynthNotes(baseA));
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    assert(returnA.steps[step].note == baseA.steps[step].note);
  }
  assert(countDrumHits(fillDrums, 12) > countDrumHits(baseDrums, 12));
}

void testPhrasePreflightDoesNotPartiallyWrite() {
  Scene scene{};
  scene.activeSongSlot = 1;
  scene.songs[1].positions[5].patterns[static_cast<int>(SongTrack::SynthA)] = 4;

  PhraseGenerator::PhraseRequest request{};
  request.bars = 4;
  request.songStart = 4;
  request.pageIndex = 0;

  const PhraseGenerator::PhraseResult result =
      PhraseGenerator::generateToSong(scene, request, generateTestBase);

  assert(!result);
  assert(result.error == PhraseGenerator::PhraseError::SongRowsOccupied);
  assert(PhraseGenerator::localSlotIsEmpty(scene, 0));
  assert(scene.songs[1].positions[4].patterns[static_cast<int>(SongTrack::SynthA)] == -1);
}

void testPhraseAllocatorSkipsUsedSlots() {
  Scene scene{};
  scene.synthABanks[0].patterns[0].steps[0].note = 36;
  scene.drumBanks[0].patterns[1].voices[0].steps[0].hit = 1;

  assert(PhraseGenerator::findContiguousEmptySlots(scene, 4) == 2);
  assert(PhraseGenerator::roleForBar(4, 0) == PhraseGenerator::PhraseBarRole::Base);
  assert(PhraseGenerator::roleForBar(4, 1) == PhraseGenerator::PhraseBarRole::MicroVariation);
  assert(PhraseGenerator::roleForBar(4, 2) == PhraseGenerator::PhraseBarRole::Return);
  assert(PhraseGenerator::roleForBar(4, 3) == PhraseGenerator::PhraseBarRole::Fill);
}

void testPhraseGenerationFailureRollsBack() {
  Scene scene{};
  PhraseGenerator::PhraseRequest request{};
  request.bars = 4;
  request.songStart = 0;
  request.pageIndex = 0;

  int generatedBars = 0;
  const PhraseGenerator::PhraseResult result =
      PhraseGenerator::generateBarsToSong(
          scene, request,
          [&](PhraseGenerator::PhraseBar& bar,
              PhraseGenerator::PhraseBarRole role,
              int barIndex) {
            (void)role;
            if (barIndex == 2) return false;
            generateTestBase(bar);
            ++generatedBars;
            return true;
          });

  assert(!result);
  assert(result.error == PhraseGenerator::PhraseError::GenerationFailed);
  assert(generatedBars == 2);
  for (int slot = 0; slot < 4; ++slot) {
    assert(PhraseGenerator::localSlotIsEmpty(scene, slot));
    const SongPosition& position = scene.songs[0].positions[slot];
    assert(position.patterns[static_cast<int>(SongTrack::SynthA)] == -1);
    assert(position.patterns[static_cast<int>(SongTrack::SynthB)] == -1);
    assert(position.patterns[static_cast<int>(SongTrack::Drums)] == -1);
  }
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

  testPhrasePlanningAndCommit();
  testPhrasePreflightDoesNotPartiallyWrite();
  testPhraseAllocatorSkipsUsedSlots();
  testPhraseGenerationFailureRollsBack();
  return 0;
}
