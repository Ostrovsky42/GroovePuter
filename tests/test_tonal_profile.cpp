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

void testAcceptedStaticModesStayConservative() {
  for (GenerativeMode mode : {
           GenerativeMode::House,
           GenerativeMode::Techno,
           GenerativeMode::Rave,
       }) {
    const TonalGenerationProfile profile = tonalGenerationProfileFor(
        settingsFor(mode));
    assert(profile.bassPolicy.allowedContours ==
           bassPitchContourBit(BassPitchContourId::RootAnchor));
    assert(profile.bassPolicy.preferredContours == 0);
    assert(profile.melodicPolicy.allowedContours ==
           melodicContourBit(MelodicContourId::Static));
    assert(profile.melodicPolicy.preferredContours == 0);
    assert(profile.bassRegister.maxAdjacentLeapSemitones == 12);
  }
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

void testSynthProfilesAvoidUnmaterializableRootOctaveAuto() {
  for (GenerativeMode mode : {
           GenerativeMode::Outrun,
           GenerativeMode::Darksynth,
           GenerativeMode::Chip,
       }) {
    const TonalGenerationProfile profile = tonalGenerationProfileFor(
        settingsFor(mode));
    assert((profile.bassPolicy.allowedContours &
            bassPitchContourBit(BassPitchContourId::RootOctave)) == 0);
    assert((profile.bassPolicy.allowedContours &
            bassPitchContourBit(BassPitchContourId::RootFifth)) != 0);
    assert((profile.bassPolicy.allowedContours &
            bassPitchContourBit(BassPitchContourId::LeapReturn)) != 0);
    assert(profile.bassPolicy.preferredContours != 0);
  }
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
  testAcceptedStaticModesStayConservative();
  testMovingProfilesUseFullBassCorridorLeapBudget();
  testSynthProfilesAvoidUnmaterializableRootOctaveAuto();
  testVariantsInheritModePolicy();
  std::cout << "Stage 15 tonal profile matrix: OK\n";
  return 0;
}
