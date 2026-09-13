#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/rhythm_selection.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kIdentityCount = 128;
constexpr StepMask kQuarterNotes =
    stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12);

enum class HousePulseWitness : uint8_t {
  None = 0,
  QuarterPulse,
};

GenreSettings houseSettings() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::House);
  settings.recipe = 0;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype, RhythmRole role) {
  for (uint8_t lane = 0; lane < archetype.laneCount; ++lane) {
    if (archetype.lanes[lane].role == role) return &archetype.lanes[lane];
  }
  return nullptr;
}

StepMask structuralAnchors(const LaneGrammar* lane) {
  if (lane == nullptr) return 0;
  return static_cast<StepMask>(lane->immutableAnchors |
                               lane->canonicalAnchors |
                               lane->preferred);
}

HousePulseWitness witnessForMasks(StepMask kick) {
  return (kick & kQuarterNotes) == kQuarterNotes
             ? HousePulseWitness::QuarterPulse
             : HousePulseWitness::None;
}

const RhythmArchetype* archetypeForId(RhythmArchetypeId archetypeId) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  if (definition == nullptr) return nullptr;
  return ReferenceVocabulary::archetypeFor(definition->key);
}

HousePulseWitness witnessForArchetype(RhythmArchetypeId archetypeId) {
  const RhythmArchetype* archetype = archetypeForId(archetypeId);
  if (archetype == nullptr) return HousePulseWitness::None;
  return witnessForMasks(structuralAnchors(laneFor(*archetype, RhythmRole::Kick)));
}

void mix(uint32_t& hash, uint32_t value) {
  hash ^= value;
  hash *= 16777619u;
}

uint32_t structuralIdeaSignature(const RhythmArchetype& archetype) {
  uint32_t hash = 2166136261u;
  constexpr RhythmRole roles[] = {
      RhythmRole::Kick,
      RhythmRole::Backbeat,
      RhythmRole::ClosedHat,
      RhythmRole::OpenHat,
      RhythmRole::Percussion,
  };
  for (RhythmRole role : roles) {
    const LaneGrammar* lane = laneFor(archetype, role);
    if (lane == nullptr) {
      mix(hash, 0xffffffffu);
      continue;
    }
    // Keep anchor-strength tiers separate. StraightDrive and StackedQuarters
    // intentionally cover the same quarter-note pulse, but one declares all
    // four kicks canonical while the other treats beats 2/4 as preferred.
    // Collapsing these tiers would erase a real structural idea distinction.
    mix(hash, static_cast<uint32_t>(lane->immutableAnchors));
    mix(hash, static_cast<uint32_t>(lane->canonicalAnchors));
    mix(hash, static_cast<uint32_t>(lane->preferred));
    mix(hash, static_cast<uint32_t>(lane->optional));
    mix(hash, static_cast<uint32_t>(lane->structuralMin));
    mix(hash, static_cast<uint32_t>(lane->structuralMax));
  }
  return hash;
}

bool checkAdmittedHouseOwnership() {
  const RhythmCompatibilityView compatibility =
      rhythmCompatibilityFor(houseSettings());
  if (compatibility.candidates == nullptr || compatibility.count < 2) {
    std::printf("G4_I5_FAIL I5_HOUSE_EMPTY_ADMISSION count=%u\n",
                compatibility.count);
    return false;
  }

  uint8_t violations = 0;
  uint8_t uniqueIdeas = 0;
  uint32_t signatures[16]{};

  for (uint8_t index = 0; index < compatibility.count; ++index) {
    const RhythmCompatibilityCandidate candidate = compatibility.candidates[index];
    const RhythmArchetype* archetype = archetypeForId(candidate.archetypeId);
    const HousePulseWitness witness = witnessForArchetype(candidate.archetypeId);
    if (witness == HousePulseWitness::None) ++violations;

    uint32_t signature = 0;
    if (archetype != nullptr) signature = structuralIdeaSignature(*archetype);
    bool seen = false;
    for (uint8_t prior = 0; prior < uniqueIdeas; ++prior) {
      if (signatures[prior] == signature) {
        seen = true;
        break;
      }
    }
    if (!seen && uniqueIdeas < 16) signatures[uniqueIdeas++] = signature;

    std::printf(
        "G4_I5_HOUSE_CANDIDATE archetype=%u weight=%u quarter_pulse=%u signature=%08x\n",
        candidate.archetypeId, candidate.weight,
        witness == HousePulseWitness::QuarterPulse ? 1u : 0u,
        static_cast<unsigned>(signature));
  }

  std::printf(
      "G4_I5_HOUSE_ADMISSION candidates=%u violations=%u structural_ideas=%u\n",
      compatibility.count, violations, uniqueIdeas);

  if (violations != 0) {
    std::puts("G4_I5_FAIL I5_HOUSE_UNOWNED_IDEA");
    return false;
  }
  if (compatibility.count < 4 || uniqueIdeas < 4) {
    std::puts("G4_I5_FAIL I5_HOUSE_IDEA_COLLAPSE");
    return false;
  }

  std::puts("G4_I5_PASS house_admission_ownership");
  return true;
}

StepMask drumOnsets(const DrumPatternSet& drums, uint8_t voice) {
  StepMask result = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[voice].steps[step].hit) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
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

bool checkMaterializedHouseOwnership() {
  const GenreSettings settings = houseSettings();
  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };

  uint16_t ready = 0;
  uint16_t violations = 0;
  uint16_t distinctArchetypes = 0;
  RhythmArchetypeId observed[16]{};

  for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
    for (RealizationLevel level : levels) {
      const StrongRhythmMigrationContext context = contextFor(level);
      StrongRhythmFrozenSelection selection{};
      const StrongRhythmMigrationResult selected =
          resolveStrongRhythmFrozenSelection(settings, context, identity, selection);
      if (selected.status != StrongRhythmMigrationStatus::Applied ||
          !selection.resolved) {
        std::printf(
            "G4_I5_FAIL I5_HOUSE_RESOLUTION identity=%u level=%u status=%u\n",
            identity, static_cast<unsigned>(level),
            static_cast<unsigned>(selected.status));
        return false;
      }

      bool seen = false;
      for (uint16_t index = 0; index < distinctArchetypes; ++index) {
        if (observed[index] == selection.composition.rhythmArchetypeId) {
          seen = true;
          break;
        }
      }
      if (!seen && distinctArchetypes < 16) {
        observed[distinctArchetypes++] = selection.composition.rhythmArchetypeId;
      }

      DrumPatternSet drums{};
      SynthPattern synthA{};
      SynthPattern synthB{};
      const StrongRhythmMigrationResult materialized =
          migrateStrongRhythmFrozenMaterial(
              settings, selection, context, drums, synthA, synthB);
      if (materialized.status != StrongRhythmMigrationStatus::Applied) {
        std::printf(
            "G4_I5_FAIL I5_HOUSE_MATERIALIZE identity=%u level=%u status=%u\n",
            identity, static_cast<unsigned>(level),
            static_cast<unsigned>(materialized.status));
        return false;
      }
      ++ready;

      const StepMask kick = drumOnsets(drums, KICK);
      if (witnessForMasks(kick) == HousePulseWitness::None) {
        if (violations < 12) {
          std::printf(
              "G4_I5_WITNESS I5_HOUSE_MATERIALIZED_OWNERSHIP identity=%u level=%u archetype=%u kick=%04x\n",
              identity, static_cast<unsigned>(level),
              selection.composition.rhythmArchetypeId, kick);
        }
        ++violations;
      }
    }
  }

  std::printf(
      "G4_I5_HOUSE_MATERIALIZED ready=%u violations=%u archetypes=%u\n",
      ready, violations, distinctArchetypes);

  if (ready != static_cast<uint16_t>(kIdentityCount * 3u) ||
      violations != 0) {
    std::puts("G4_I5_FAIL I5_HOUSE_MATERIALIZED_OWNERSHIP");
    return false;
  }
  if (distinctArchetypes < 4) {
    std::puts("G4_I5_FAIL I5_HOUSE_MATERIALIZED_IDEA_COLLAPSE");
    return false;
  }

  std::puts("G4_I5_PASS house_materialized_ownership");
  return true;
}

}  // namespace

int main() {
  bool ok = true;
  ok = checkAdmittedHouseOwnership() && ok;
  ok = checkMaterializedHouseOwnership() && ok;
  if (!ok) {
    std::puts("G4-I5 House structural ownership: FAIL");
    return 1;
  }
  std::puts("G4-I5 House structural ownership: PASS");
  return 0;
}
