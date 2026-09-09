#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/rhythm_types.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kFirstIdentity = 1;
constexpr uint16_t kIdentityCount = 128;
constexpr uint8_t kPilotCount = 4;
constexpr uint8_t kLevelCount = 3;

struct Pilot {
  const char* name;
  GenerativeMode mode;
  uint8_t recipe;
};

constexpr std::array<Pilot, kPilotCount> kPilots = {{
    {"Acid", GenerativeMode::Acid, kBaseRecipeId},
    {"House", GenerativeMode::House, kBaseRecipeId},
    {"DubTechno", GenerativeMode::Reggae, 5},
    {"DnB", GenerativeMode::DrumAndBass, kBaseRecipeId},
}};

constexpr std::array<RealizationLevel, kLevelCount> kLevels = {{
    RealizationLevel::P1Canonical,
    RealizationLevel::P2Variation,
    RealizationLevel::P3Transformation,
}};

constexpr const char* kLevelNames[kLevelCount] = {"P1", "P2", "P3"};

uint8_t bitCount(StepMask value) {
  uint8_t count = 0;
  while (value != 0) {
    value = static_cast<StepMask>(value & static_cast<StepMask>(value - 1u));
    ++count;
  }
  return count;
}

GenreSettings settingsFor(const Pilot& pilot) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(pilot.mode);
  settings.recipe = pilot.recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = 0;
  context.level = level;
  context.generationAttemptOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

StepMask drumOnsets(const DrumPatternSet& drums, uint8_t voice) {
  StepMask mask = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[voice].steps[step].hit) {
      mask = static_cast<StepMask>(mask | stepBit(step));
    }
  }
  return mask;
}

StepMask synthAttacks(const SynthPattern& synth) {
  StepMask mask = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& event = synth.steps[step];
    if (event.note >= 0 && !event.slide) {
      mask = static_cast<StepMask>(mask | stepBit(step));
    }
  }
  return mask;
}

StepMask clickStructure(const DrumPatternSet& drums,
                        const SynthPattern& synthA,
                        const SynthPattern& synthB) {
  StepMask mask = static_cast<StepMask>(synthAttacks(synthA) | synthAttacks(synthB));
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    mask = static_cast<StepMask>(mask | drumOnsets(drums, voice));
  }
  return mask;
}

uint16_t drumAttackCount(const DrumPatternSet& drums) {
  uint16_t count = 0;
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    count = static_cast<uint16_t>(count + bitCount(drumOnsets(drums, voice)));
  }
  return count;
}

struct Sample {
  bool ready = false;
  uint16_t identity = 0;
  StepMask click = 0;
  RhythmArchetypeId archetype = kNoArchetypeId;
  BassRhythmId bass = BassRhythmId::Auto;
  ChordRhythmId chord = ChordRhythmId::Auto;
  StepMask kick = 0;
  uint16_t attacks = 0;
};

Sample materialize(const Pilot& pilot, uint16_t identity, RealizationLevel level) {
  Sample sample{};
  sample.identity = identity;
  const GenreSettings settings = settingsFor(pilot);
  const StrongRhythmMigrationContext context = contextFor(level);
  StrongRhythmFrozenSelection selection{};
  const StrongRhythmMigrationResult selected =
      resolveStrongRhythmFrozenSelection(settings, context, identity, selection);
  if (selected.status != StrongRhythmMigrationStatus::Applied || !selection.resolved) {
    return sample;
  }

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  const StrongRhythmMigrationResult result = migrateStrongRhythmFrozenMaterial(
      settings, selection, context, drums, synthA, synthB);
  if (result.status != StrongRhythmMigrationStatus::Applied) return sample;

  sample.ready = true;
  sample.click = clickStructure(drums, synthA, synthB);
  sample.archetype = selection.composition.rhythmArchetypeId;
  sample.bass = selection.composition.bassRhythm;
  sample.chord = selection.composition.chordRhythm;
  sample.kick = drumOnsets(drums, KICK);
  sample.attacks = static_cast<uint16_t>(
      drumAttackCount(drums) + bitCount(synthAttacks(synthA)) + bitCount(synthAttacks(synthB)));
  return sample;
}

struct LevelCorpus {
  uint16_t ready = 0;
  uint32_t attackTotal = 0;
  std::map<StepMask, uint16_t> clickCounts;
  std::map<StepMask, Sample> clickWitness;
};

struct ArchetypeStats {
  uint16_t count = 0;
  uint16_t fourFloor = 0;
};

uint32_t intersectionSize(const std::map<StepMask, uint16_t>& left,
                          const std::map<StepMask, uint16_t>& right) {
  uint32_t result = 0;
  for (const auto& entry : left) {
    if (right.find(entry.first) != right.end()) ++result;
  }
  return result;
}

uint32_t collisionPairs(const std::map<StepMask, uint16_t>& left,
                        const std::map<StepMask, uint16_t>& right) {
  uint32_t result = 0;
  for (const auto& entry : left) {
    const auto it = right.find(entry.first);
    if (it != right.end()) {
      result += static_cast<uint32_t>(entry.second) * it->second;
    }
  }
  return result;
}

void printWitness(const char* side, const Pilot& pilot, uint8_t level,
                  const Sample& sample) {
  std::printf(
      "G4_C0 XWITNESS side=%s pilot=%s level=%s identity=%u click=%04x archetype=%u bass=%u chord=%u\n",
      side, pilot.name, kLevelNames[level], static_cast<unsigned>(sample.identity),
      static_cast<unsigned>(sample.click), static_cast<unsigned>(sample.archetype),
      static_cast<unsigned>(sample.bass), static_cast<unsigned>(sample.chord));
}

uint32_t harmonicChangeMs(const GenerationProfileView& profile, uint16_t bpm) {
  if (profile.harmonicChangeRate != HarmonicChangeRateId::Every2Beats || bpm == 0) {
    return 0;
  }
  return 120000u / bpm;
}

}  // namespace

int main() {
  std::array<std::array<LevelCorpus, kLevelCount>, kPilotCount> corpus{};
  std::array<std::map<RhythmArchetypeId, ArchetypeStats>, kPilotCount> archetypes{};
  const StepMask fourFloor = static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));

  int failures = 0;
  for (uint8_t pilotIndex = 0; pilotIndex < kPilotCount; ++pilotIndex) {
    const Pilot& pilot = kPilots[pilotIndex];
    const GenerationProfileView profile = generationProfileFor(settingsFor(pilot));
    if (!isValidGenerationProfile(profile)) {
      std::fprintf(stderr, "G4-C0 cross FAIL: invalid profile for %s\n", pilot.name);
      ++failures;
      continue;
    }

    for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
      LevelCorpus& level = corpus[pilotIndex][levelIndex];
      for (uint16_t offset = 0; offset < kIdentityCount; ++offset) {
        const uint16_t identity = static_cast<uint16_t>(kFirstIdentity + offset);
        const Sample sample = materialize(pilot, identity, kLevels[levelIndex]);
        if (!sample.ready) continue;
        ++level.ready;
        level.attackTotal += sample.attacks;
        ++level.clickCounts[sample.click];
        level.clickWitness.emplace(sample.click, sample);

        if (levelIndex == 1) {
          ArchetypeStats& stats = archetypes[pilotIndex][sample.archetype];
          ++stats.count;
          if ((sample.kick & fourFloor) == fourFloor) ++stats.fourFloor;
        }
      }
      if (level.ready != kIdentityCount) {
        std::fprintf(stderr,
                     "G4-C0 cross FAIL: %s %s materialized %u/%u\n",
                     pilot.name, kLevelNames[levelIndex],
                     static_cast<unsigned>(level.ready),
                     static_cast<unsigned>(kIdentityCount));
        ++failures;
      }

      const uint32_t avgAttacksX100 = level.ready == 0
          ? 0
          : (level.attackTotal * 100u) / level.ready;
      const uint16_t bpms[3] = {
          profile.corridor.bpmMin,
          profile.corridor.suggestedBpm,
          profile.corridor.bpmMax,
      };
      const char* tempoNames[3] = {"low", "suggested", "high"};
      for (uint8_t tempo = 0; tempo < 3; ++tempo) {
        const uint32_t attacksPerSecondX100 =
            (avgAttacksX100 * bpms[tempo]) / 240u;
        std::printf(
            "G4_C0 PHYSICAL pilot=%s level=%s tempo=%s bpm=%u avg_attacks_per_bar_x100=%u attacks_per_second_x100=%u harmonic_change_ms=%u\n",
            pilot.name, kLevelNames[levelIndex], tempoNames[tempo],
            static_cast<unsigned>(bpms[tempo]),
            static_cast<unsigned>(avgAttacksX100),
            static_cast<unsigned>(attacksPerSecondX100),
            static_cast<unsigned>(harmonicChangeMs(profile, bpms[tempo])));
      }
    }

    for (const auto& entry : archetypes[pilotIndex]) {
      std::printf(
          "G4_C0 ARCHETYPE pilot=%s level=P2 archetype=%u count=%u four_floor=%u\n",
          pilot.name, static_cast<unsigned>(entry.first),
          static_cast<unsigned>(entry.second.count),
          static_cast<unsigned>(entry.second.fourFloor));
    }
  }

  for (uint8_t level = 0; level < kLevelCount; ++level) {
    for (uint8_t left = 0; left < kPilotCount; ++left) {
      for (uint8_t right = static_cast<uint8_t>(left + 1); right < kPilotCount; ++right) {
        const auto& a = corpus[left][level].clickCounts;
        const auto& b = corpus[right][level].clickCounts;
        const uint32_t shared = intersectionSize(a, b);
        const uint32_t unionCount = static_cast<uint32_t>(a.size() + b.size() - shared);
        const uint32_t jaccardX1000 = unionCount == 0 ? 0 : (shared * 1000u) / unionCount;
        const uint32_t pairs = collisionPairs(a, b);
        const uint32_t pairRateX10000 =
            (pairs * 10000u) / (static_cast<uint32_t>(kIdentityCount) * kIdentityCount);
        std::printf(
            "G4_C0 CROSS level=%s left=%s right=%s left_unique=%zu right_unique=%zu shared_click=%u jaccard_x1000=%u collision_pairs=%u collision_rate_x10000=%u\n",
            kLevelNames[level], kPilots[left].name, kPilots[right].name,
            a.size(), b.size(), static_cast<unsigned>(shared),
            static_cast<unsigned>(jaccardX1000), static_cast<unsigned>(pairs),
            static_cast<unsigned>(pairRateX10000));

        bool printed = false;
        for (const auto& entry : a) {
          const auto rightIt = b.find(entry.first);
          if (rightIt == b.end()) continue;
          printWitness("A", kPilots[left], level,
                       corpus[left][level].clickWitness.at(entry.first));
          printWitness("B", kPilots[right], level,
                       corpus[right][level].clickWitness.at(entry.first));
          printed = true;
          break;
        }
        if (!printed) {
          std::printf("G4_C0 XWITNESS none level=%s left=%s right=%s\n",
                      kLevelNames[level], kPilots[left].name, kPilots[right].name);
        }
      }
    }
  }

  if (failures != 0) {
    std::fprintf(stderr, "G4_C0 CROSS status=measurement_failed failures=%d\n", failures);
    return 1;
  }
  std::puts("G4_C0 CROSS status=measurement_complete");
  return 0;
}
