#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/rhythm_selection.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kIdentityCount = 128;
constexpr StepMask kTheOne = stepBit(0);

const RhythmArchetype* archetypeForId(RhythmArchetypeId archetypeId) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  if (definition == nullptr) return nullptr;
  return ReferenceVocabulary::archetypeFor(definition->key);
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype, RhythmRole role) {
  for (uint8_t lane = 0; lane < archetype.laneCount; ++lane) {
    if (archetype.lanes[lane].role == role) return &archetype.lanes[lane];
  }
  return nullptr;
}

bool hasKickBassResponseWithinThree(const RhythmArchetype& archetype) {
  for (uint8_t index = 0; index < archetype.relationshipCount; ++index) {
    const LaneRelationship& relationship = archetype.relationships[index];
    if (relationship.source == RhythmRole::Kick &&
        relationship.target == RhythmRole::BassRhythm &&
        relationship.op == RelationshipOp::Respond &&
        relationship.minOffset >= 0 && relationship.maxOffset <= 3) {
      return true;
    }
  }
  return false;
}

bool acidBassArticulationContract(const RhythmArchetype& archetype) {
  const LaneGrammar* bass = laneFor(archetype, RhythmRole::BassRhythm);
  return bass != nullptr && bass->shortGate != 0 && bass->heldGate != 0 &&
         hasKickBassResponseWithinThree(archetype);
}

bool technoStaticProgressionContract(ProgressionId progression) {
  return progression == ProgressionId::StaticModal ||
         progression == ProgressionId::PedalDrone;
}

bool funkTheOneContract(const RhythmArchetype& archetype) {
  const LaneGrammar* kick = laneFor(archetype, RhythmRole::Kick);
  return kick != nullptr && (kick->canonicalAnchors & kTheOne) != 0;
}

GenreSettings settingsFor(GenerativeMode mode, GenreRecipeId recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext migrationContextFor(RealizationLevel level) {
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
  StepMask result = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[voice].steps[step].hit) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

bool checkAdversarialFixtures() {
  bool ok = true;

  const RhythmArchetype* acid = archetypeForId(405);
  if (acid == nullptr || !acidBassArticulationContract(*acid)) {
    std::puts("G4_CC1_FAIL fixture=acid_positive");
    ok = false;
  } else {
    LaneGrammar lanes[8]{};
    if (acid->laneCount > 8) return false;
    for (uint8_t i = 0; i < acid->laneCount; ++i) lanes[i] = acid->lanes[i];
    RhythmArchetype mutant = *acid;
    mutant.lanes = lanes;

    for (uint8_t i = 0; i < mutant.laneCount; ++i) {
      if (lanes[i].role == RhythmRole::BassRhythm) {
        lanes[i].shortGate = 0;
        break;
      }
    }
    if (acidBassArticulationContract(mutant)) {
      std::puts("G4_CC1_FAIL fixture=acid_remove_short_gate");
      ok = false;
    }

    for (uint8_t i = 0; i < acid->laneCount; ++i) lanes[i] = acid->lanes[i];
    for (uint8_t i = 0; i < mutant.laneCount; ++i) {
      if (lanes[i].role == RhythmRole::BassRhythm) {
        lanes[i].heldGate = 0;
        break;
      }
    }
    if (acidBassArticulationContract(mutant)) {
      std::puts("G4_CC1_FAIL fixture=acid_remove_held_gate");
      ok = false;
    }

    LaneRelationship relationships[8]{};
    if (acid->relationshipCount > 8) return false;
    for (uint8_t i = 0; i < acid->relationshipCount; ++i) {
      relationships[i] = acid->relationships[i];
      if (relationships[i].source == RhythmRole::Kick &&
          relationships[i].target == RhythmRole::BassRhythm &&
          relationships[i].op == RelationshipOp::Respond) {
        relationships[i].target = RhythmRole::Percussion;
      }
    }
    mutant.lanes = acid->lanes;
    mutant.relationships = relationships;
    if (acidBassArticulationContract(mutant)) {
      std::puts("G4_CC1_FAIL fixture=acid_remove_kick_bass_response");
      ok = false;
    }
  }

  if (!technoStaticProgressionContract(ProgressionId::StaticModal) ||
      !technoStaticProgressionContract(ProgressionId::PedalDrone) ||
      technoStaticProgressionContract(ProgressionId::PopCycle) ||
      technoStaticProgressionContract(ProgressionId::BorrowedLift)) {
    std::puts("G4_CC1_FAIL fixture=techno_static_progression_boundary");
    ok = false;
  }

  const RhythmArchetype* funk = archetypeForId(713);
  if (funk == nullptr || !funkTheOneContract(*funk)) {
    std::puts("G4_CC1_FAIL fixture=funk_positive");
    ok = false;
  } else {
    LaneGrammar lanes[8]{};
    if (funk->laneCount > 8) return false;
    for (uint8_t i = 0; i < funk->laneCount; ++i) lanes[i] = funk->lanes[i];
    RhythmArchetype mutant = *funk;
    mutant.lanes = lanes;
    for (uint8_t i = 0; i < mutant.laneCount; ++i) {
      if (lanes[i].role == RhythmRole::Kick) {
        lanes[i].canonicalAnchors = static_cast<StepMask>(
            lanes[i].canonicalAnchors & static_cast<StepMask>(~kTheOne));
        break;
      }
    }
    if (funkTheOneContract(mutant)) {
      std::puts("G4_CC1_FAIL fixture=funk_remove_the_one");
      ok = false;
    }
  }

  if (ok) std::puts("G4_CC1_PASS adversarial_contract_fixtures");
  return ok;
}

bool checkAcidAdmissionContracts() {
  uint8_t ownerCount = 0;
  uint16_t candidateCount = 0;
  uint16_t violations = 0;
  uint16_t weightIndependenceViolations = 0;

  const uint8_t recipeCount = availableRecipeCount(GenerativeMode::Acid);
  for (uint8_t ordinal = 0; ordinal < recipeCount; ++ordinal) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ordinal, recipe)) {
      std::printf("G4_CC1_FAIL acid_recipe_enumeration ordinal=%u\n", ordinal);
      return false;
    }
    ++ownerCount;
    const RhythmCompatibilityView compatibility =
        rhythmCompatibilityFor(settingsFor(GenerativeMode::Acid, recipe));
    if (compatibility.candidates == nullptr || compatibility.count == 0) {
      std::printf("G4_CC1_FAIL acid_empty_admission recipe=%u\n", recipe);
      return false;
    }

    for (uint8_t index = 0; index < compatibility.count; ++index) {
      const RhythmCompatibilityCandidate candidate = compatibility.candidates[index];
      const RhythmArchetype* archetype = archetypeForId(candidate.archetypeId);
      ++candidateCount;
      const bool original = archetype != nullptr &&
                            acidBassArticulationContract(*archetype);
      if (!original) {
        ++violations;
        std::printf(
            "G4_CC1_WITNESS acid_admission recipe=%u archetype=%u weight=%u\n",
            recipe, candidate.archetypeId, candidate.weight);
      }

      RhythmCompatibilityCandidate reweighted = candidate;
      reweighted.weight = static_cast<uint8_t>(candidate.weight ^ 0x5au);
      const RhythmArchetype* reweightedArchetype =
          archetypeForId(reweighted.archetypeId);
      const bool afterWeightMutation = reweightedArchetype != nullptr &&
                                       acidBassArticulationContract(*reweightedArchetype);
      if (original != afterWeightMutation) ++weightIndependenceViolations;
    }
  }

  std::printf(
      "G4_CC1_ACID owners=%u candidates=%u violations=%u weight_independence_violations=%u\n",
      ownerCount, candidateCount, violations, weightIndependenceViolations);
  if (ownerCount != 3 || candidateCount != 8 || violations != 0 ||
      weightIndependenceViolations != 0) {
    std::puts("G4_CC1_FAIL acid_bass_articulation_contract");
    return false;
  }
  std::puts("G4_CC1_PASS acid_bass_articulation_contract");
  return true;
}

bool checkTechnoHarmonicContract() {
  const GenreSettings settings = settingsFor(GenerativeMode::Techno, 0);
  const GenerationProfileView profile = generationProfileFor(settings);
  if (profile.progressions.candidates == nullptr || profile.progressions.count == 0) {
    std::puts("G4_CC1_FAIL techno_empty_progression_space");
    return false;
  }

  uint8_t candidateViolations = 0;
  for (uint8_t index = 0; index < profile.progressions.count; ++index) {
    const ProgressionId progression = static_cast<ProgressionId>(
        profile.progressions.candidates[index].id);
    if (!technoStaticProgressionContract(progression)) ++candidateViolations;

    ChordProgressionRequest request{};
    request.requestedId = progression;
    request.family = RhythmFamily::FourFloor;
    request.generation.projectSeed = 0x54454348u;
    request.generation.phraseOrdinal = index;
    request.harmonicEventCount = kMaxHarmonicEvents;
    request.phraseBars = 1;
    const ChordProgressionResult realized = realizeChordProgression(request);
    if (realized.status != ChordProgressionStatus::ValidButStatic ||
        realized.plan.eventCount != 1) {
      std::printf(
          "G4_CC1_WITNESS techno_progression candidate=%u status=%u events=%u\n",
          static_cast<unsigned>(progression),
          static_cast<unsigned>(realized.status), realized.plan.eventCount);
      ++candidateViolations;
    }
  }

  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };
  uint16_t selected = 0;
  uint16_t selectedViolations = 0;
  for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
    for (RealizationLevel level : levels) {
      StrongRhythmFrozenSelection selection{};
      const StrongRhythmMigrationResult result = resolveStrongRhythmFrozenSelection(
          settings, migrationContextFor(level), identity, selection);
      if (result.status != StrongRhythmMigrationStatus::Applied ||
          !selection.resolved) {
        std::printf(
            "G4_CC1_FAIL techno_resolution identity=%u level=%u status=%u\n",
            identity, static_cast<unsigned>(level),
            static_cast<unsigned>(result.status));
        return false;
      }
      ++selected;
      if (!technoStaticProgressionContract(selection.composition.progression)) {
        ++selectedViolations;
      }
    }
  }

  std::printf(
      "G4_CC1_TECHNO progression_candidates=%u candidate_violations=%u selected=%u selected_violations=%u\n",
      profile.progressions.count, candidateViolations, selected,
      selectedViolations);
  if (profile.progressions.count != 2 || candidateViolations != 0 ||
      selected != kIdentityCount * 3u || selectedViolations != 0) {
    std::puts("G4_CC1_FAIL techno_no_harmonic_motion_contract");
    return false;
  }
  std::puts("G4_CC1_PASS techno_no_harmonic_motion_contract");
  return true;
}

bool checkFunkTheOneContracts() {
  const GenreSettings settings = settingsFor(GenerativeMode::FunkSoul, 0);
  const RhythmCompatibilityView compatibility = rhythmCompatibilityFor(settings);
  if (compatibility.candidates == nullptr || compatibility.count == 0) {
    std::puts("G4_CC1_FAIL funk_empty_admission");
    return false;
  }

  uint8_t admissionViolations = 0;
  uint8_t weightIndependenceViolations = 0;
  for (uint8_t index = 0; index < compatibility.count; ++index) {
    const RhythmCompatibilityCandidate candidate = compatibility.candidates[index];
    const RhythmArchetype* archetype = archetypeForId(candidate.archetypeId);
    const bool original = archetype != nullptr && funkTheOneContract(*archetype);
    if (!original) {
      ++admissionViolations;
      std::printf("G4_CC1_WITNESS funk_admission archetype=%u weight=%u\n",
                  candidate.archetypeId, candidate.weight);
    }

    RhythmCompatibilityCandidate reweighted = candidate;
    reweighted.weight = static_cast<uint8_t>(candidate.weight ^ 0xa5u);
    const RhythmArchetype* reweightedArchetype = archetypeForId(reweighted.archetypeId);
    const bool afterWeightMutation = reweightedArchetype != nullptr &&
                                     funkTheOneContract(*reweightedArchetype);
    if (original != afterWeightMutation) ++weightIndependenceViolations;
  }

  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };
  uint16_t materialized = 0;
  uint16_t materializedViolations = 0;
  for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
    for (RealizationLevel level : levels) {
      const StrongRhythmMigrationContext context = migrationContextFor(level);
      StrongRhythmFrozenSelection selection{};
      const StrongRhythmMigrationResult selected = resolveStrongRhythmFrozenSelection(
          settings, context, identity, selection);
      if (selected.status != StrongRhythmMigrationStatus::Applied ||
          !selection.resolved) {
        std::printf(
            "G4_CC1_FAIL funk_resolution identity=%u level=%u status=%u\n",
            identity, static_cast<unsigned>(level),
            static_cast<unsigned>(selected.status));
        return false;
      }

      const RhythmArchetype* selectedArchetype =
          archetypeForId(selection.composition.rhythmArchetypeId);
      if (selectedArchetype == nullptr || !funkTheOneContract(*selectedArchetype)) {
        ++materializedViolations;
      }

      DrumPatternSet drums{};
      SynthPattern synthA{};
      SynthPattern synthB{};
      const StrongRhythmMigrationResult realized = migrateStrongRhythmFrozenMaterial(
          settings, selection, context, drums, synthA, synthB);
      if (realized.status != StrongRhythmMigrationStatus::Applied) {
        std::printf(
            "G4_CC1_FAIL funk_materialize identity=%u level=%u status=%u\n",
            identity, static_cast<unsigned>(level),
            static_cast<unsigned>(realized.status));
        return false;
      }
      ++materialized;
      const StepMask kick = drumOnsets(drums, KICK);
      if ((kick & kTheOne) == 0) {
        if (materializedViolations < 12) {
          std::printf(
              "G4_CC1_WITNESS funk_the_one identity=%u level=%u archetype=%u kick=%04x\n",
              identity, static_cast<unsigned>(level),
              selection.composition.rhythmArchetypeId, kick);
        }
        ++materializedViolations;
      }
    }
  }

  std::printf(
      "G4_CC1_FUNK candidates=%u admission_violations=%u weight_independence_violations=%u materialized=%u materialized_violations=%u\n",
      compatibility.count, admissionViolations, weightIndependenceViolations,
      materialized, materializedViolations);
  if (compatibility.count != 3 || admissionViolations != 0 ||
      weightIndependenceViolations != 0 || materialized != kIdentityCount * 3u ||
      materializedViolations != 0) {
    std::puts("G4_CC1_FAIL funk_the_one_contract");
    return false;
  }
  std::puts("G4_CC1_PASS funk_the_one_contract");
  return true;
}

}  // namespace

int main() {
  bool ok = true;
  ok = checkAdversarialFixtures() && ok;
  ok = checkAcidAdmissionContracts() && ok;
  ok = checkTechnoHarmonicContract() && ok;
  ok = checkFunkTheOneContracts() && ok;

  if (!ok) {
    std::puts("G4-CC1 Acid/Techno/Funk contract wave: FAIL");
    return 1;
  }
  std::puts("G4-CC1 Acid/Techno/Funk contract wave: PASS");
  return 0;
}
