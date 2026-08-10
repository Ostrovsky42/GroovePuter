#include <cassert>
#include <cstdint>
#include <cstring>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterRhythm;

namespace {

static_assert(static_cast<uint8_t>(ProgressionId::Auto) == 0);
static_assert(static_cast<uint8_t>(ProgressionId::StaticModal) == 1);
static_assert(static_cast<uint8_t>(ProgressionId::PedalDrone) == 2);
static_assert(static_cast<uint8_t>(ProgressionId::PopCycle) == 3);
static_assert(static_cast<uint8_t>(ProgressionId::TwoFiveOne) == 4);
static_assert(static_cast<uint8_t>(ProgressionId::ParallelShift) == 5);
static_assert(static_cast<uint8_t>(ProgressionId::MinorFall) == 6);
static_assert(static_cast<uint8_t>(ProgressionId::BorrowedLift) == 7);
static_assert(static_cast<uint8_t>(ProgressionId::Count) == 8);
static_assert(sizeof(ChordProgressionPlan) == 26);
static_assert(sizeof(GenerationCompositionResult) == 26);

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe = 0) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

GenerationContext contextFor(uint8_t mode, uint8_t recipe, uint16_t ordinal) {
  GenerationContext generation{};
  generation.projectSeed =
      0x15000000u | (static_cast<uint32_t>(mode) << 8u) | recipe;
  generation.phraseOrdinal = ordinal;
  return generation;
}

bool contains(WeightedIdentityView view, ProgressionId id) {
  const uint8_t expected = static_cast<uint8_t>(id);
  for (uint8_t index = 0; index < view.count; ++index) {
    if (view.candidates[index].id == expected &&
        view.candidates[index].weight != 0) {
      return true;
    }
  }
  return false;
}

bool isExactProfile(GenerativeMode mode, uint8_t recipe) {
  const GenerationProfileView profile =
      generationProfileFor(settingsFor(mode, recipe));
  return profile.progressions.candidates != nullptr && profile.recipe == recipe;
}

ChordProgressionResult realize(ProgressionId id,
                               RhythmFamily family,
                               uint8_t eventCount,
                               uint8_t phraseBars,
                               const GenerationContext& generation) {
  ChordProgressionRequest request{};
  request.requestedId = id;
  request.family = family;
  request.generation = generation;
  request.harmonicEventCount = eventCount;
  request.phraseBars = phraseBars;
  return realizeChordProgression(request);
}

void assertEventBounds(const ChordProgressionResult& result) {
  assert(result.plan.eventCount <= kMaxHarmonicEvents);
  for (uint8_t index = 0; index < result.plan.eventCount; ++index) {
    const HarmonicEvent& event = result.plan.events[index];
    assert(event.degree <= 6);
    assert(static_cast<uint8_t>(event.quality) <
           static_cast<uint8_t>(ChordQuality::Count));
    assert(event.rootOffsetSemitones >= -kMaxRootOffsetSemitones);
    assert(event.rootOffsetSemitones <= kMaxRootOffsetSemitones);
    if (event.rootOffsetSemitones != 0) {
      assert(result.plan.id == ProgressionId::ParallelShift ||
             result.plan.id == ProgressionId::BorrowedLift);
    }
  }
}

void testEveryConcreteProgressionIsProfileReachable() {
  bool reached[static_cast<uint8_t>(ProgressionId::Count)]{};
  uint16_t exactProfiles = 0;
  for (uint8_t modeValue = 0; modeValue < kGenerativeModeCount; ++modeValue) {
    const GenerativeMode mode = static_cast<GenerativeMode>(modeValue);
    for (uint8_t recipe = 0; recipe <= kDustyJazzRecipeId; ++recipe) {
      if (!isExactProfile(mode, recipe)) continue;
      ++exactProfiles;
      const GenerationProfileView profile =
          generationProfileFor(settingsFor(mode, recipe));
      assert(isValidGenerationProfile(profile));
      for (uint8_t index = 0; index < profile.progressions.count; ++index) {
        const uint8_t id = profile.progressions.candidates[index].id;
        assert(id > static_cast<uint8_t>(ProgressionId::Auto));
        assert(id < static_cast<uint8_t>(ProgressionId::Count));
        reached[id] = true;
      }
    }
  }
  assert(exactProfiles == 33);
  for (uint8_t id = static_cast<uint8_t>(ProgressionId::StaticModal);
       id < static_cast<uint8_t>(ProgressionId::Count); ++id) {
    assert(reached[id]);
  }
}

void testDataOnlyGenreReuseCriterion() {
  const GenerationProfileView acid =
      generationProfileFor(settingsFor(GenerativeMode::Acid));
  const GenerationProfileView techno =
      generationProfileFor(settingsFor(GenerativeMode::Techno));
  const GenerationProfileView rave =
      generationProfileFor(settingsFor(GenerativeMode::Rave));
  assert(contains(acid.progressions, ProgressionId::StaticModal));
  assert(contains(techno.progressions, ProgressionId::StaticModal));
  assert(contains(rave.progressions, ProgressionId::StaticModal));

  const GenerationProfileView loFi =
      generationProfileFor(settingsFor(GenerativeMode::LoFi));
  const GenerationProfileView tripHop =
      generationProfileFor(settingsFor(GenerativeMode::TripHop));
  const GenerationProfileView funkSoul =
      generationProfileFor(settingsFor(GenerativeMode::FunkSoul));
  assert(contains(loFi.progressions, ProgressionId::TwoFiveOne));
  assert(contains(tripHop.progressions, ProgressionId::TwoFiveOne));
  assert(contains(funkSoul.progressions, ProgressionId::TwoFiveOne));

  const GenerationProfileView house =
      generationProfileFor(settingsFor(GenerativeMode::House));
  const GenerationProfileView outrun =
      generationProfileFor(settingsFor(GenerativeMode::Outrun));
  const GenerationProfileView loFiHouse = generationProfileFor(
      settingsFor(GenerativeMode::LoFi, kLoFiHouseRecipeId));
  assert(contains(house.progressions, ProgressionId::PopCycle));
  assert(contains(outrun.progressions, ProgressionId::PopCycle));
  assert(contains(loFiHouse.progressions, ProgressionId::PopCycle));
}

void testDeterminismAcrossEveryProfilePair() {
  for (uint8_t modeValue = 0; modeValue < kGenerativeModeCount; ++modeValue) {
    const GenerativeMode mode = static_cast<GenerativeMode>(modeValue);
    for (uint8_t recipe = 0; recipe <= kDustyJazzRecipeId; ++recipe) {
      if (!isExactProfile(mode, recipe)) continue;
      const GenerationProfileView profile =
          generationProfileFor(settingsFor(mode, recipe));
      for (uint8_t candidate = 0; candidate < profile.progressions.count;
           ++candidate) {
        const ProgressionId id = static_cast<ProgressionId>(
            profile.progressions.candidates[candidate].id);
        for (uint16_t ordinal = 0; ordinal < 64; ++ordinal) {
          const GenerationContext generation =
              contextFor(modeValue, recipe, ordinal);
          const ChordProgressionResult first =
              realize(id, RhythmFamily::FourFloor, 8, 4, generation);
          const ChordProgressionResult repeat =
              realize(id, RhythmFamily::FourFloor, 8, 4, generation);
          assert(first.status == repeat.status);
          assert(std::memcmp(&first.plan, &repeat.plan,
                             sizeof(ChordProgressionPlan)) == 0);
        }
      }
    }
  }
}

void testBoundsForEveryProgressionAndAllowedRequestSize() {
  constexpr uint8_t phraseBars[] = {1, 2, 4, 8};
  for (uint8_t idValue = static_cast<uint8_t>(ProgressionId::StaticModal);
       idValue < static_cast<uint8_t>(ProgressionId::Count); ++idValue) {
    const ProgressionId id = static_cast<ProgressionId>(idValue);
    for (uint8_t bars : phraseBars) {
      for (uint8_t count = 0; count <= kMaxHarmonicEvents; ++count) {
        for (uint8_t familyValue = 0;
             familyValue < static_cast<uint8_t>(RhythmFamily::Count);
             ++familyValue) {
          const ChordProgressionResult result = realize(
              id, static_cast<RhythmFamily>(familyValue), count, bars,
              contextFor(idValue, familyValue, count));
          assert(result.status == ChordProgressionStatus::Ok ||
                 result.status == ChordProgressionStatus::ValidButStatic);
          assert(result.plan.id == id);
          assertEventBounds(result);
          if (count == 0) {
            assert(result.plan.eventCount == 0);
          } else if (id == ProgressionId::StaticModal ||
                     id == ProgressionId::PedalDrone) {
            assert(result.status == ChordProgressionStatus::ValidButStatic);
            assert(result.plan.eventCount == 1);
          } else {
            assert(result.status == ChordProgressionStatus::Ok);
            assert(result.plan.eventCount == count);
          }
        }
      }
    }
  }
}

void testGrammarPrefixAndCycleContract() {
  const GenerationContext generation = contextFor(3, 9, 17);
  constexpr ProgressionId ids[] = {
      ProgressionId::PopCycle,
      ProgressionId::TwoFiveOne,
      ProgressionId::ParallelShift,
      ProgressionId::MinorFall,
      ProgressionId::BorrowedLift,
  };
  for (ProgressionId id : ids) {
    const ChordProgressionResult full =
        realize(id, RhythmFamily::FourFloor, 8, 4, generation);
    assert(full.status == ChordProgressionStatus::Ok);
    for (uint8_t count = 1; count <= kMaxHarmonicEvents; ++count) {
      const ChordProgressionResult prefix =
          realize(id, RhythmFamily::FourFloor, count, 4, generation);
      assert(prefix.status == ChordProgressionStatus::Ok);
      assert(prefix.plan.eventCount == count);
      assert(std::memcmp(prefix.plan.events, full.plan.events,
                         static_cast<size_t>(count) * sizeof(HarmonicEvent)) == 0);
    }
  }
}

void testInvalidRequestsAreRejected() {
  const GenerationContext generation = contextFor(0, 0, 0);

  ChordProgressionRequest request{};
  request.requestedId = ProgressionId::Count;
  request.family = RhythmFamily::FourFloor;
  request.generation = generation;
  request.harmonicEventCount = 1;
  request.phraseBars = 1;
  assert(realizeChordProgression(request).status ==
         ChordProgressionStatus::InvalidRequest);

  request.requestedId = ProgressionId::PopCycle;
  request.family = RhythmFamily::Count;
  assert(realizeChordProgression(request).status ==
         ChordProgressionStatus::InvalidRequest);

  request.family = RhythmFamily::FourFloor;
  request.harmonicEventCount =
      static_cast<uint8_t>(kMaxHarmonicEvents + 1u);
  assert(realizeChordProgression(request).status ==
         ChordProgressionStatus::InvalidRequest);

  request.harmonicEventCount = 1;
  request.phraseBars = 3;
  assert(realizeChordProgression(request).status ==
         ChordProgressionStatus::InvalidRequest);
}

void testAutoUsesExistingChordPitchDomainDeterministically() {
  for (uint8_t familyValue = 0;
       familyValue < static_cast<uint8_t>(RhythmFamily::Count);
       ++familyValue) {
    for (uint16_t ordinal = 0; ordinal < 128; ++ordinal) {
      const GenerationContext generation =
          contextFor(15, familyValue, ordinal);
      const ChordProgressionResult first = realize(
          ProgressionId::Auto, static_cast<RhythmFamily>(familyValue), 4, 1,
          generation);
      const ChordProgressionResult repeat = realize(
          ProgressionId::Auto, static_cast<RhythmFamily>(familyValue), 4, 1,
          generation);
      assert(first.status == ChordProgressionStatus::Ok ||
             first.status == ChordProgressionStatus::ValidButStatic);
      assert(first.plan.id != ProgressionId::Auto);
      assert(std::memcmp(&first.plan, &repeat.plan,
                         sizeof(ChordProgressionPlan)) == 0);
      assertEventBounds(first);
    }
  }
}

}  // namespace

int main() {
  testEveryConcreteProgressionIsProfileReachable();
  testDataOnlyGenreReuseCriterion();
  testDeterminismAcrossEveryProfilePair();
  testBoundsForEveryProgressionAndAllowedRequestSize();
  testGrammarPrefixAndCycleContract();
  testInvalidRequestsAreRejected();
  testAutoUsesExistingChordPitchDomainDeterministically();
  return 0;
}
