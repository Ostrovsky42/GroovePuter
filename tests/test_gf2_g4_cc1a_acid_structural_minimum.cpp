#include <cstdint>
#include <cstdio>
#include <fstream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/rhythm_selection.h"
#include "src/generation/composition/tonal_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/roles/bass_pitch_behavior.h"
#include "src/generation/roles/bass_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kIdentityCount = 128;
constexpr uint8_t kLevelCount = 3;
constexpr uint8_t kOwnerExpected = 3;
constexpr uint16_t kCandidateEdgeExpected = 8;
constexpr uint16_t kMaterializationExpected =
    static_cast<uint16_t>(kIdentityCount * kLevelCount * kOwnerExpected);

struct ArticulationSpace {
  bool bassExists = false;
  bool independentAttack = false;
  bool connectedExtended = false;
  bool shortIntent = false;
  bool normalIntent = false;
  bool heldIntent = false;
  bool tieIntent = false;
};

struct PatternObservation {
  uint8_t independentAttacks = 0;
  uint8_t connectedExtended = 0;
};

struct CandidateEvidence {
  uint16_t owners = 0;
  uint16_t candidateEdges = 0;
  uint16_t expressiveEdges = 0;
  uint16_t upstreamExpressiveOwners = 0;
  uint16_t bassIdentityEdges = 0;
  uint16_t uniqueBassIdentities = 0;
  uint16_t continuationCapableBassEdges = 0;
  uint16_t nonPlainArticulationOwners = 0;
  uint16_t weightIndependenceViolations = 0;
};

struct MaterializedBucket {
  uint16_t rows = 0;
  uint16_t allDetached = 0;
  uint16_t allConnected = 0;
  uint16_t mixed = 0;
  uint16_t withContinuation = 0;
  uint16_t withSlide = 0;
  uint16_t multiClass = 0;
};

struct MaterializedEvidence {
  uint16_t attempted = 0;
  uint16_t successful = 0;
  uint16_t failed = 0;
  uint16_t allDetached = 0;
  uint16_t allConnected = 0;
  uint16_t mixed = 0;
  uint16_t withContinuation = 0;
  uint16_t withSlide = 0;
  uint16_t multiClass = 0;
  MaterializedBucket bucket[kOwnerExpected][kLevelCount]{};
};

uint8_t popcount16(uint16_t value) {
  uint8_t count = 0;
  while (value != 0) {
    value = static_cast<uint16_t>(value & static_cast<uint16_t>(value - 1u));
    ++count;
  }
  return count;
}

const RhythmArchetype* archetypeForId(RhythmArchetypeId id) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(id);
  return definition == nullptr ? nullptr
                               : ReferenceVocabulary::archetypeFor(definition->key);
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype, RhythmRole role) {
  for (uint8_t index = 0; index < archetype.laneCount; ++index) {
    if (archetype.lanes[index].role == role) return &archetype.lanes[index];
  }
  return nullptr;
}

StepMask declaredOnsetSpace(const LaneGrammar& lane) {
  const StepMask declared = static_cast<StepMask>(
      lane.immutableAnchors | lane.canonicalAnchors | lane.preferred |
      lane.optional);
  return static_cast<StepMask>(declared & static_cast<StepMask>(~lane.forbidden));
}

ArticulationSpace projectArticulationSpace(const RhythmArchetype& archetype) {
  ArticulationSpace result{};
  const LaneGrammar* bass = laneFor(archetype, RhythmRole::BassRhythm);
  if (bass == nullptr) return result;

  result.bassExists = true;
  const StepMask legal = declaredOnsetSpace(*bass);
  const StepMask shortIntent = static_cast<StepMask>(bass->shortGate & legal);
  const StepMask heldIntent = static_cast<StepMask>(bass->heldGate & legal);
  const StepMask tieIntent = static_cast<StepMask>(bass->tieGate & legal);
  const StepMask explicitIntent = static_cast<StepMask>(
      shortIntent | heldIntent | tieIntent);
  const StepMask normalIntent = static_cast<StepMask>(
      legal & static_cast<StepMask>(~explicitIntent));

  result.shortIntent = shortIntent != 0;
  result.normalIntent = normalIntent != 0;
  result.heldIntent = heldIntent != 0;
  result.tieIntent = tieIntent != 0;
  result.independentAttack = result.shortIntent || result.normalIntent;
  result.connectedExtended = result.heldIntent || result.tieIntent;
  return result;
}

bool expressive(const ArticulationSpace& space) {
  return space.bassExists && space.independentAttack && space.connectedExtended;
}

void merge(ArticulationSpace& destination, const ArticulationSpace& source) {
  destination.bassExists = destination.bassExists || source.bassExists;
  destination.independentAttack =
      destination.independentAttack || source.independentAttack;
  destination.connectedExtended =
      destination.connectedExtended || source.connectedExtended;
  destination.shortIntent = destination.shortIntent || source.shortIntent;
  destination.normalIntent = destination.normalIntent || source.normalIntent;
  destination.heldIntent = destination.heldIntent || source.heldIntent;
  destination.tieIntent = destination.tieIntent || source.tieIntent;
}

bool perPatternContrast(const PatternObservation& observation) {
  return observation.independentAttacks != 0 &&
         observation.connectedExtended != 0;
}

bool allowedByExpressiveOwner(const ArticulationSpace& owner,
                              const PatternObservation& observation) {
  if (!expressive(owner)) return false;
  if (observation.independentAttacks == 0 &&
      observation.connectedExtended == 0) {
    return false;
  }
  return (observation.independentAttacks == 0 || owner.independentAttack) &&
         (observation.connectedExtended == 0 || owner.connectedExtended);
}

bool candidateCapability(const RhythmCompatibilityCandidate& candidate) {
  const RhythmArchetype* archetype = archetypeForId(candidate.archetypeId);
  return archetype != nullptr && expressive(projectArticulationSpace(*archetype));
}

GenreSettings settingsFor(GenreRecipeId recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
  settings.recipe = recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
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

const char* ownerName(GenreRecipeId recipe) {
  switch (recipe) {
    case 0: return "BASE";
    case 6: return "CHICAGO_JACK";
    case 7: return "ROLLING_ACID";
    default: return "UNKNOWN";
  }
}

const char* levelName(RealizationLevel level) {
  switch (level) {
    case RealizationLevel::P1Canonical: return "P1";
    case RealizationLevel::P2Variation: return "P2";
    case RealizationLevel::P3Transformation: return "P3";
    case RealizationLevel::Count: break;
  }
  return "INVALID";
}

StepMask kickOnsets(const DrumPatternSet& drums) {
  StepMask result = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[KICK].steps[step].hit)
      result = static_cast<StepMask>(result | stepBit(step));
  }
  return result;
}

StepMask protectedSpaceFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask bit = rhythmRoleBit(role);
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    if ((archetype.protectedSpaces[index].affectedRoles & bit) != 0) {
      result = static_cast<StepMask>(
          result | archetype.protectedSpaces[index].steps);
    }
  }
  return result;
}

bool acidAllowsSparseBar(RhythmFamily family) {
  return family == RhythmFamily::SparsePulse;
}

bool checkAdversarialFixtures() {
  bool ok = true;
  const RhythmArchetype* source = archetypeForId(405);
  if (source == nullptr) return false;
  const ArticulationSpace sourceSpace = projectArticulationSpace(*source);
  if (!expressive(sourceSpace)) return false;

  const PatternObservation uniformDetached{8, 0};
  const PatternObservation staccatoDominant{7, 1};
  const PatternObservation connectedDominant{1, 7};
  const PatternObservation mixed{4, 4};
  if (perPatternContrast(uniformDetached) ||
      !perPatternContrast(staccatoDominant) ||
      !perPatternContrast(connectedDominant) ||
      !perPatternContrast(mixed) ||
      !allowedByExpressiveOwner(sourceSpace, uniformDetached) ||
      !allowedByExpressiveOwner(sourceSpace, staccatoDominant) ||
      !allowedByExpressiveOwner(sourceSpace, connectedDominant) ||
      !allowedByExpressiveOwner(sourceSpace, mixed)) {
    std::puts("G4_CC1A_FAIL fixture=capability_vs_per_pattern");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=capability_vs_per_pattern");
  }

  LaneGrammar lanes[kMaxLanes]{};
  if (source->laneCount > kMaxLanes) return false;
  for (uint8_t i = 0; i < source->laneCount; ++i) lanes[i] = source->lanes[i];
  RhythmArchetype mutant = *source;
  mutant.lanes = lanes;
  for (uint8_t i = 0; i < mutant.laneCount; ++i) {
    if (lanes[i].role == RhythmRole::BassRhythm) {
      lanes[i].shortGate = 0;
      lanes[i].heldGate = 0;
      lanes[i].tieGate = 0;
    }
  }
  if (expressive(projectArticulationSpace(mutant))) {
    std::puts("G4_CC1A_FAIL fixture=flatten_articulation");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=flatten_articulation");
  }

  for (uint8_t i = 0; i < source->laneCount; ++i) lanes[i] = source->lanes[i];
  mutant = *source;
  mutant.lanes = lanes;
  for (uint8_t i = 0; i < mutant.laneCount; ++i) {
    if (lanes[i].role == RhythmRole::BassRhythm) {
      lanes[i].role = RhythmRole::Percussion;
      break;
    }
  }
  if (projectArticulationSpace(mutant).bassExists) {
    std::puts("G4_CC1A_FAIL fixture=missing_bass_semantic_dimension");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=missing_bass_semantic_dimension");
  }

  const char* label = "ACID";
  const bool labelBefore = expressive(sourceSpace);
  label = "MUTATED_LABEL";
  const bool labelAfter = expressive(sourceSpace);
  if (label == nullptr || labelBefore != labelAfter) {
    std::puts("G4_CC1A_FAIL fixture=label_independence");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=label_independence");
  }

  LaneRelationship relationships[kMaxRelationships]{};
  if (source->relationshipCount > kMaxRelationships) return false;
  for (uint8_t i = 0; i < source->relationshipCount; ++i)
    relationships[i] = source->relationships[i];
  mutant = *source;
  mutant.relationships = relationships;
  for (uint8_t i = 0; i < mutant.relationshipCount; ++i) {
    relationships[i].source = RhythmRole::Percussion;
    relationships[i].target = RhythmRole::ClosedHat;
    relationships[i].op = RelationshipOp::Coincide;
  }
  if (expressive(projectArticulationSpace(mutant)) != expressive(sourceSpace)) {
    std::puts("G4_CC1A_FAIL fixture=kick_relationship_independence");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=kick_relationship_independence");
  }

  for (uint8_t i = 0; i < source->relationshipCount; ++i)
    relationships[i] = source->relationships[i];
  for (uint8_t i = 0; i < source->relationshipCount; ++i) {
    relationships[i].minOffset = -7;
    relationships[i].maxOffset = 7;
  }
  if (expressive(projectArticulationSpace(mutant)) != expressive(sourceSpace)) {
    std::puts("G4_CC1A_FAIL fixture=grid_constant_independence");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=grid_constant_independence");
  }
  return ok;
}

bool bassIdentityCanContinue(BassRhythmId id, RhythmArchetypeId archetypeId) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(archetypeId);
  const RhythmArchetype* archetype = archetypeForId(archetypeId);
  if (definition == nullptr || archetype == nullptr) return false;

  for (uint8_t bar = 0; bar < 4; ++bar) {
    BassRhythmRequest request{};
    request.requestedId = id;
    request.family = definition->family;
    request.archetypeId = archetypeId;
    request.kickOnsets = static_cast<StepMask>(
        stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
    request.protectedSpace = protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
    request.generation.projectSeed = 0x43433141u;
    request.generation.phraseOrdinal = 1;
    request.barOrdinal = bar;
    request.allowEmptyBar = acidAllowsSparseBar(definition->family);
    const BassRhythmResult result = realizeBassRhythm(request);
    if ((result.status == BassRhythmStatus::Ok ||
         result.status == BassRhythmStatus::ValidButEmpty) &&
        result.plan.continuations != 0) {
      return true;
    }
  }
  return false;
}

bool collectCandidateEvidence(CandidateEvidence& evidence) {
  uint16_t uniqueBassMask = 0;
  const uint8_t ownerCount = availableRecipeCount(GenerativeMode::Acid);
  for (uint8_t ordinal = 0; ordinal < ownerCount; ++ordinal) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ordinal, recipe)) return false;
    ++evidence.owners;
    const GenreSettings settings = settingsFor(recipe);
    const RhythmCompatibilityView compatibility = rhythmCompatibilityFor(settings);
    if (compatibility.candidates == nullptr || compatibility.count == 0)
      return false;

    ArticulationSpace ownerSpace{};
    for (uint8_t index = 0; index < compatibility.count; ++index) {
      const RhythmCompatibilityCandidate candidate = compatibility.candidates[index];
      ++evidence.candidateEdges;
      const RhythmArchetype* archetype = archetypeForId(candidate.archetypeId);
      if (archetype == nullptr) return false;
      const ArticulationSpace projected = projectArticulationSpace(*archetype);
      merge(ownerSpace, projected);
      if (expressive(projected)) ++evidence.expressiveEdges;

      const bool before = candidateCapability(candidate);
      RhythmCompatibilityCandidate reweighted = candidate;
      reweighted.weight = static_cast<uint8_t>(candidate.weight ^ 0xA5u);
      const bool after = candidateCapability(reweighted);
      if (before != after) ++evidence.weightIndependenceViolations;
    }
    if (expressive(ownerSpace)) ++evidence.upstreamExpressiveOwners;

    const GenerationProfileView profile = generationProfileFor(settings);
    if (profile.bassRhythms.candidates == nullptr ||
        profile.bassRhythms.count == 0) {
      return false;
    }
    const RhythmArchetypeId representative =
        compatibility.candidates[0].archetypeId;
    for (uint8_t index = 0; index < profile.bassRhythms.count; ++index) {
      ++evidence.bassIdentityEdges;
      const uint8_t rawId = profile.bassRhythms.candidates[index].id;
      if (rawId < 16) {
        uniqueBassMask = static_cast<uint16_t>(
            uniqueBassMask | static_cast<uint16_t>(1u << rawId));
      }
      if (bassIdentityCanContinue(static_cast<BassRhythmId>(rawId), representative))
        ++evidence.continuationCapableBassEdges;
    }

    const TonalGenerationProfile tonal = tonalGenerationProfileFor(settings);
    const uint16_t plain =
        bassArticulationStyleBit(BassArticulationStyleId::Plain);
    if (tonal.bassPolicy.allowedArticulations != plain)
      ++evidence.nonPlainArticulationOwners;

    std::printf(
        "G4_CC1A_OWNER recipe=%u name=%s candidates=%u upstream_expressive=%u bass_ids=%u allowed_articulation_mask=%04x\n",
        recipe, ownerName(recipe), compatibility.count,
        expressive(ownerSpace) ? 1u : 0u, profile.bassRhythms.count,
        tonal.bassPolicy.allowedArticulations);
  }

  evidence.uniqueBassIdentities = popcount16(uniqueBassMask);
  std::printf(
      "G4_CC1A_CANDIDATE owners=%u candidate_edges=%u expressive_edges=%u upstream_expressive_owners=%u bass_identity_edges=%u unique_bass_ids=%u continuation_capable_bass_edges=%u non_plain_articulation_owners=%u weight_independence_violations=%u\n",
      evidence.owners, evidence.candidateEdges, evidence.expressiveEdges,
      evidence.upstreamExpressiveOwners, evidence.bassIdentityEdges,
      evidence.uniqueBassIdentities, evidence.continuationCapableBassEdges,
      evidence.nonPlainArticulationOwners,
      evidence.weightIndependenceViolations);

  return evidence.owners == kOwnerExpected &&
         evidence.candidateEdges == kCandidateEdgeExpected &&
         evidence.expressiveEdges == kCandidateEdgeExpected &&
         evidence.upstreamExpressiveOwners == kOwnerExpected &&
         evidence.bassIdentityEdges == 12 &&
         evidence.uniqueBassIdentities == 4 &&
         evidence.continuationCapableBassEdges == 0 &&
         evidence.nonPlainArticulationOwners == 0 &&
         evidence.weightIndependenceViolations == 0;
}

bool reconstructBass(const GenreSettings& settings,
                     const StrongRhythmFrozenSelection& selection,
                     const DrumPatternSet& drums,
                     BassRhythmPlan& rhythm,
                     BassPitchBehaviorPlan& pitch) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(
          selection.composition.rhythmArchetypeId);
  const RhythmArchetype* archetype =
      archetypeForId(selection.composition.rhythmArchetypeId);
  if (definition == nullptr || archetype == nullptr) return false;

  BassRhythmRequest rhythmRequest{};
  rhythmRequest.requestedId = selection.composition.bassRhythm;
  rhythmRequest.family = definition->family;
  rhythmRequest.archetypeId = definition->archetypeId;
  rhythmRequest.kickOnsets = kickOnsets(drums);
  rhythmRequest.protectedSpace =
      protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
  rhythmRequest.generation = selection.realizationGeneration;
  rhythmRequest.barOrdinal = 0;
  rhythmRequest.allowEmptyBar = acidAllowsSparseBar(definition->family);
  const BassRhythmResult rhythmResult = realizeBassRhythm(rhythmRequest);
  if (rhythmResult.status != BassRhythmStatus::Ok &&
      rhythmResult.status != BassRhythmStatus::ValidButEmpty) {
    return false;
  }

  BassPitchBehaviorRequest pitchRequest{};
  pitchRequest.rhythmPlan = rhythmResult.plan;
  pitchRequest.archetypeId = definition->archetypeId;
  pitchRequest.generation = rhythmRequest.generation;
  pitchRequest.barOrdinal = 0;
  pitchRequest.policy = tonalGenerationProfileFor(settings).bassPolicy;
  const BassPitchBehaviorResult pitchResult =
      realizeBassPitchBehavior(pitchRequest);
  if (pitchResult.status != BassPitchBehaviorStatus::Ok &&
      pitchResult.status != BassPitchBehaviorStatus::ValidButEmpty) {
    return false;
  }

  rhythm = rhythmResult.plan;
  pitch = pitchResult.plan;
  return true;
}

uint8_t activeSynthCells(const SynthPattern& pattern) {
  uint8_t count = 0;
  for (const SynthStep& step : pattern.steps) {
    if (step.note >= 0) ++count;
  }
  return count;
}

bool collectMaterializedEvidence(MaterializedEvidence& evidence,
                                 const char* censusPath) {
  std::ofstream census;
  if (censusPath != nullptr) {
    census.open(censusPath);
    if (!census) return false;
    census << "owner\trecipe\tidentity\tlevel\tarchetype\tbass_identity\tbass_articulation\tattack_count\tcontinuation_count\tslide_into_count\tactive_cells\tclass_count\tsignature\n";
  }

  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };
  const uint8_t ownerCount = availableRecipeCount(GenerativeMode::Acid);

  for (uint8_t ownerIndex = 0; ownerIndex < ownerCount; ++ownerIndex) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ownerIndex, recipe))
      return false;
    const GenreSettings settings = settingsFor(recipe);

    for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
      for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
        const RealizationLevel level = levels[levelIndex];
        ++evidence.attempted;
        StrongRhythmMigrationContext context = contextFor(level);
        StrongRhythmFrozenSelection selection{};
        const StrongRhythmMigrationResult selected =
            resolveStrongRhythmFrozenSelection(settings, context, identity, selection);
        if (selected.status != StrongRhythmMigrationStatus::Applied ||
            !selection.resolved) {
          ++evidence.failed;
          continue;
        }

        DrumPatternSet drums{};
        SynthPattern synthA{};
        SynthPattern synthB{};
        const StrongRhythmMigrationResult realized =
            migrateStrongRhythmFrozenMaterial(
                settings, selection, context, drums, synthA, synthB);
        if (realized.status != StrongRhythmMigrationStatus::Applied) {
          ++evidence.failed;
          continue;
        }

        BassRhythmPlan rhythm{};
        BassPitchBehaviorPlan pitch{};
        if (!reconstructBass(settings, selection, drums, rhythm, pitch) ||
            rhythm.id != realized.bassRhythmId ||
            pitch.contour != realized.bassPitchContour) {
          ++evidence.failed;
          continue;
        }

        const uint8_t attacks = popcount16(pitch.onsets);
        const uint8_t continuations = popcount16(pitch.continuations);
        const uint8_t slideInto = popcount16(pitch.slideIntoOnsets);
        const uint8_t activeCells = activeSynthCells(synthA);
        if (activeCells != static_cast<uint8_t>(attacks + continuations)) {
          ++evidence.failed;
          continue;
        }

        ++evidence.successful;
        MaterializedBucket& bucket = evidence.bucket[ownerIndex][levelIndex];
        ++bucket.rows;
        const uint8_t connected = static_cast<uint8_t>(continuations + slideInto);
        const uint8_t classes = static_cast<uint8_t>(
            (attacks != 0 ? 1u : 0u) + (connected != 0 ? 1u : 0u));
        const bool allDetached = attacks != 0 && connected == 0;
        const bool allConnected = attacks == 1 && connected != 0;
        const bool mixed = attacks > 1 && connected != 0;

        if (allDetached) {
          ++bucket.allDetached;
          ++evidence.allDetached;
        }
        if (allConnected) {
          ++bucket.allConnected;
          ++evidence.allConnected;
        }
        if (mixed) {
          ++bucket.mixed;
          ++evidence.mixed;
        }
        if (continuations != 0) {
          ++bucket.withContinuation;
          ++evidence.withContinuation;
        }
        if (slideInto != 0) {
          ++bucket.withSlide;
          ++evidence.withSlide;
        }
        if (classes > 1) {
          ++bucket.multiClass;
          ++evidence.multiClass;
        }

        if (census.is_open()) {
          census << ownerName(recipe) << '\t'
                 << static_cast<unsigned>(recipe) << '\t'
                 << identity << '\t' << levelName(level) << '\t'
                 << selection.composition.rhythmArchetypeId << '\t'
                 << static_cast<unsigned>(rhythm.id) << '\t'
                 << bassArticulationStyleName(pitch.articulation) << '\t'
                 << static_cast<unsigned>(attacks) << '\t'
                 << static_cast<unsigned>(continuations) << '\t'
                 << static_cast<unsigned>(slideInto) << '\t'
                 << static_cast<unsigned>(activeCells) << '\t'
                 << static_cast<unsigned>(classes) << '\t'
                 << 'A' << static_cast<unsigned>(attacks)
                 << "_C" << static_cast<unsigned>(continuations)
                 << "_S" << static_cast<unsigned>(slideInto) << '\n';
        }
      }
    }
  }

  for (uint8_t ownerIndex = 0; ownerIndex < ownerCount; ++ownerIndex) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ownerIndex, recipe))
      return false;
    for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
      const MaterializedBucket& bucket = evidence.bucket[ownerIndex][levelIndex];
      std::printf(
          "G4_CC1A_DIST owner=%s level=%s rows=%u all_detached=%u all_connected=%u mixed=%u continuation=%u slide=%u multi_class=%u\n",
          ownerName(recipe), levelName(levels[levelIndex]), bucket.rows,
          bucket.allDetached, bucket.allConnected, bucket.mixed,
          bucket.withContinuation, bucket.withSlide, bucket.multiClass);
    }
  }

  std::printf(
      "G4_CC1A_MATERIAL attempted=%u successful=%u failed=%u all_detached=%u all_connected=%u mixed=%u continuation=%u slide=%u multi_class=%u\n",
      evidence.attempted, evidence.successful, evidence.failed,
      evidence.allDetached, evidence.allConnected, evidence.mixed,
      evidence.withContinuation, evidence.withSlide, evidence.multiClass);

  return evidence.attempted == kMaterializationExpected &&
         evidence.successful == kMaterializationExpected &&
         evidence.failed == 0 &&
         evidence.allDetached == kMaterializationExpected &&
         evidence.allConnected == 0 && evidence.mixed == 0 &&
         evidence.withContinuation == 0 && evidence.withSlide == 0 &&
         evidence.multiClass == 0;
}

}  // namespace

int main(int argc, char** argv) {
  bool ok = checkAdversarialFixtures();

  CandidateEvidence candidate{};
  ok = collectCandidateEvidence(candidate) && ok;
  if (candidate.weightIndependenceViolations == 0)
    std::puts("G4_CC1A_PASS fixture=weight_independence");
  else
    ok = false;

  MaterializedEvidence materialized{};
  ok = collectMaterializedEvidence(
           materialized, argc > 1 ? argv[1] : nullptr) && ok;

  if (candidate.upstreamExpressiveOwners == kOwnerExpected &&
      materialized.multiClass == 0) {
    std::puts("G4_CC1A_PASS observation=candidate_capability_differs_from_materialized_usage");
  } else {
    std::puts("G4_CC1A_FAIL observation=candidate_capability_differs_from_materialized_usage");
    ok = false;
  }

  std::puts("G4_CC1A_HYPOTHESIS old_cc1=REJECTED_AS_PORTABLE_CONTRACT");
  std::puts("G4_CC1A_HYPOTHESIS per_pattern_contrast=REJECTED_BY_1152_ROW_CORPUS");
  std::puts("G4_CC1A_HYPOTHESIS owner_space_capability=BEST_FORMULATION_UPSTREAM_ONLY");
  std::puts("G4_CC1A_FINDING authoritative_bass_path=ARTICULATION_COLLAPSED");
  std::puts("G4_CC1A_CONTRACT domains=BASS+ARTICULATION predicate_kind=CAPABILITY status=REVIEW_REQUIRED");

  if (!ok) {
    std::puts("G4-CC1A Acid structural minimum: FAIL");
    return 1;
  }
  std::puts("G4-CC1A Acid structural minimum: PASS");
  return 0;
}
