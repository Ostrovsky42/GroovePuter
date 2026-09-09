#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/phrase_execution.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kIdentityCount = 128;
constexpr uint8_t kRequestedPhraseBars = 4;

struct Pilot {
  const char* name;
  GenerativeMode mode;
  uint8_t recipe;
};

constexpr Pilot kPilots[] = {
    {"Acid", GenerativeMode::Acid, kBaseRecipeId},
    {"House", GenerativeMode::House, kBaseRecipeId},
    {"DubTechno", GenerativeMode::Reggae, 5},
    {"DnB", GenerativeMode::DrumAndBass, kBaseRecipeId},
};

GenreSettings settingsFor(const Pilot& pilot) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(pilot.mode);
  settings.recipe = pilot.recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

PhraseExecutionMaterializationSettings materializationSettings() {
  PhraseExecutionMaterializationSettings settings{};
  settings.level = RealizationLevel::P2Variation;
  settings.generationAttemptOrdinal = 0;
  settings.feelProfile = FeelProfileId::Straight;
  settings.feelAmount = 0;
  settings.tonalMaterializationEnabled = true;
  settings.rootPitchClass = 0;
  settings.scaleTypeValue = kScaleDorian;
  return settings;
}

bool checkPilot(const Pilot& pilot) {
  const GenreSettings settings = settingsFor(pilot);
  const PhraseExecutionMaterializationSettings materialization =
      materializationSettings();

  static PhraseExecutionScratch scratch{};
  static PreparedPhraseExecution prepared{};

  uint16_t ready = 0;
  uint16_t nonLoop = 0;
  uint16_t causalViolations = 0;

  for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
    const PhraseExecutionStatus status = preparePhraseExecution(
        settings, materialization, identity, kRequestedPhraseBars, scratch,
        prepared);
    if (status != PhraseExecutionStatus::Ready) {
      std::printf(
          "G4_I3_FAIL PHRASE_NOT_READY pilot=%s identity=%u status=%u\n",
          pilot.name, identity, static_cast<unsigned>(status));
      return false;
    }
    ++ready;

    const PhraseEvolutionLawId law = prepared.selection.composition.phraseLaw;
    if (law == PhraseEvolutionLawId::Loop) continue;
    ++nonLoop;

    if (prepared.phraseTrajectory == kNoTrajectoryId) {
      if (causalViolations < 8) {
        std::printf(
            "G4_I3_WITNESS FALSE_PHRASE_LAW pilot=%s identity=%u "
            "archetype=%u law=%u bars=%u\n",
            pilot.name, identity,
            prepared.selection.composition.rhythmArchetypeId,
            static_cast<unsigned>(law),
            prepared.selection.composition.phraseBars);
      }
      ++causalViolations;
    }
  }

  std::printf(
      "G4_I3_PILOT pilot=%s ready=%u non_loop=%u causal_violations=%u\n",
      pilot.name, ready, nonLoop, causalViolations);

  if (ready != kIdentityCount) {
    std::printf("G4_I3_FAIL READY_COUNT pilot=%s\n", pilot.name);
    return false;
  }
  if (causalViolations != 0) {
    std::printf("G4_I3_FAIL FALSE_PHRASE_LAW pilot=%s\n", pilot.name);
    return false;
  }

  // DnB is the positive control: at four requested bars its existing profile
  // and Stage-12 overlay must retain at least one genuinely causal non-Loop law.
  if (pilot.mode == GenerativeMode::DrumAndBass && nonLoop == 0) {
    std::printf("G4_I3_FAIL POSITIVE_CONTROL_DNB_HAS_NO_NON_LOOP\n");
    return false;
  }

  std::printf("G4_I3_PASS phrase_truthfulness pilot=%s\n", pilot.name);
  return true;
}

}  // namespace

int main() {
  bool ok = true;
  for (const Pilot& pilot : kPilots) ok = checkPilot(pilot) && ok;

  if (!ok) {
    std::printf("G4-I3 phrase-law truthfulness: FAIL\n");
    return 1;
  }

  std::printf("G4-I3 phrase-law truthfulness: PASS\n");
  return 0;
}
