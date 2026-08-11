#include <cstdint>
#include <iomanip>
#include <iostream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void hashByte(uint64_t& hash, uint8_t value) {
  hash ^= value;
  hash *= kFnvPrime;
}

void hashI8(uint64_t& hash, int8_t value) {
  hashByte(hash, static_cast<uint8_t>(value));
}

struct PatternHashes {
  uint64_t topology = kFnvOffset;
  uint64_t pitch = kFnvOffset;
  uint64_t articulation = kFnvOffset;
  uint64_t full = kFnvOffset;
};

PatternHashes fingerprint(const SynthPattern& pattern) {
  PatternHashes hashes{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& event = pattern.steps[step];
    const uint8_t active = event.note >= 0 ? 1u : 0u;

    hashByte(hashes.topology, step);
    hashByte(hashes.topology, active);

    hashByte(hashes.pitch, step);
    hashI8(hashes.pitch, event.note);

    hashByte(hashes.articulation, step);
    hashByte(hashes.articulation, event.slide ? 1u : 0u);
    hashByte(hashes.articulation, event.accent ? 1u : 0u);
    hashByte(hashes.articulation, event.ghost ? 1u : 0u);

    hashByte(hashes.full, step);
    hashI8(hashes.full, event.note);
    hashByte(hashes.full, event.slide ? 1u : 0u);
    hashByte(hashes.full, event.accent ? 1u : 0u);
    hashByte(hashes.full, event.ghost ? 1u : 0u);
    hashByte(hashes.full, event.velocity);
    hashI8(hashes.full, event.timing);
    hashByte(hashes.full, event.fx);
    hashByte(hashes.full, event.fxParam);
    hashByte(hashes.full, event.probability);
  }
  return hashes;
}

SynthPattern pitchSource(int baseNote, int cycle) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    SynthStep& event = pattern.steps[step];
    event.note = static_cast<int8_t>(baseNote + (step % cycle));
    event.velocity = static_cast<uint8_t>(84 + (step % 16));
    event.accent = (step % 4u) == 0u;
    event.slide = false;
    event.ghost = false;
    event.timing = static_cast<int8_t>((step % 3u) - 1);
    event.probability = 100;
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
  return context;
}

const char* modeName(GenerativeMode mode) {
  switch (mode) {
    case GenerativeMode::Acid: return "Acid";
    case GenerativeMode::Outrun: return "Outrun";
    case GenerativeMode::Darksynth: return "Darksynth";
    case GenerativeMode::Electro: return "Electro";
    case GenerativeMode::Rave: return "Rave";
    case GenerativeMode::Reggae: return "Reggae";
    case GenerativeMode::TripHop: return "TripHop";
    case GenerativeMode::Broken: return "Broken";
    case GenerativeMode::Chip: return "Chip";
    case GenerativeMode::House: return "House";
    case GenerativeMode::Techno: return "Techno";
    case GenerativeMode::HipHop: return "HipHop";
    case GenerativeMode::FunkSoul: return "FunkSoul";
    case GenerativeMode::UkGarage: return "UkGarage";
    case GenerativeMode::DrumAndBass: return "DrumAndBass";
    case GenerativeMode::LoFi: return "LoFi";
    default: return "Unknown";
  }
}

void printHash(uint64_t value) {
  std::cout << std::hex << std::setw(16) << std::setfill('0') << value << std::dec;
}

void printVoice(const char* mode, int ordinal, const char* voice,
                const PatternHashes& hashes,
                const StrongRhythmMigrationResult& result) {
  std::cout << mode << '\t' << ordinal << '\t' << voice << '\t'
            << static_cast<int>(result.status) << '\t'
            << static_cast<int>(result.synthBRole) << '\t';
  printHash(hashes.topology);
  std::cout << '\t';
  printHash(hashes.pitch);
  std::cout << '\t';
  printHash(hashes.articulation);
  std::cout << '\t';
  printHash(hashes.full);
  std::cout << '\n';
}

}  // namespace

int main() {
  constexpr GenerativeMode modes[] = {
      GenerativeMode::Acid,
      GenerativeMode::Outrun,
      GenerativeMode::Darksynth,
      GenerativeMode::Electro,
      GenerativeMode::Rave,
      GenerativeMode::Reggae,
      GenerativeMode::TripHop,
      GenerativeMode::Broken,
      GenerativeMode::Chip,
      GenerativeMode::House,
      GenerativeMode::Techno,
      GenerativeMode::HipHop,
      GenerativeMode::FunkSoul,
      GenerativeMode::UkGarage,
      GenerativeMode::DrumAndBass,
      GenerativeMode::LoFi,
  };

  std::cout << "mode\tordinal\tvoice\tstatus\tsecondary_role\ttopology\tpitch\tarticulation\tfull\n";
  for (GenerativeMode mode : modes) {
    for (int16_t ordinal = 0; ordinal < 8; ++ordinal) {
      DrumPatternSet drums{};
      SynthPattern synthA = pitchSource(36, 5);
      SynthPattern synthB = pitchSource(60, 7);
      const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
          settingsFor(mode), contextFor(ordinal), drums, synthA, synthB);
      printVoice(modeName(mode), ordinal, "A", fingerprint(synthA), result);
      printVoice(modeName(mode), ordinal, "B", fingerprint(synthB), result);
    }
  }
  return 0;
}
