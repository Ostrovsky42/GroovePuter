#include "../src/dsp/atlas_runtime.h"
#include "../src/dsp/phrase_generator.h"

#include <cassert>
#include <cstring>

namespace {

int countSynthNotes(const SynthPattern& pattern) {
  int count = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0) ++count;
  }
  return count;
}

int countDrumHits(const DrumPatternSet& pattern, int startStep = 0) {
  int count = 0;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = startStep; step < DrumPattern::kSteps; ++step) {
      if (pattern.voices[voice].steps[step].hit) ++count;
    }
  }
  return count;
}

void validateRanges(const SynthPattern& pattern) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& value = pattern.steps[step];
    assert(value.note >= -1 && value.note <= 127);
    assert(value.velocity >= 1 && value.velocity <= 127);
    assert(value.timing >= -23 && value.timing <= 23);
    assert(value.probability <= 100);
  }
}

void validateRanges(const DrumPatternSet& pattern) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& value = pattern.voices[voice].steps[step];
      assert(value.velocity >= 1 && value.velocity <= 127);
      assert(value.timing >= -23 && value.timing <= 23);
      assert(value.probability <= 100);
    }
  }
}

struct RecipeExpectation {
  uint8_t id;
  const char* atlasId;
  uint16_t bpm;
  uint8_t swing;
  int minSynthA;
  int minSynthB;
  int minDrums;
};

constexpr RecipeExpectation kRecipes[] = {
    {6, "REC_ACID_CHICAGO_JACK", 124, 52, 8, 3, 15},
    {7, "REC_ACID_ROLLING", 128, 54, 8, 3, 15},
    {8, "REC_UKG_CLASSIC_2STEP", 134, 66, 3, 3, 16},
    {9, "REC_UKG_DARK_SKIPPY", 136, 68, 3, 3, 16},
    {10, "REC_DUB_DEEP_CHORD", 120, 54, 2, 3, 10},
    {11, "REC_DUB_MINIMAL_SPACE", 116, 51, 2, 3, 9},
};

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

void testPhrasePreflightAndRollback() {
  Scene occupied{};
  occupied.activeSongSlot = 1;
  occupied.songs[1].positions[5].patterns[
      static_cast<int>(SongTrack::SynthA)] = 4;

  PhraseGenerator::PhraseRequest request{};
  request.bars = 4;
  request.songStart = 4;
  request.pageIndex = 0;

  PhraseGenerator::PhraseResult result =
      PhraseGenerator::generateToSong(occupied, request, generateTestBase);
  assert(!result);
  assert(result.error == PhraseGenerator::PhraseError::SongRowsOccupied);
  assert(PhraseGenerator::localSlotIsEmpty(occupied, 0));

  Scene rollback{};
  request.songStart = 0;
  int generatedBars = 0;
  result = PhraseGenerator::generateBarsToSong(
      rollback, request,
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
    assert(PhraseGenerator::localSlotIsEmpty(rollback, slot));
    const SongPosition& position = rollback.songs[0].positions[slot];
    assert(position.patterns[static_cast<int>(SongTrack::SynthA)] == -1);
    assert(position.patterns[static_cast<int>(SongTrack::SynthB)] == -1);
    assert(position.patterns[static_cast<int>(SongTrack::Drums)] == -1);
  }
}

void testPhraseAllocatorProtectsNonDefaultSteps() {
  Scene scene{};
  scene.synthABanks[0].patterns[0].steps[0].note = 36;
  scene.drumBanks[0].patterns[1].voices[0].steps[0].hit = 1;
  assert(PhraseGenerator::findContiguousEmptySlots(scene, 4) == 2);

  Scene nonDefaultSteps{};
  nonDefaultSteps.synthABanks[0].patterns[0].steps[0].note = -2;  // TIE
  nonDefaultSteps.synthABanks[0].patterns[1].steps[0].fx =
      static_cast<uint8_t>(StepFx::Retrig);
  nonDefaultSteps.drumBanks[0].patterns[2].voices[0].steps[0].fx =
      static_cast<uint8_t>(StepFx::Roll);
  assert(PhraseGenerator::findContiguousEmptySlots(nonDefaultSteps, 1) == 3);

  Scene referenced{};
  referenced.activeSongSlot = 0;
  const int referencedPattern = songPatternFromPageBankIndex(3, 0, 0);
  referenced.songs[1].positions[12].patterns[
      static_cast<int>(SongTrack::SynthA)] = referencedPattern;
  assert(PhraseGenerator::localSlotIsEmpty(referenced, 0));
  assert(PhraseGenerator::globalPatternIsReferenced(
      referenced, referencedPattern));
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(referenced, 3, 0));
  assert(PhraseGenerator::findSafeContiguousEmptySlots(referenced, 3, 1) == 1);

  PhraseGenerator::PhraseRequest safeRequest{};
  safeRequest.bars = 1;
  safeRequest.songStart = 0;
  safeRequest.pageIndex = 3;
  const PhraseGenerator::PhraseResult safeResult =
      PhraseGenerator::generateToSong(referenced, safeRequest, generateTestBase);
  assert(safeResult);
  assert(safeResult.firstLocalSlot == 1);
  assert(referenced.songs[1].positions[12].patterns[
      static_cast<int>(SongTrack::SynthA)] == referencedPattern);

  assert(PhraseGenerator::roleForBar(4, 0) ==
         PhraseGenerator::PhraseBarRole::Base);
  assert(PhraseGenerator::roleForBar(4, 1) ==
         PhraseGenerator::PhraseBarRole::MicroVariation);
  assert(PhraseGenerator::roleForBar(4, 2) ==
         PhraseGenerator::PhraseBarRole::Return);
  assert(PhraseGenerator::roleForBar(4, 3) ==
         PhraseGenerator::PhraseBarRole::Fill);
}

}  // namespace

int main() {
  for (const RecipeExpectation& expected : kRecipes) {
    assert(AtlasRuntime::hasRecipe(expected.id));
    assert(AtlasRuntime::variationCount(expected.id) == 3);

    for (uint8_t variation = 0; variation < 3; ++variation) {
      SynthPattern synthA{};
      SynthPattern synthB{};
      DrumPatternSet drums{};
      AtlasRuntimeMetadata metadata{};

      assert(AtlasRuntime::applyRecipe(
          expected.id, variation, synthA, synthB, drums, &metadata));
      assert(metadata.atlasRecipeId != nullptr);
      assert(std::strcmp(metadata.atlasRecipeId, expected.atlasId) == 0);
      assert(metadata.slotId != nullptr);
      assert(metadata.bpm == expected.bpm);
      assert(metadata.swingPercent == expected.swing);
      assert(countSynthNotes(synthA) >= expected.minSynthA);
      assert(countSynthNotes(synthB) >= expected.minSynthB);
      assert(countDrumHits(drums) >= expected.minDrums);
      validateRanges(synthA);
      validateRanges(synthB);
      validateRanges(drums);
    }
  }

  SynthPattern sentinelA{};
  SynthPattern sentinelB{};
  DrumPatternSet sentinelDrums{};
  sentinelA.steps[0].note = 55;
  assert(!AtlasRuntime::applyRecipe(
      250, 0, sentinelA, sentinelB, sentinelDrums, nullptr));
  assert(sentinelA.steps[0].note == 55);

  testPhrasePlanningAndCommit();
  testPhrasePreflightAndRollback();
  testPhraseAllocatorProtectsNonDefaultSteps();
  return 0;
}