#include <cassert>
#include <cstdint>
#include <iostream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

bool isExactProfile(GenerativeMode mode, uint8_t recipe) {
  const GenerationProfileView profile = generationProfileFor(settingsFor(mode, recipe));
  return profile.progressions.candidates != nullptr && profile.recipe == recipe;
}

StrongRhythmMigrationContext contextFor(int16_t patternAddress) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = patternAddress;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

SynthPattern sourcePattern(int base) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(base + (step % 5));
    pattern.steps[step].velocity = 100;
    pattern.steps[step].probability = 100;
  }
  return pattern;
}

bool registerOk(const SynthPattern& pattern, uint8_t minimum, uint8_t maximum) {
  for (const SynthStep& step : pattern.steps) {
    if (step.note < 0) continue;
    if (step.note < minimum || step.note > maximum) return false;
  }
  return true;
}

void printFailure(GenerativeMode mode,
                  uint8_t recipe,
                  int16_t patternAddress,
                  const StrongRhythmMigrationResult& result,
                  bool aRegisterOk,
                  bool bRegisterOk) {
  std::cerr
      << "FAIL mode=" << static_cast<int>(mode)
      << " recipe=" << static_cast<int>(recipe)
      << " address=" << patternAddress
      << " status=" << static_cast<int>(result.status)
      << " progression=" << static_cast<int>(result.progressionId)
      << " bassContour=" << static_cast<int>(result.bassPitchContour)
      << " bassPitch=" << static_cast<int>(result.bassPitchBehaviorStatus)
      << " bassTonal=" << static_cast<int>(result.bassTonalStatus)
      << " bassProj=" << static_cast<int>(result.bassTonalProjectionStatus)
      << " bassAdapt=" << static_cast<int>(result.bassTonalAdaptStatus)
      << " chordTonal=" << static_cast<int>(result.chordTonalStatus)
      << " chordProj=" << static_cast<int>(result.chordTonalProjectionStatus)
      << " chordAdapt=" << static_cast<int>(result.chordTonalAdaptStatus)
      << " melodyContour=" << static_cast<int>(result.melodicPitchContour)
      << " melodyTonal=" << static_cast<int>(result.melodicTonalStatus)
      << " melodyProj=" << static_cast<int>(result.melodicTonalProjectionStatus)
      << " melodyAdapt=" << static_cast<int>(result.melodicTonalAdaptStatus)
      << " synthBRole=" << static_cast<int>(result.synthBRole)
      << " Areg=" << (aRegisterOk ? 1 : 0)
      << " Breg=" << (bRegisterOk ? 1 : 0)
      << '\n';
}

}  // namespace

int main() {
  uint32_t cases = 0;
  uint32_t failures = 0;
  uint16_t exactProfiles = 0;

  for (uint8_t modeValue = 0; modeValue < kGenerativeModeCount; ++modeValue) {
    const GenerativeMode mode = static_cast<GenerativeMode>(modeValue);
    for (uint8_t recipe = 0; recipe <= kDustyJazzRecipeId; ++recipe) {
      if (!isExactProfile(mode, recipe)) continue;
      ++exactProfiles;
      const GenreSettings settings = settingsFor(mode, recipe);

      // StrongRhythmMigrationContext validates patternAddress against
      // kMaxGlobalPatterns == 256. Exhausting 0..255 therefore covers every
      // production-reachable ordinal; asking this bridge for 512 distinct
      // ordinals would violate its existing public contract.
      for (int16_t patternAddress = 0;
           patternAddress < kMaxGlobalPatterns; ++patternAddress) {
        DrumPatternSet drums{};
        SynthPattern synthA = sourcePattern(36);
        SynthPattern synthB = sourcePattern(60);
        const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
            settings, contextFor(patternAddress), drums, synthA, synthB);
        const bool aRegisterOk = registerOk(synthA, 24, 47);
        const bool bRegisterOk = registerOk(synthB, 48, 71);
        if (result.status != StrongRhythmMigrationStatus::Applied ||
            !result.tonalMaterializationApplied ||
            !aRegisterOk || !bRegisterOk) {
          ++failures;
          if (failures <= 32)
            printFailure(mode, recipe, patternAddress, result,
                         aRegisterOk, bRegisterOk);
        }
        ++cases;
      }
    }
  }

  assert(exactProfiles == 33);
  assert(cases == 33u * static_cast<uint32_t>(kMaxGlobalPatterns));
  assert(failures == 0);
  std::cout << "Stage 15 tonal register sweep: " << cases << " cases OK\n";
  return 0;
}
