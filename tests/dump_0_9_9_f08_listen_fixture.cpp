#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

struct ListenCase {
  GenerativeMode mode;
  const char* modeName;
  int16_t ordinal;
  char voice;
  const char* progression;
};

constexpr ListenCase kCases[] = {
    {GenerativeMode::DrumAndBass, "DrumAndBass", 5, 'B', "MINOR FALL"},
    {GenerativeMode::TripHop, "TripHop", 4, 'A', "II-V-I"},
    {GenerativeMode::House, "House", 4, 'A', "POP CYCLE"},
    {GenerativeMode::House, "House", 5, 'B', "POP CYCLE"},
    {GenerativeMode::Outrun, "Outrun", 0, 'B', "POP CYCLE"},
    {GenerativeMode::UkGarage, "UkGarage", 1, 'B', "BORROWED LIFT"},
    {GenerativeMode::FunkSoul, "FunkSoul", 6, 'B', "BORROWED LIFT"},
    {GenerativeMode::TripHop, "TripHop", 2, 'B', "PARALLEL SHIFT"},
    {GenerativeMode::Acid, "Acid", 2, 'B', "STATIC MODAL"},
    {GenerativeMode::Techno, "Techno", 4, 'B', "PEDAL DRONE"},
    {GenerativeMode::Reggae, "Reggae", 4, 'B', "BORROWED LIFT"},
};

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
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
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
  return context;
}

void printByte(uint8_t value) {
  std::cout << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(value) << std::dec;
}

void printMask(StepMask value) {
  std::cout << std::hex << std::setw(4) << std::setfill('0')
            << static_cast<uint16_t>(value) << std::dec;
}

void printDrums(const DrumPatternSet& drums) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& event = drums.voices[voice].steps[step];
      printByte(event.hit ? 1u : 0u);
      printByte(event.accent ? 1u : 0u);
      printByte(event.velocity);
      printByte(static_cast<uint8_t>(event.timing));
      printByte(event.fx);
      printByte(event.fxParam);
      printByte(event.probability);
    }
  }
}

void printSynth(const SynthPattern& pattern) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& event = pattern.steps[step];
    printByte(static_cast<uint8_t>(event.note));
    printByte(event.slide ? 1u : 0u);
    printByte(event.accent ? 1u : 0u);
    printByte(event.ghost ? 1u : 0u);
    printByte(event.velocity);
    printByte(static_cast<uint8_t>(event.timing));
    printByte(event.fx);
    printByte(event.fxParam);
    printByte(event.probability);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 ||
      (std::strcmp(argv[1], "--legacy") != 0 &&
       std::strcmp(argv[1], "--independent") != 0)) {
    std::cerr << "usage: f08_listen_dump --legacy|--independent\n";
    return 2;
  }
  const bool legacyClock = std::strcmp(argv[1], "--legacy") == 0;

  std::cout << "index\tmode\tordinal\tvoice\tprogression\tclock\tbpm\t"
               "drums\tsynth_a\tsynth_b\n";

  for (size_t index = 0; index < sizeof(kCases) / sizeof(kCases[0]); ++index) {
    const ListenCase& listen = kCases[index];
    const GenreSettings settings = settingsFor(listen.mode);
    DrumPatternSet drums{};
    SynthPattern synthA = pitchSource(36, 5);
    SynthPattern synthB = pitchSource(60, 7);

    const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
        settings, contextFor(listen.ordinal), drums, synthA, synthB);
    if (result.status != StrongRhythmMigrationStatus::Applied) {
      std::cerr << "case " << (index + 1)
                << " migration failed status=" << static_cast<int>(result.status)
                << "\n";
      return 3;
    }

    const char* progression = chordProgressionName(result.progressionId);
    if (std::strcmp(progression, listen.progression) != 0) {
      std::cerr << "case " << (index + 1) << " progression moved: "
                << progression << " != " << listen.progression << "\n";
      return 4;
    }

    const GenerationProfileView profile = generationProfileFor(settings);
    if (profile.corridor.suggestedBpm == 0) {
      std::cerr << "case " << (index + 1) << " has no suggested BPM\n";
      return 5;
    }

    std::cout << (index + 1) << '\t'
              << listen.modeName << '\t'
              << listen.ordinal << '\t'
              << listen.voice << '\t'
              << progression << '\t';
    printMask(legacyClock ? result.chordOnsets : result.harmonicEventOnsets);
    std::cout << '\t'
              << static_cast<unsigned>(profile.corridor.suggestedBpm) << '\t';
    printDrums(drums);
    std::cout << '\t';
    printSynth(synthA);
    std::cout << '\t';
    printSynth(synthB);
    std::cout << '\n';
  }

  return 0;
}
