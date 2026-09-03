// GF2-C2 Gate B — production-backed materialized musical-capacity dump.
//
// This tool owns no generation policy. It enumerates the production profile
// catalog, executes the real frozen-selection/migration/phrase APIs, wraps the
// existing GF2-C2-V0R GenerationObservation, and emits only research-side
// timbre-free physical evidence plus execution provenance.

#include "../../scenes.h"
#include "../../src/dsp/genre_manager.h"
#include "../../src/generation/composition/generation_profile.h"
#include "../../src/generation/migration/phrase_execution.h"
#include "../../src/generation/migration/strong_rhythm_migration.h"
#include "../../tests/support/gf2_gate_b_observation.h"
#include "../../tests/support/gf2_generation_observation.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

SerialMock Serial;
SDMock SD;

using namespace GroovePuterRhythm;

namespace {

constexpr uint8_t kRootPitchClass = 0;
constexpr uint8_t kFeelAmount = 20;

struct ProfileCase {
  uint16_t ordinal = 0;
  GenerativeMode genre = GenerativeMode::Acid;
  GenreRecipeId recipe = kBaseRecipeId;
  GenreSettings settings{};
  std::string profileId;
  std::string genreName;
  std::string recipeName;
};

const char* depthName(RealizationLevel level) {
  switch (level) {
    case RealizationLevel::P1Canonical: return "P1";
    case RealizationLevel::P2Variation: return "P2";
    case RealizationLevel::P3Transformation: return "P3";
    case RealizationLevel::Count: return "INVALID";
  }
  return "INVALID";
}

const char* migrationStatusName(StrongRhythmMigrationStatus status) {
  switch (status) {
    case StrongRhythmMigrationStatus::Legacy: return "LEGACY";
    case StrongRhythmMigrationStatus::Applied: return "APPLIED";
    case StrongRhythmMigrationStatus::InvalidContext: return "INVALID_CONTEXT";
    case StrongRhythmMigrationStatus::AttemptUnavailable: return "ATTEMPT_UNAVAILABLE";
    case StrongRhythmMigrationStatus::RealizationFailed: return "REALIZATION_FAILED";
    case StrongRhythmMigrationStatus::MaterializationFailed: return "MATERIALIZATION_FAILED";
    case StrongRhythmMigrationStatus::CompatibilityBindingFailed: return "COMPATIBILITY_BINDING_FAILED";
    case StrongRhythmMigrationStatus::FeelApplyFailed: return "FEEL_APPLY_FAILED";
    case StrongRhythmMigrationStatus::Count: return "INVALID";
  }
  return "INVALID";
}

const char* phraseExecutionStatusName(PhraseExecutionStatus status) {
  switch (status) {
    case PhraseExecutionStatus::Ready: return "READY";
    case PhraseExecutionStatus::Rejected: return "REJECTED";
    case PhraseExecutionStatus::InvalidContext: return "INVALID_CONTEXT";
    case PhraseExecutionStatus::ProgressionSourceFailure: return "PROGRESSION_SOURCE_FAILURE";
    case PhraseExecutionStatus::HarmonicProjectionFailure: return "HARMONIC_PROJECTION_FAILURE";
    case PhraseExecutionStatus::SemanticProbeFailure: return "SEMANTIC_PROBE_FAILURE";
    case PhraseExecutionStatus::SemanticContractFailure: return "SEMANTIC_CONTRACT_FAILURE";
    case PhraseExecutionStatus::Count: return "INVALID";
  }
  return "INVALID";
}

const char* phraseLengthStatusName(PhraseLengthRequestStatus status) {
  switch (status) {
    case PhraseLengthRequestStatus::Accepted: return "ACCEPTED";
    case PhraseLengthRequestStatus::Rejected: return "REJECTED";
    case PhraseLengthRequestStatus::Count: return "INVALID";
  }
  return "INVALID";
}

const char* phraseRejectReasonName(PhraseLengthRejectReason reason) {
  switch (reason) {
    case PhraseLengthRejectReason::None: return "NONE";
    case PhraseLengthRejectReason::InvalidPhraseLengthDomain: return "INVALID_PHRASE_LENGTH_DOMAIN";
    case PhraseLengthRejectReason::NoAdmissibleLawForRequestedLength: return "NO_ADMISSIBLE_LAW";
    case PhraseLengthRejectReason::CompositionResolutionFailed: return "COMPOSITION_RESOLUTION_FAILED";
    case PhraseLengthRejectReason::Count: return "INVALID";
  }
  return "INVALID";
}

const char* barFunctionName(BarFunction function) {
  switch (function) {
    case BarFunction::Statement: return "STATEMENT";
    case BarFunction::Repeat: return "REPEAT";
    case BarFunction::RepeatWithGhosts: return "REPEAT_WITH_GHOSTS";
    case BarFunction::Response: return "RESPONSE";
    case BarFunction::Reduction: return "REDUCTION";
    case BarFunction::Build: return "BUILD";
    case BarFunction::Turnaround: return "TURNAROUND";
    case BarFunction::Break: return "BREAK";
    case BarFunction::Return: return "RETURN";
    case BarFunction::Count: return "INVALID";
  }
  return "INVALID";
}

const char* provenanceName(GF2Measurement::MaterialProvenance provenance) {
  switch (provenance) {
    case GF2Measurement::MaterialProvenance::RequestedOperationAccepted:
      return "REQUESTED_OPERATION_ACCEPTED";
    case GF2Measurement::MaterialProvenance::PreviousMaterialRetained:
      return "PREVIOUS_MATERIAL_RETAINED";
    case GF2Measurement::MaterialProvenance::NonAcceptedMaterialChanged:
      return "NON_ACCEPTED_MATERIAL_CHANGED";
  }
  return "INVALID";
}

std::string hex16(uint16_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(4)
         << static_cast<unsigned>(value);
  return stream.str();
}

SynthPattern pitchSource(int baseNote) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(baseNote + (step % 5));
    pattern.steps[step].velocity = static_cast<uint8_t>(88 + (step % 12));
  }
  return pattern;
}

std::vector<uint32_t> loadSeeds(const char* path) {
  std::ifstream input(path);
  if (!input) {
    std::fprintf(stderr, "cannot open seed corpus: %s\n", path);
    std::exit(2);
  }
  std::string line;
  std::getline(input, line);
  if (line != "seed") {
    std::fprintf(stderr, "invalid seed header: %s\n", line.c_str());
    std::exit(2);
  }
  std::vector<uint32_t> seeds;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    std::size_t consumed = 0;
    const unsigned long value = std::stoul(line, &consumed, 0);
    if (consumed != line.size() || value > 0xFFFFFFFFul) {
      std::fprintf(stderr, "invalid seed: %s\n", line.c_str());
      std::exit(2);
    }
    seeds.push_back(static_cast<uint32_t>(value));
  }
  return seeds;
}

std::vector<ProfileCase> enumerateProfiles() {
  std::vector<ProfileCase> profiles;
  uint16_t ordinal = 0;
  for (int genreIndex = 0; genreIndex < kGenerativeModeCount; ++genreIndex) {
    const auto genre = static_cast<GenerativeMode>(genreIndex);
    const uint8_t count = availableRecipeCount(genre);
    for (uint8_t recipeOrdinal = 0; recipeOrdinal < count; ++recipeOrdinal) {
      GenreRecipeId recipe = kBaseRecipeId;
      if (!availableRecipeAt(genre, recipeOrdinal, recipe)) {
        std::fprintf(stderr, "production catalog enumeration failed genre=%d ordinal=%u\n",
                     genreIndex, static_cast<unsigned>(recipeOrdinal));
        std::exit(3);
      }
      ProfileCase value{};
      value.ordinal = ordinal++;
      value.genre = genre;
      value.recipe = recipe;
      value.settings.generativeMode = static_cast<uint8_t>(genreIndex);
      value.settings.recipe = static_cast<uint8_t>(recipe);
      value.settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
      value.settings.rhythmArchetypeId = kNoArchetypeId;
      value.genreName = GenreCatalog::generativeModeName(genre);
      value.recipeName = recipe == kBaseRecipeId ? "BASE" : GenreCatalog::recipeName(recipe);
      value.profileId = value.genreName + std::string("/") + value.recipeName;
      profiles.push_back(value);
    }
  }
  return profiles;
}

StrongRhythmMigrationContext migrationContext(uint32_t seed,
                                               RealizationLevel level,
                                               uint16_t phraseIdentity) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = static_cast<int16_t>(seed % static_cast<uint32_t>(kMaxGlobalPatterns));
  context.level = level;
  context.generationAttemptOrdinal = seed;
  context.phraseGenerationIdentity = phraseIdentity;
  context.feelProfile = FeelProfileId::Auto;
  context.feelAmount = kFeelAmount;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = kRootPitchClass;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

PhraseExecutionMaterializationSettings phraseMaterializationSettings(
    uint32_t seed, RealizationLevel level) {
  PhraseExecutionMaterializationSettings settings{};
  settings.level = level;
  settings.generationAttemptOrdinal = seed;
  settings.feelProfile = FeelProfileId::Auto;
  settings.feelAmount = kFeelAmount;
  settings.tonalMaterializationEnabled = true;
  settings.rootPitchClass = kRootPitchClass;
  settings.scaleTypeValue = kScaleDorian;
  return settings;
}

std::string phraseFallback(const PreparedPhraseExecution& prepared) {
  if (prepared.status == PhraseExecutionStatus::Ready) return "NONE";
  if (prepared.status == PhraseExecutionStatus::Rejected) {
    return phraseRejectReasonName(prepared.length.rejectReason);
  }
  return phraseExecutionStatusName(prepared.status);
}

struct PhraseObservation {
  std::string executionStatus = "NOT_OBSERVED";
  std::string lengthStatus = "NOT_OBSERVED";
  std::string rejectReason = "NOT_OBSERVED";
  std::string trajectory = "NOT_OBSERVED";
  std::string admitted = "NO";
  std::string actualPlanBars = "NOT_OBSERVED";
  std::string fallback = "NOT_OBSERVED";
  std::string materializationStatus = "NOT_OBSERVED";
  std::string material;
};

PhraseObservation observePhrase(const ProfileCase& profile,
                                uint32_t seed,
                                RealizationLevel level,
                                uint16_t phraseIdentity,
                                uint8_t requestedBars) {
  PhraseObservation observation{};
  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution prepared{};
  const PhraseExecutionStatus status = preparePhraseExecution(
      profile.settings, phraseMaterializationSettings(seed, level),
      phraseIdentity, requestedBars, scratch, prepared);
  observation.executionStatus = phraseExecutionStatusName(status);
  observation.lengthStatus = phraseLengthStatusName(prepared.length.status);
  observation.rejectReason = phraseRejectReasonName(prepared.length.rejectReason);
  observation.admitted = status == PhraseExecutionStatus::Ready ? "YES" : "NO";
  observation.fallback = phraseFallback(prepared);
  if (prepared.phraseTrajectory == kNoTrajectoryId) {
    observation.trajectory = "NONE";
  } else {
    observation.trajectory = std::to_string(static_cast<unsigned>(prepared.phraseTrajectory));
  }
  if (status != PhraseExecutionStatus::Ready) return observation;

  observation.actualPlanBars = std::to_string(
      static_cast<unsigned>(prepared.length.effectivePhraseBars));
  bool allApplied = true;
  std::ostringstream material;
  for (uint8_t bar = 0; bar < prepared.length.effectivePhraseBars; ++bar) {
    DrumPatternSet drums{};
    SynthPattern synthA = pitchSource(36);
    SynthPattern synthB = pitchSource(60);
    const int16_t physicalAddress = static_cast<int16_t>(
        (static_cast<uint32_t>(seed % static_cast<uint32_t>(kMaxGlobalPatterns)) + bar) %
        static_cast<uint32_t>(kMaxGlobalPatterns));
    const StrongRhythmMigrationResult result = materializePreparedPhraseBar(
        prepared, bar, physicalAddress, drums, synthA, synthB);
    if (bar != 0) material << '|';
    material << static_cast<unsigned>(bar) << '@' << barFunctionName(result.phraseBarFunction) << '@';
    if (result.status != StrongRhythmMigrationStatus::Applied) {
      allApplied = false;
      material << "FAILED:" << migrationStatusName(result.status);
      continue;
    }
    const GF2GateB::NeutralMaterialObservation neutral =
        GF2GateB::observeNeutralMaterial(drums, synthA, synthB, result, kRootPitchClass);
    material << GF2GateB::compactNeutralMaterial(neutral);
  }
  observation.materializationStatus = allApplied ? "ALL_APPLIED" : "FAILED";
  observation.material = material.str();
  return observation;
}

void printHeader() {
  std::cout
      << "profile_ordinal\tprofile_id\tgenre\trecipe\tseed\tdepth\t"
      << "selection_status\tmigration_status\tv0r_provenance\tv0r_requested_result_effective\t"
      << "v0r_requested_mode\tv0r_requested_recipe\tv0r_attempt\tv0r_level\t"
      << "v0r_migration_route\tv0r_archetype\tv0r_effective_material_fingerprint\t"
      << "declared_phrase_law\trequested_bars\tphrase_execution_status\tphrase_length_status\t"
      << "phrase_reject_reason\tresolved_trajectory\tphrase_admitted\tactual_plan_bars\tfallback\t"
      << "phrase_materialization_status\tphrase_material\t"
      << "requested_density_intent\tdensity_min\tdensity_max\tresolved_density\tgrid_steps\t"
      << "requested_feel\tresolved_feel\tphysical_duration\t"
      << "kick_onsets\tbackbeat_onsets\that_onsets\tsupport_onsets\t"
      << "kick_accents\tbackbeat_accents\that_accents\tsupport_accents\tdrum_timing\t"
      << "synth_a_onsets\tsynth_b_onsets\tsynth_a_accents\tsynth_b_accents\t"
      << "synth_a_ghosts\tsynth_b_ghosts\tsynth_a_timing\tsynth_b_timing\t"
      << "synth_a_pitch_class\tsynth_b_pitch_class\tsynth_a_contour\tsynth_b_contour\t"
      << "harmonic_event_onsets\tharmonic_event_count\tchord_onsets\tmelodic_fill_onsets\t"
      << "chord_applied\tmelodic_applied\tsynth_b_role\tphysical_event_count\tsilence_mask\n";
}

void printRealization(const ProfileCase& profile,
                      uint32_t seed,
                      RealizationLevel level) {
  const uint16_t phraseIdentity = static_cast<uint16_t>(seed & 0xFFFFu);
  StrongRhythmMigrationContext context = migrationContext(seed, level, phraseIdentity);
  StrongRhythmFrozenSelection selection{};
  const StrongRhythmMigrationResult selectionResult = resolveStrongRhythmFrozenSelection(
      profile.settings, context, phraseIdentity, selection);

  DrumPatternSet drums{};
  SynthPattern synthA = pitchSource(36);
  SynthPattern synthB = pitchSource(60);
  const uint32_t previousFingerprint =
      GF2Measurement::materialFingerprint(drums, synthA, synthB);

  StrongRhythmMigrationResult result = selectionResult;
  if (selectionResult.status == StrongRhythmMigrationStatus::Applied && selection.resolved) {
    result = migrateStrongRhythmFrozenMaterial(
        profile.settings, selection, context, drums, synthA, synthB);
  }
  const uint32_t effectiveFingerprint =
      GF2Measurement::materialFingerprint(drums, synthA, synthB);
  const bool accepted = result.status == StrongRhythmMigrationStatus::Applied;
  const GF2Measurement::GenerationObservation v0r = GF2Measurement::observeGeneration(
      profile.settings, context, result, static_cast<uint8_t>(result.status), accepted,
      previousFingerprint, effectiveFingerprint);

  GF2GateB::NeutralMaterialObservation neutral{};
  if (accepted) {
    neutral = GF2GateB::observeNeutralMaterial(
        drums, synthA, synthB, result, kRootPitchClass);
  }

  std::string declaredLaw = "NOT_OBSERVED";
  std::string requestedBars = "NOT_OBSERVED";
  PhraseObservation phrase{};
  if (selectionResult.status == StrongRhythmMigrationStatus::Applied && selection.resolved) {
    declaredLaw = phraseEvolutionLawName(selection.composition.phraseLaw);
    requestedBars = std::to_string(static_cast<unsigned>(selection.composition.phraseBars));
    phrase = observePhrase(profile, seed, level, phraseIdentity,
                           selection.composition.phraseBars);
  }

  const GenerationCorridor corridor = selection.composition.corridor;
  std::cout
      << profile.ordinal << '\t' << profile.profileId << '\t'
      << profile.genreName << '\t' << profile.recipeName << '\t'
      << "0x" << std::hex << std::setfill('0') << std::setw(8) << seed << std::dec << '\t'
      << depthName(level) << '\t'
      << migrationStatusName(selectionResult.status) << '\t'
      << migrationStatusName(result.status) << '\t'
      << provenanceName(v0r.provenance) << '\t'
      << (v0r.requestedResultEffective ? "YES" : "NO") << '\t'
      << static_cast<unsigned>(v0r.requestedMode) << '\t'
      << static_cast<unsigned>(v0r.requestedRecipe) << '\t'
      << v0r.generationAttemptOrdinal << '\t'
      << static_cast<unsigned>(v0r.realizationLevel) << '\t'
      << static_cast<unsigned>(v0r.migrationRoute) << '\t'
      << static_cast<unsigned>(v0r.migrationArchetype) << '\t'
      << v0r.effectiveMaterialFingerprint << '\t'
      << declaredLaw << '\t' << requestedBars << '\t'
      << phrase.executionStatus << '\t' << phrase.lengthStatus << '\t'
      << phrase.rejectReason << '\t' << phrase.trajectory << '\t'
      << phrase.admitted << '\t' << phrase.actualPlanBars << '\t'
      << phrase.fallback << '\t' << phrase.materializationStatus << '\t'
      << phrase.material << '\t'
      << "PROFILE_CORRIDOR_AUTO" << '\t'
      << (selection.resolved ? std::to_string(static_cast<unsigned>(corridor.densityMin)) : "NOT_OBSERVED") << '\t'
      << (selection.resolved ? std::to_string(static_cast<unsigned>(corridor.densityMax)) : "NOT_OBSERVED") << '\t'
      << (selection.resolved ? std::to_string(static_cast<unsigned>(selection.structuralDensityTarget)) : "NOT_OBSERVED") << '\t'
      << (selection.resolved ? std::to_string(static_cast<unsigned>(corridor.gridSteps)) : "NOT_OBSERVED") << '\t'
      << "AUTO" << '\t'
      << (selection.resolved ? std::to_string(static_cast<unsigned>(selection.resolvedFeel)) : "NOT_OBSERVED") << '\t'
      << "NOT_OBSERVED" << '\t'
      << hex16(neutral.kickOnsets) << '\t' << hex16(neutral.backbeatOnsets) << '\t'
      << hex16(neutral.hatOnsets) << '\t' << hex16(neutral.supportOnsets) << '\t'
      << hex16(neutral.kickAccents) << '\t' << hex16(neutral.backbeatAccents) << '\t'
      << hex16(neutral.hatAccents) << '\t' << hex16(neutral.supportAccents) << '\t'
      << neutral.drumTiming << '\t'
      << hex16(neutral.synthAOnsets) << '\t' << hex16(neutral.synthBOnsets) << '\t'
      << hex16(neutral.synthAAccents) << '\t' << hex16(neutral.synthBAccents) << '\t'
      << hex16(neutral.synthAGhosts) << '\t' << hex16(neutral.synthBGhosts) << '\t'
      << neutral.synthATiming << '\t' << neutral.synthBTiming << '\t'
      << neutral.synthAPitchClass << '\t' << neutral.synthBPitchClass << '\t'
      << neutral.synthAContour << '\t' << neutral.synthBContour << '\t'
      << hex16(neutral.harmonicEventOnsets) << '\t'
      << static_cast<unsigned>(neutral.harmonicEventCount) << '\t'
      << hex16(neutral.chordOnsets) << '\t' << hex16(neutral.melodicFillOnsets) << '\t'
      << (neutral.chordApplied ? "YES" : "NO") << '\t'
      << (neutral.melodicApplied ? "YES" : "NO") << '\t'
      << (accepted ? std::to_string(static_cast<unsigned>(neutral.synthBRole)) : "NOT_OBSERVED") << '\t'
      << (accepted ? std::to_string(static_cast<unsigned>(neutral.physicalEventCount)) : "NOT_OBSERVED") << '\t'
      << (accepted ? hex16(neutral.silenceMask) : "NOT_OBSERVED") << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s tests/support/gf2_gate_b_seeds.tsv\n", argv[0]);
    return 2;
  }

  const std::vector<uint32_t> seeds = loadSeeds(argv[1]);
  const std::vector<ProfileCase> profiles = enumerateProfiles();
  std::cerr << "META\tprofile_count\t" << profiles.size() << '\n';
  std::cerr << "META\tseed_count\t" << seeds.size() << '\n';
  std::cerr << "META\tdepth_count\t3\n";

  printHeader();
  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };
  for (const ProfileCase& profile : profiles) {
    for (uint32_t seed : seeds) {
      for (RealizationLevel level : levels) {
        printRealization(profile, seed, level);
      }
    }
  }
  return 0;
}
