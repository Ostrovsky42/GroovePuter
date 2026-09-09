// G4-C0R3/C0R4 — research-only active-axis, semantic-topology, and
// role-plan/native-vocabulary dump.
//
// This tool owns no generation semantics. It supplies the same explicit C0R
// coordinates as C0R1/C0R2, calls the production frozen-selection/migration
// APIs, and observes production-owned plans at their real execution seams.

#include "../../scenes.h"
#include "../../src/dsp/genre_manager.h"
#include "../../src/generation/composition/generation_profile.h"
#include "../../src/generation/migration/strong_rhythm_migration.h"
#include "g4_c0r3_tonal_probe.h"
#include "g4_c0r4_bass_candidates_probe.h"
#include "g4_c0r4_role_plan_probe.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
constexpr const char* kNotObserved = "NOT_OBSERVED";

struct ProfileCase {
  uint16_t ordinal = 0;
  GenerativeMode genre = GenerativeMode::Acid;
  GenreRecipeId recipe = kBaseRecipeId;
  GenreSettings settings{};
  std::string profileId;
  std::string genreName;
  std::string recipeName;
};

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

bool parseUnsignedArgument(const char* text,
                           uint32_t maximum,
                           uint32_t& destination) {
  if (text == nullptr || *text == '\0' || *text == '-') return false;
  char* end = nullptr;
  const unsigned long value = std::strtoul(text, &end, 0);
  if (end == text || *end != '\0' || value > static_cast<unsigned long>(maximum)) {
    return false;
  }
  destination = static_cast<uint32_t>(value);
  return true;
}

bool parseRealizationLevelArgument(const char* text,
                                   RealizationLevel& destination) {
  if (text == nullptr) return false;
  const std::string value(text);
  if (value == "P1") {
    destination = RealizationLevel::P1Canonical;
    return true;
  }
  if (value == "P2") {
    destination = RealizationLevel::P2Variation;
    return true;
  }
  if (value == "P3") {
    destination = RealizationLevel::P3Transformation;
    return true;
  }
  return false;
}

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
    case StrongRhythmMigrationStatus::CompatibilityBindingFailed:
      return "COMPATIBILITY_BINDING_FAILED";
    case StrongRhythmMigrationStatus::FeelApplyFailed: return "FEEL_APPLY_FAILED";
    case StrongRhythmMigrationStatus::Count: return "INVALID";
  }
  return "INVALID";
}

const char* roleName(SemanticSynthBRole role) {
  switch (role) {
    case SemanticSynthBRole::Chord: return "CHORD";
    case SemanticSynthBRole::Melodic: return "MELODIC";
    case SemanticSynthBRole::ChordWithMelodicFill: return "CHORD_WITH_MELODIC_FILL";
    case SemanticSynthBRole::Count: return "INVALID";
  }
  return "INVALID";
}

const char* rhythmFamilyName(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor: return "FOUR_FLOOR";
    case RhythmFamily::MachineSyncopation: return "MACHINE_SYNCOPATION";
    case RhythmFamily::Breakbeat: return "BREAKBEAT";
    case RhythmFamily::UkTwoStep: return "UK_TWO_STEP";
    case RhythmFamily::HipHopBackbeat: return "HIP_HOP_BACKBEAT";
    case RhythmFamily::DubPulse: return "DUB_PULSE";
    case RhythmFamily::Funk16: return "FUNK_16";
    case RhythmFamily::SparsePulse: return "SPARSE_PULSE";
    case RhythmFamily::Count: return "INVALID";
  }
  return "INVALID";
}

std::string hexMask(uint16_t value) {
  std::ostringstream stream;
  stream << "0x" << std::hex << std::setfill('0') << std::setw(4)
         << static_cast<unsigned>(value);
  return stream.str();
}

const char* nativeMembership(uint16_t nativeMask, BassRhythmId selected) {
  const uint8_t ordinal = static_cast<uint8_t>(selected);
  if (ordinal >= 16u) return "OUTSIDE_NATIVE_SET";
  return (nativeMask & (uint16_t{1} << ordinal)) != 0
      ? "NATIVE"
      : "OUTSIDE_NATIVE_SET";
}

SynthPattern pitchSource(int baseNote) {
  SynthPattern pattern{};
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step].note = static_cast<int8_t>(baseNote + (step % 5));
    pattern.steps[step].velocity = static_cast<uint8_t>(88 + (step % 12));
  }
  return pattern;
}

StrongRhythmMigrationContext migrationContext(
    uint16_t identityOrdinal,
    uint32_t generationAttemptOrdinal,
    int16_t patternAddress,
    RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = patternAddress;
  context.level = level;
  context.generationAttemptOrdinal = generationAttemptOrdinal;
  context.phraseGenerationIdentity = identityOrdinal;
  context.feelProfile = FeelProfileId::Auto;
  context.feelAmount = kFeelAmount;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = kRootPitchClass;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

struct ActiveAxisProjection {
  const char* archetype = "ACTIVE";
  const char* bass = "ACTIVE";
  const char* chord = "AUDIBILITY_UNPROVEN";
  const char* melodic = "AUDIBILITY_UNPROVEN";
  const char* motif = "AUDIBILITY_UNPROVEN";
  const char* progression = "ACTIVE";
  const char* phraseLaw = "PLANNING_ONLY";
};

ActiveAxisProjection activeAxes(SemanticSynthBRole role) {
  ActiveAxisProjection projection{};
  switch (role) {
    case SemanticSynthBRole::Chord:
      projection.chord = "ACTIVE";
      projection.melodic = "INACTIVE_BY_PHYSICAL_ROLE";
      projection.motif = "INACTIVE_BY_PHYSICAL_ROLE";
      break;
    case SemanticSynthBRole::Melodic:
      projection.chord = "INACTIVE_BY_PHYSICAL_ROLE";
      projection.melodic = "ACTIVE";
      // F-13 tonal adapter intentionally ignores sourceOrder. Selection exists,
      // but C0R3 must not promote MotifShape to audible causality without proof.
      projection.motif = "AUDIBILITY_UNPROVEN";
      break;
    case SemanticSynthBRole::ChordWithMelodicFill:
      projection.chord = "ACTIVE";
      projection.melodic = "ACTIVE";
      projection.motif = "AUDIBILITY_UNPROVEN";
      break;
    case SemanticSynthBRole::Count:
      projection.chord = "AUDIBILITY_UNPROVEN";
      projection.melodic = "AUDIBILITY_UNPROVEN";
      projection.motif = "AUDIBILITY_UNPROVEN";
      break;
  }
  return projection;
}

struct SemanticTopology {
  StepMask bassAttacks = 0;
  StepMask bassContinuations = 0;
  StepMask secondaryAttacks = 0;
  StepMask secondaryContinuations = 0;
  bool observed = false;
};

SemanticTopology semanticTopology(
    SemanticSynthBRole role,
    const G4C0R3::TonalProbeSnapshot& snapshot) {
  SemanticTopology topology{};
  if (snapshot.overflow || snapshot.callCount < 2) return topology;

  topology.bassAttacks = snapshot.plans[0].onsets;
  topology.bassContinuations = snapshot.plans[0].continuations;

  switch (role) {
    case SemanticSynthBRole::Chord:
    case SemanticSynthBRole::Melodic:
      if (snapshot.callCount != 2) return SemanticTopology{};
      topology.secondaryAttacks = snapshot.plans[1].onsets;
      topology.secondaryContinuations = snapshot.plans[1].continuations;
      break;
    case SemanticSynthBRole::ChordWithMelodicFill:
      if (snapshot.callCount != 3) return SemanticTopology{};
      topology.secondaryAttacks = static_cast<StepMask>(
          snapshot.plans[1].onsets | snapshot.plans[2].onsets);
      topology.secondaryContinuations = static_cast<StepMask>(
          snapshot.plans[1].continuations | snapshot.plans[2].continuations);
      break;
    case SemanticSynthBRole::Count:
      return topology;
  }

  topology.observed = true;
  return topology;
}

void printObservation(const ProfileCase& profile,
                      uint16_t identityOrdinal,
                      uint32_t generationAttemptOrdinal,
                      int16_t patternAddress,
                      RealizationLevel level) {
  StrongRhythmMigrationContext context = migrationContext(
      identityOrdinal, generationAttemptOrdinal, patternAddress, level);
  StrongRhythmFrozenSelection selection{};
  const StrongRhythmMigrationResult selectionResult =
      resolveStrongRhythmFrozenSelection(
          profile.settings, context, identityOrdinal, selection);

  const ReferenceVocabulary::Definition* definition = selection.resolved
      ? ReferenceVocabulary::definitionForId(
            selection.composition.rhythmArchetypeId)
      : nullptr;
  const uint16_t nativeMask = definition == nullptr
      ? 0
      : G4C0R4::nativeBassCandidateMask(definition->family);

  DrumPatternSet drums{};
  SynthPattern synthA = pitchSource(36);
  SynthPattern synthB = pitchSource(60);

  G4C0R3::resetTonalProbe();
  G4C0R4::resetRolePlanProbe();
  StrongRhythmMigrationResult result = selectionResult;
  if (selectionResult.status == StrongRhythmMigrationStatus::Applied &&
      selection.resolved) {
    result = migrateStrongRhythmFrozenMaterial(
        profile.settings, selection, context, drums, synthA, synthB);
  }
  const G4C0R3::TonalProbeSnapshot snapshot = G4C0R3::tonalProbeSnapshot();
  const G4C0R4::RolePlanProbeSnapshot rolePlan = G4C0R4::rolePlanProbeSnapshot();
  const SemanticTopology topology = semanticTopology(result.synthBRole, snapshot);
  const ActiveAxisProjection axes = activeAxes(result.synthBRole);
  const bool accepted = result.status == StrongRhythmMigrationStatus::Applied &&
                        selection.resolved && topology.observed;
  const bool rolePlanObserved = accepted && definition != nullptr &&
                                rolePlan.observed && !rolePlan.overflow &&
                                rolePlan.callCount == 1 &&
                                rolePlan.archetypeId == definition->archetypeId;
  const StepMask planningOnsets = rolePlanObserved
      ? static_cast<StepMask>(rolePlan.bass.structural |
                              rolePlan.bass.secondary |
                              rolePlan.bass.ghosts)
      : 0;

  std::cout
      << "profile_ordinal\tprofile_id\tdepth\tidentity_ordinal\t"
      << "generation_attempt_ordinal\tpattern_address\tmigration_status\t"
      << "selected_archetype\tselected_bass_rhythm\tselected_chord_rhythm\t"
      << "selected_melodic_rhythm\tselected_motif_shape\tselected_progression\t"
      << "synth_b_role\tarchetype_axis_status\tbass_axis_status\t"
      << "chord_axis_status\tmelodic_axis_status\tmotif_axis_status\t"
      << "progression_axis_status\tphrase_law_axis_status\t"
      << "bass_attack_mask\tbass_continuation_mask\tsecondary_attack_mask\t"
      << "secondary_continuation_mask\tsecondary_topology_role\t"
      << "rhythm_family\tbass_native_candidate_mask\tbass_native_membership\t"
      << "planning_bass_onset_mask\tplanning_bass_structural_mask\t"
      << "planning_bass_secondary_mask\tplanning_bass_ghost_mask\n";

  std::cout
      << profile.ordinal << '\t'
      << profile.profileId << '\t'
      << depthName(level) << '\t'
      << identityOrdinal << '\t'
      << generationAttemptOrdinal << '\t'
      << patternAddress << '\t'
      << migrationStatusName(result.status) << '\t'
      << static_cast<unsigned>(result.archetype) << '\t'
      << static_cast<unsigned>(selection.composition.bassRhythm) << '\t'
      << static_cast<unsigned>(selection.composition.chordRhythm) << '\t'
      << static_cast<unsigned>(selection.composition.melodicRhythm) << '\t'
      << static_cast<unsigned>(selection.composition.motifShape) << '\t'
      << static_cast<unsigned>(selection.composition.progression) << '\t'
      << roleName(result.synthBRole) << '\t'
      << axes.archetype << '\t'
      << axes.bass << '\t'
      << axes.chord << '\t'
      << axes.melodic << '\t'
      << axes.motif << '\t'
      << axes.progression << '\t'
      << axes.phraseLaw << '\t'
      << (accepted ? hexMask(topology.bassAttacks) : kNotObserved) << '\t'
      << (accepted ? hexMask(topology.bassContinuations) : kNotObserved) << '\t'
      << (accepted ? hexMask(topology.secondaryAttacks) : kNotObserved) << '\t'
      << (accepted ? hexMask(topology.secondaryContinuations) : kNotObserved) << '\t'
      << roleName(result.synthBRole) << '\t'
      << (definition != nullptr ? rhythmFamilyName(definition->family) : kNotObserved) << '\t'
      << (definition != nullptr ? hexMask(nativeMask) : kNotObserved) << '\t'
      << (definition != nullptr
              ? nativeMembership(nativeMask, selection.composition.bassRhythm)
              : kNotObserved) << '\t'
      << (rolePlanObserved ? hexMask(planningOnsets) : kNotObserved) << '\t'
      << (rolePlanObserved ? hexMask(rolePlan.bass.structural) : kNotObserved) << '\t'
      << (rolePlanObserved ? hexMask(rolePlan.bass.secondary) : kNotObserved) << '\t'
      << (rolePlanObserved ? hexMask(rolePlan.bass.ghosts) : kNotObserved) << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 7 || std::string(argv[1]) != "--g4-c0r3-dump") {
    std::fprintf(
        stderr,
        "usage: %s --g4-c0r3-dump PROFILE IDENTITY ATTEMPT PATTERN_ADDRESS P1|P2|P3\n",
        argv[0]);
    return 2;
  }

  const std::vector<ProfileCase> profiles = enumerateProfiles();
  uint32_t profileOrdinal = 0;
  uint32_t identityOrdinal = 0;
  uint32_t generationAttemptOrdinal = 0;
  uint32_t patternAddress = 0;
  RealizationLevel level = RealizationLevel::Count;
  const bool valid =
      parseUnsignedArgument(argv[2], 0xFFFFu, profileOrdinal) &&
      profileOrdinal < profiles.size() &&
      parseUnsignedArgument(argv[3], 0xFFFEu, identityOrdinal) &&
      parseUnsignedArgument(argv[4], 0xFFFFFFFFu, generationAttemptOrdinal) &&
      parseUnsignedArgument(
          argv[5], static_cast<uint32_t>(kMaxGlobalPatterns - 1), patternAddress) &&
      parseRealizationLevelArgument(argv[6], level);
  if (!valid) {
    std::fprintf(
        stderr,
        "usage: %s --g4-c0r3-dump PROFILE IDENTITY ATTEMPT PATTERN_ADDRESS P1|P2|P3\n",
        argv[0]);
    return 2;
  }

  printObservation(
      profiles[profileOrdinal],
      static_cast<uint16_t>(identityOrdinal),
      generationAttemptOrdinal,
      static_cast<int16_t>(patternAddress),
      level);
  return 0;
}
