#include "strong_rhythm_migration.h"

#include <cstddef>

#include "../generation_context.h"
#include "../rhythm/rhythm_realizer.h"

namespace GroovePuterRhythm {
namespace {

using Archetype = ReferenceVocabulary::Archetype;

struct ArchetypeSet {
  const Archetype* values = nullptr;
  uint8_t count = 0;
};

constexpr Archetype kAcid[] = {
    Archetype::StraightAcid,
    Archetype::RollingAcid,
    Archetype::SyncopatedAcid,
    Archetype::SparseAcid,
};

constexpr Archetype kTechno[] = {
    Archetype::StraightDrive,
    Archetype::OffbeatOpenHat,
    Archetype::HypnoticSparse,
    Archetype::BrokenTechno,
};

constexpr Archetype kRave[] = {
    Archetype::StraightDrive,
    Archetype::OffbeatOpenHat,
    Archetype::BrokenTechno,
    Archetype::ShuffledFourFour,
};

constexpr Archetype kDrumAndBass[] = {
    Archetype::TwoStepRoll,
    Archetype::GhostedRoll,
    Archetype::SparseFastBreak,
    Archetype::HalftimeSwitch,
};

constexpr Archetype kDub[] = {
    Archetype::OneDropSpace,
    Archetype::Steppers,
    Archetype::SparseSkank,
    Archetype::ChordResponse,
};

constexpr Archetype kChicagoJack[] = {
    Archetype::StraightAcid,
    Archetype::RollingAcid,
};

constexpr Archetype kRollingAcid[] = {
    Archetype::RollingAcid,
    Archetype::SyncopatedAcid,
};

constexpr Archetype kDeepChord[] = {
    Archetype::ChordResponse,
    Archetype::SparseSkank,
    Archetype::OneDropSpace,
    Archetype::Steppers,
};

template <size_t N>
constexpr ArchetypeSet archetypeSet(const Archetype (&values)[N]) {
  return ArchetypeSet{values, static_cast<uint8_t>(N)};
}

ArchetypeSet archetypesFor(StrongRhythmRoute route) {
  switch (route) {
    case StrongRhythmRoute::AcidBase:
      return archetypeSet(kAcid);
    case StrongRhythmRoute::TechnoBase:
      return archetypeSet(kTechno);
    case StrongRhythmRoute::RaveBase:
      return archetypeSet(kRave);
    case StrongRhythmRoute::DrumAndBass:
      return archetypeSet(kDrumAndBass);
    case StrongRhythmRoute::DubTechno:
      return archetypeSet(kDub);
    case StrongRhythmRoute::ChicagoJack:
      return archetypeSet(kChicagoJack);
    case StrongRhythmRoute::RollingAcid:
      return archetypeSet(kRollingAcid);
    case StrongRhythmRoute::DeepChord:
      return archetypeSet(kDeepChord);
    case StrongRhythmRoute::Legacy:
    case StrongRhythmRoute::Count:
    default:
      return {};
  }
}

uint32_t mixByte(uint32_t hash, uint8_t value) {
  // Compact FNV-1a-style mixer. This is context construction, not an RNG.
  return (hash ^ static_cast<uint32_t>(value)) * 16777619u;
}

uint32_t projectSeedFor(const GenreSettings& settings,
                        StrongRhythmRoute route) {
  uint32_t hash = 2166136261u;
  hash = mixByte(hash, settings.generativeMode);
  hash = mixByte(hash, settings.recipe);
  hash = mixByte(hash, settings.morphTarget);
  hash = mixByte(hash, settings.morphAmount);
  hash = mixByte(hash, static_cast<uint8_t>(route));
  return hash;
}

bool validLevel(RealizationLevel level) {
  return static_cast<uint8_t>(level) <
         static_cast<uint8_t>(RealizationLevel::Count);
}

RhythmRoleMask deferredSynthRoles() {
  return static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::BassRhythm) |
      rhythmRoleBit(RhythmRole::ChordRhythm) |
      rhythmRoleBit(RhythmRole::MelodicRhythm));
}

}  // namespace

StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings) {
  if (settings.morphAmount > 0 && settings.morphTarget != kBaseRecipeId &&
      settings.morphTarget != settings.recipe) {
    return StrongRhythmRoute::Legacy;
  }

  // Recipe selection is stronger than the base genre. Unsupported recipes must
  // not leak into Stage 5 simply because they map to Acid/Breaks/Dub internally.
  switch (settings.recipe) {
    case 2:  // Drum&Bass
      return StrongRhythmRoute::DrumAndBass;
    case 5:  // Dub Techno
      return StrongRhythmRoute::DubTechno;
    case 6:  // Chicago Jack
      return StrongRhythmRoute::ChicagoJack;
    case 7:  // Rolling Acid
      return StrongRhythmRoute::RollingAcid;
    case 10:  // Deep Chord
      return StrongRhythmRoute::DeepChord;
    case kBaseRecipeId:
      break;
    default:
      return StrongRhythmRoute::Legacy;
  }

  if (settings.generativeMode >= kGenerativeModeCount) {
    return StrongRhythmRoute::Legacy;
  }
  const GenerativeMode mode =
      static_cast<GenerativeMode>(settings.generativeMode);
  switch (mode) {
    case GenerativeMode::Acid:
      return StrongRhythmRoute::AcidBase;
    case GenerativeMode::Darksynth:  // current visible name: Techno
      return StrongRhythmRoute::TechnoBase;
    case GenerativeMode::Rave:
      return StrongRhythmRoute::RaveBase;
    default:
      return StrongRhythmRoute::Legacy;
  }
}

StrongRhythmMigrationResult migrateStrongRhythmDrums(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& destination) {
  StrongRhythmMigrationResult result{};
  result.route = selectStrongRhythmRoute(settings);
  if (result.route == StrongRhythmRoute::Legacy) {
    result.status = StrongRhythmMigrationStatus::Legacy;
    return result;
  }
  if (context.patternAddress < 0 ||
      context.patternAddress >= kMaxGlobalPatterns ||
      !validLevel(context.level)) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const ArchetypeSet choices = archetypesFor(result.route);
  if (choices.values == nullptr || choices.count == 0) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  const uint32_t projectSeed = projectSeedFor(settings, result.route);
  const uint32_t selectionCoordinate =
      (static_cast<uint32_t>(static_cast<uint16_t>(context.patternAddress)) << 8u) |
      static_cast<uint32_t>(static_cast<uint8_t>(result.route));
  const uint8_t choiceIndex = static_cast<uint8_t>(
      deterministicValue(projectSeed, selectionCoordinate) % choices.count);
  result.archetype = choices.values[choiceIndex];

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(result.archetype);
  if (definition == nullptr) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

  RhythmRealizationRequest request{};
  request.catalog = &ReferenceVocabulary::catalog();
  request.archetypeId = definition->archetypeId;
  request.phraseBars = 1;
  request.level = context.level;
  request.generation.projectSeed = projectSeed;
  request.generation.phraseOrdinal =
      static_cast<uint16_t>(context.patternAddress);

  const RhythmRealizationResult realization = realizeRhythmPhrase(request);
  result.realizationStatus = realization.status;
  if (realization.status != RealizationStatus::Ok &&
      realization.status != RealizationStatus::ValidButSparse) {
    result.status = StrongRhythmMigrationStatus::RealizationFailed;
    return result;
  }

  MaterializedPatterns candidate{};
  PatternMaterializationDiagnostics diagnostics{};
  const PatternMaterializerBinding binding =
      standardDrumPatternBinding(deferredSynthRoles());
  result.materializationStatus = materializeRhythmPattern(
      realization.plan, binding, candidate, &diagnostics);
  if (result.materializationStatus != PatternMaterializeStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::MaterializationFailed;
    return result;
  }

  // Stage 5 owns drum event topology, not FEEL or automation. Replace every
  // physical drum voice so unmapped legacy tom/clap events cannot contaminate
  // the relational groove, while preserving legacy lanes and PatternGroove.
  DrumPatternSet next = destination;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    next.voices[voice] = candidate.drums.voices[voice];
  }
  destination = next;

  result.status = StrongRhythmMigrationStatus::Applied;
  return result;
}

}  // namespace GroovePuterRhythm
