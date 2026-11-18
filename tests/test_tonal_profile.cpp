#include <cassert>
#include <cstdint>
#include <iostream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/tonal_profile.h"

using namespace GroovePuterRhythm;

namespace {

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe = kBaseRecipeId) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  return settings;
}

void assertValid(const TonalGenerationProfile& profile) {
  assert((profile.bassPolicy.allowedContours &
          bassPitchContourBit(BassPitchContourId::RootAnchor)) != 0);
  assert((profile.bassPolicy.allowedArticulations &
          bassArticulationStyleBit(BassArticulationStyleId::Plain)) != 0);
  assert((profile.bassPolicy.preferredContours &
          ~profile.bassPolicy.allowedContours) == 0);
  assert((profile.bassPolicy.preferredArticulations &
          ~profile.bassPolicy.allowedArticulations) == 0);

  assert((profile.melodicPolicy.allowedRhythmOperations &
          melodicRhythmOperationBit(MelodicRhythmOperationId::Preserve)) != 0);
  assert((profile.melodicPolicy.allowedContours &
          melodicContourBit(MelodicContourId::Static)) != 0);
  assert((profile.melodicPolicy.allowedMotifOperations &
          melodicMotifOperationBit(MelodicMotifOperationId::None)) != 0);
  assert((profile.melodicPolicy.preferredRhythmOperations &
          ~profile.melodicPolicy.allowedRhythmOperations) == 0);
  assert((profile.melodicPolicy.preferredContours &
          ~profile.melodicPolicy.allowedContours) == 0);
  assert((profile.melodicPolicy.preferredMotifOperations &
          ~profile.melodicPolicy.allowedMotifOperations) == 0);

  assert(profile.bassRegister.minMidi >= 24);
  assert(profile.bassRegister.maxMidi <= 71);
  assert(profile.secondaryRegister.minMidi >= 24);
  assert(profile.secondaryRegister.maxMidi <= 71);
  assert(profile.bassRegister.minMidi <= profile.bassRegister.maxMidi);
  assert(profile.secondaryRegister.minMidi <= profile.secondaryRegister.maxMidi);
  assert(profile.bassRegister.maxMidi < profile.secondaryRegister.minMidi);
  assert(profile.bassRegister.maxAdjacentLeapSemitones <= 23);
  assert(profile.secondaryRegister.maxAdjacentLeapSemitones <= 23);
}

void testAllModes() {
  for (uint8_t value = 0; value < kGenerativeModeCount; ++value) {
    const TonalGenerationProfile profile = tonalGenerationProfileFor(
        settingsFor(static_cast<GenerativeMode>(value)));
    assertValid(profile);
  }
}

void testTechnoStaysNearlyStatic() {
  const TonalGenerationProfile profile = tonalGenerationProfileFor(
      settingsFor(GenerativeMode::Techno));
  assert(profile.bassPolicy.allowedContours ==
         bassPitchContourBit(BassPitchContourId::RootAnchor));
  assert(profile.bassPolicy.preferredContours == 0);
  assert(profile.melodicPolicy.allowedContours ==
         (melodicContourBit(MelodicContourId::Static) |
          melodicContourBit(MelodicContourId::Neighbor)));
  assert(profile.melodicPolicy.preferredContours ==
         melodicContourBit(MelodicContourId::Neighbor));
  assert(profile.bassRegister.maxAdjacentLeapSemitones == 12);
}

void testRaveUsesRepeatedHookProfile() {
  const TonalGenerationProfile profile = tonalGenerationProfileFor(
      settingsFor(GenerativeMode::Rave));
  const uint16_t expectedMelodicContours =
      melodicContourBit(MelodicContourId::Static) |
      melodicContourBit(MelodicContourId::Neighbor) |
      melodicContourBit(MelodicContourId::LeapReturn) |
      melodicContourBit(MelodicContourId::RepeatThenUp) |
      melodicContourBit(MelodicContourId::RepeatThenDown);
  const uint16_t expectedPreferredMelodicContours =
      melodicContourBit(MelodicContourId::LeapReturn) |
      melodicContourBit(MelodicContourId::RepeatThenUp) |
      melodicContourBit(MelodicContourId::Neighbor);
  assert(profile.bassPolicy.allowedContours ==
         (bassPitchContourBit(BassPitchContourId::RootAnchor) |
          bassPitchContourBit(BassPitchContourId::RootFifth) |
          bassPitchContourBit(BassPitchContourId::NeighborReturn) |
          bassPitchContourBit(BassPitchContourId::LeapReturn) |
          bassPitchContourBit(BassPitchContourId::PedalTurn)));
  assert(profile.bassPolicy.preferredContours ==
         (bassPitchContourBit(BassPitchContourId::RootFifth) |
          bassPitchContourBit(BassPitchContourId::LeapReturn) |
          bassPitchContourBit(BassPitchContourId::NeighborReturn)));
  assert(profile.melodicPolicy.allowedContours == expectedMelodicContours);
  assert(profile.melodicPolicy.preferredContours ==
         expectedPreferredMelodicContours);
  assert(profile.bassRegister.maxAdjacentLeapSemitones == 23);
}

void testHouseMovesWithDriveLead() {
  const TonalGenerationProfile profile =
      tonalGenerationProfileFor(settingsFor(GenerativeMode::House));
  assert(profile.bassPolicy.allowedContours !=
         bassPitchContourBit(BassPitchContourId::RootAnchor));
  assert((profile.bassPolicy.allowedArticulations &
          bassArticulationStyleBit(BassArticulationStyleId::AccentPulse)) != 0);
  assert((profile.melodicPolicy.allowedContours &
          melodicContourBit(MelodicContourId::StepUp)) != 0);
  assert((profile.melodicPolicy.preferredContours &
          melodicContourBit(MelodicContourId::MotifAnswer)) != 0);
}

void testMovingProfilesUseFullBassCorridorLeapBudget() {
  for (GenerativeMode mode : {
           GenerativeMode::Acid,
           GenerativeMode::Outrun,
           GenerativeMode::Darksynth,
           GenerativeMode::Electro,
           GenerativeMode::Reggae,
           GenerativeMode::TripHop,
           GenerativeMode::Broken,
           GenerativeMode::Chip,
           GenerativeMode::HipHop,
           GenerativeMode::FunkSoul,
           GenerativeMode::UkGarage,
           GenerativeMode::DrumAndBass,
           GenerativeMode::LoFi,
       }) {
    const TonalGenerationProfile profile = tonalGenerationProfileFor(
        settingsFor(mode));
    assert(profile.bassRegister.minMidi == 24);
    assert(profile.bassRegister.maxMidi == 47);
    assert(profile.bassRegister.maxAdjacentLeapSemitones == 23);
  }
}

void testSynthBassProfilesMatchTheirIntendedContours() {
  const uint16_t rootOctave =
      bassPitchContourBit(BassPitchContourId::RootOctave);
  const uint16_t rootFifth =
      bassPitchContourBit(BassPitchContourId::RootFifth);
  const uint16_t leapReturn =
      bassPitchContourBit(BassPitchContourId::LeapReturn);
  for (GenerativeMode mode : {GenerativeMode::Outrun,
                              GenerativeMode::Darksynth}) {
    const TonalGenerationProfile profile = tonalGenerationProfileFor(
        settingsFor(mode));
    assert((profile.bassPolicy.allowedContours & rootOctave) != 0);
    assert(profile.bassPolicy.preferredContours ==
           (bassPitchContourBit(BassPitchContourId::RootAnchor) | rootOctave));
  }

  const TonalGenerationProfile chip = tonalGenerationProfileFor(
      settingsFor(GenerativeMode::Chip));
  assert((chip.bassPolicy.allowedContours & rootOctave) == 0);
  assert((chip.bassPolicy.allowedContours & rootFifth) != 0);
  assert((chip.bassPolicy.allowedContours & leapReturn) != 0);
  assert(chip.bassPolicy.preferredContours != 0);
}

void testVariantsInheritModePolicy() {
  const TonalGenerationProfile lofiBase = tonalGenerationProfileFor(
      settingsFor(GenerativeMode::LoFi));
  for (uint8_t recipe : {
           kClassicChillRecipeId,
           kDrunkenGrooveRecipeId,
           kLoFiHouseRecipeId,
           kMinimalSleepRecipeId,
       }) {
    const TonalGenerationProfile variant = tonalGenerationProfileFor(
        settingsFor(GenerativeMode::LoFi, recipe));
    assert(variant.bassPolicy.allowedContours == lofiBase.bassPolicy.allowedContours);
    assert(variant.bassPolicy.preferredContours == lofiBase.bassPolicy.preferredContours);
    assert(variant.melodicPolicy.allowedContours == lofiBase.melodicPolicy.allowedContours);
    assert(variant.melodicPolicy.preferredContours == lofiBase.melodicPolicy.preferredContours);
  }

  const TonalGenerationProfile hiphopBase = tonalGenerationProfileFor(
      settingsFor(GenerativeMode::HipHop));
  for (uint8_t recipe : {kGoldenEraRecipeId, kDustyJazzRecipeId}) {
    const TonalGenerationProfile variant = tonalGenerationProfileFor(
        settingsFor(GenerativeMode::HipHop, recipe));
    assert(variant.bassPolicy.allowedContours == hiphopBase.bassPolicy.allowedContours);
    assert(variant.melodicPolicy.allowedContours == hiphopBase.melodicPolicy.allowedContours);
  }
}

}  // namespace

int main() {
  testAllModes();
  testTechnoStaysNearlyStatic();
  testRaveUsesRepeatedHookProfile();
  testHouseMovesWithDriveLead();
  testMovingProfilesUseFullBassCorridorLeapBudget();
  testSynthBassProfilesMatchTheirIntendedContours();
  testVariantsInheritModePolicy();
  std::cout << "Stage 15 tonal profile matrix: OK\n";
  return 0;
}
