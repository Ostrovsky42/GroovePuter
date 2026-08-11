#include <cstdint>
#include <iostream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

SynthPattern pitchSource(int baseNote, int cycle) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(baseNote + (step % cycle));
    pattern.steps[step].velocity = 100;
    pattern.steps[step].probability = 100;
  }
  return pattern;
}

GenreSettings settingsFor(GenerativeMode mode) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = kBaseRecipeId;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(int16_t ordinal) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = ordinal;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

constexpr GenerativeMode kModes[] = {
    GenerativeMode::Acid, GenerativeMode::Outrun, GenerativeMode::Darksynth,
    GenerativeMode::Electro, GenerativeMode::Rave, GenerativeMode::Reggae,
    GenerativeMode::TripHop, GenerativeMode::Broken, GenerativeMode::Chip,
    GenerativeMode::House, GenerativeMode::Techno, GenerativeMode::HipHop,
    GenerativeMode::FunkSoul, GenerativeMode::UkGarage,
    GenerativeMode::DrumAndBass, GenerativeMode::LoFi,
};

}  // namespace

int main() {
  uint32_t failures = 0;
  for (GenerativeMode mode : kModes) {
    for (int16_t ordinal = 0; ordinal < 8; ++ordinal) {
      DrumPatternSet drums{};
      SynthPattern synthA = pitchSource(36, 5);
      SynthPattern synthB = pitchSource(60, 7);
      const StrongRhythmMigrationResult r = migrateStrongRhythmMaterial(
          settingsFor(mode), contextFor(ordinal), drums, synthA, synthB);
      if (r.status == StrongRhythmMigrationStatus::Applied) continue;
      ++failures;
      std::cout
          << "FAIL mode=" << static_cast<int>(mode)
          << " ordinal=" << ordinal
          << " status=" << static_cast<int>(r.status)
          << " bassPitch=" << static_cast<int>(r.bassPitchBehaviorStatus)
          << " bassTonal=" << static_cast<int>(r.bassTonalStatus)
          << " bassProj=" << static_cast<int>(r.bassTonalProjectionStatus)
          << " bassAdapt=" << static_cast<int>(r.bassTonalAdaptStatus)
          << " chordProg=" << static_cast<int>(r.chordProgressionStatus)
          << " chordTonal=" << static_cast<int>(r.chordTonalStatus)
          << " chordProj=" << static_cast<int>(r.chordTonalProjectionStatus)
          << " chordAdapt=" << static_cast<int>(r.chordTonalAdaptStatus)
          << " melodyPitch=" << static_cast<int>(r.melodicPitchIntentStatus)
          << " melodyTonal=" << static_cast<int>(r.melodicTonalStatus)
          << " melodyProj=" << static_cast<int>(r.melodicTonalProjectionStatus)
          << " melodyAdapt=" << static_cast<int>(r.melodicTonalAdaptStatus)
          << '\n';
    }
  }
  std::cout << "failures=" << failures << '\n';
  return failures == 0 ? 0 : 1;
}
