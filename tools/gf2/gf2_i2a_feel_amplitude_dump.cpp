// GF2-I2A — FEEL amplitude census.
//
// Emits one TSV row per (genre, recipe, FEEL profile, FEEL amount) describing
// how much timing displacement the profile actually produces in materialized
// material. This is the measurement the I2A design gate is built on: GF2-I2
// proved the profile is causal, and this tool answers how large that cause is
// at each amount, including the amounts the product actually ships.
//
// Reports what moved as well as how much: the GF2 magnitude contract requires
// the anchor voice to be tracked separately, because displacement measured
// against nothing steady is not perceived as displacement.

#include "../../scenes.h"
#include "../../src/dsp/genre_manager.h"
#include "../../src/dsp/mini_drumvoices.h"
#include "../../src/generation/composition/generation_profile.h"
#include "../../src/generation/migration/strong_rhythm_migration.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Scene persistence is linked for the genre/recipe name tables; the host build
// supplies the Arduino globals it expects.
SerialMock Serial;
SDMock SD;

using namespace GroovePuterRhythm;

namespace {

// Production-shipped and diagnostic amounts. 20 is the current default; 2 / 12 /
// 22 are the TIGHT / HUMAN / LOOSE presets.
constexpr uint8_t kAmounts[] = {0, 2, 12, 20, 22, 30, 40, 50, 60, 80, 100};

constexpr int16_t kPatternAddress = 3;

const char* profileName(uint8_t raw) {
  return feelProfileName(static_cast<FeelProfileId>(raw));
}

const char* routeName(StrongRhythmRoute route) {
  switch (route) {
    case StrongRhythmRoute::Legacy: return "LEGACY";
    case StrongRhythmRoute::TechnoBase: return "TECHNO_BASE";
    case StrongRhythmRoute::DrumAndBass: return "DNB";
    case StrongRhythmRoute::DubTechno: return "DUB_TECHNO";
    case StrongRhythmRoute::ChicagoJack: return "CHICAGO_JACK";
    case StrongRhythmRoute::RollingAcid: return "ROLLING_ACID";
    case StrongRhythmRoute::DeepChord: return "DEEP_CHORD";
    case StrongRhythmRoute::Stage7Composition: return "STAGE7";
    default: return "OTHER";
  }
}

struct Measurement {
  bool applied = false;
  int onsets = 0;
  int displaced = 0;
  int maxTicks = 0;
  bool anchorMoved = false;
  int perRole[6] = {0, 0, 0, 0, 0, 0};  // kick snare hats otherDrums synthA synthB
  uint32_t fingerprint = 2166136261u;
};

void mix(uint32_t& hash, int value) {
  hash ^= static_cast<uint32_t>(static_cast<uint8_t>(value));
  hash *= 16777619u;
}

int roleBucket(int voice) {
  if (voice == KICK) return 0;
  if (voice == SNARE) return 1;
  if (voice == CLOSED_HAT || voice == OPEN_HAT) return 2;
  return 3;
}

void account(Measurement& m, int bucket, int timing) {
  ++m.onsets;
  mix(m.fingerprint, timing);
  if (timing == 0) return;
  ++m.displaced;
  ++m.perRole[bucket];
  const int magnitude = timing < 0 ? -timing : timing;
  if (magnitude > m.maxTicks) m.maxTicks = magnitude;
  if (bucket == 0) m.anchorMoved = true;
}

Measurement measure(const GenreSettings& settings,
                    FeelProfileId profile,
                    uint8_t amount) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = kPatternAddress;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = profile;
  context.feelAmount = amount;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  Measurement m{};
  const StrongRhythmMigrationResult result =
      migrateStrongRhythmMaterial(settings, context, drums, synthA, synthB);
  if (result.status != StrongRhythmMigrationStatus::Applied) return m;
  m.applied = true;

  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& s = drums.voices[voice].steps[step];
      if (!s.hit) continue;
      account(m, roleBucket(voice), s.timing);
    }
  }
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (synthA.steps[step].note >= 0) account(m, 4, synthA.steps[step].timing);
    if (synthB.steps[step].note >= 0) account(m, 5, synthB.steps[step].timing);
  }
  return m;
}

}  // namespace

int main() {
  std::printf(
      "genre_id\tgenre\trecipe_id\trecipe\troute\tsuggested_bpm\tprofile\t"
      "amount\tstatus\tonsets\tdisplaced\tmax_ticks\tmax_ms_x10\tanchor_moved\t"
      "d_kick\td_snare\td_hats\td_other_drums\td_synth_a\td_synth_b\t"
      "timing_fingerprint\n");

  for (int genreIndex = 0; genreIndex < kGenerativeModeCount; ++genreIndex) {
    const auto genre = static_cast<GenerativeMode>(genreIndex);
    const uint8_t recipeCount = availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < recipeCount; ++ordinal) {
      GenreRecipeId recipe = 0;
      if (!availableRecipeAt(genre, ordinal, recipe)) continue;

      GenreSettings settings{};
      settings.generativeMode = static_cast<uint8_t>(genreIndex);
      settings.recipe = static_cast<uint8_t>(recipe);
      settings.rhythmSelectionMode =
          static_cast<uint8_t>(RhythmSelectionMode::Auto);
      settings.rhythmArchetypeId = kNoArchetypeId;

      const StrongRhythmRoute route = selectStrongRhythmRoute(settings);
      const GenerationProfileView profile = generationProfileFor(settings);
      const uint16_t bpm = profile.corridor.suggestedBpm;

      for (uint8_t raw = 0;
           raw <= static_cast<uint8_t>(FeelProfileId::Auto); ++raw) {
        for (uint8_t amount : kAmounts) {
          const Measurement m =
              measure(settings, static_cast<FeelProfileId>(raw), amount);
          // One tick is a 96th of a bar; at `bpm` that is 60000/bpm/4/24 ms.
          const int maxMsX10 = (bpm == 0 || m.maxTicks == 0)
              ? 0
              : static_cast<int>(
                    (600000L * m.maxTicks) / (static_cast<long>(bpm) * 96L));
          std::printf(
              "%d\t%s\t%u\t%s\t%s\t%u\t%s\t%u\t%s\t%d\t%d\t%d\t%d\t%s\t"
              "%d\t%d\t%d\t%d\t%d\t%d\t%08x\n",
              genreIndex, GenreCatalog::generativeModeName(genre),
              static_cast<unsigned>(recipe), GenreCatalog::recipeName(recipe),
              routeName(route), static_cast<unsigned>(bpm), profileName(raw),
              static_cast<unsigned>(amount),
              m.applied ? "APPLIED" : "NOT_APPLIED", m.onsets, m.displaced,
              m.maxTicks, maxMsX10, m.anchorMoved ? "YES" : "NO",
              m.perRole[0], m.perRole[1], m.perRole[2], m.perRole[3],
              m.perRole[4], m.perRole[5],
              m.applied ? m.fingerprint : 0u);
        }
      }
    }
  }
  return 0;
}
