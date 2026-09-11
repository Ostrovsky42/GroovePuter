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
  uint16_t candidateEdgesWithExpressiveSpace = 0;
  uint16_t ownersWithUpstreamExpressiveSpace = 0;
  uint16_t profileBassIdentityEdges = 0;
  uint16_t uniqueProfileBassIdentities = 0;
  uint16_t continuationCapableProfileBassEdges = 0;
  uint16_t ownersWithMoreThanPlainPitchArticulation = 0;
  uint16_t weightIndependenceViolations = 0;
};

struct MaterializedBucket {
  uint16_t rows = 0;
  uint16_t allDetached = 0;
  uint16_t allConnected = 0;
  uint16_t mixed = 0;
  uint16_t withContinuation = 0;
  uint16_t withSlide = 0;
  uint16_t withMultipleClasses = 0;
};

struct MaterializedEvidence {
  uint16_t attempted = 0;
  uint16_t successful = 0;
  uint16_t failed = 0;
  uint16_t rowsWithContinuation = 0;
  uint16_t rowsWithSlide = 0;
  uint16_t rowsWithMultipleClasses = 0;
  uint16_t rowsAllDetached = 0;
  uint16_t rowsAllConnected = 0;
  uint16_t rowsMixed = 0;
  MaterializedBucket buckets[kOwnerExpected][kLevelCount]{};
};

uint8_t popcount16(uint16_t value) {
  uint8_t count = 0;
  while (value != 0) {
    value = static_cast<uint16_t>(value & static_cast<uint16_t>(value - 1u));
    ++count;
  }
  return count;
}

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

bool expressiveArticulationSpace(const ArticulationSpace& space) {
  return space.bassExists && space.independentAttack &&
         space.connectedExtended;
}

void mergeArticulationSpace(ArticulationSpace& destination,
                            const ArticulationSpace& source) {
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

bool patternAllowedByOwnerSpace(const ArticulationSpace& ownerSpace,
                                const PatternObservation& observation) {
  if (!expressiveArticulationSpace(ownerSpace)) return false;
  if (observation.independentAttacks == 0 &&
      observation.connectedExtended == 0) {
    return false;
  }
  if (observation.independentAttacks != 0 && !ownerSpace.independentAttack)
    return false;
  if (observation.connectedExtended != 0 && !ownerSpace.connectedExtended)
    return false;
  return true;
}

bool candidatePassesCapability(const RhythmCompatibilityCandidate& candidate) {
  const RhythmArchetype* archetype = archetypeForId(candidate.archetypeId);
  return archetype != nullptr &&
         expressiveArticulationSpace(projectArticulationSpace(*archetype));
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

StepMask drumKickOnsets(const DrumPatternSet& drums) {
  StepMask result = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[KICK].steps[step].hit) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

StepMask protectedSpaceFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask roleBit = rhythmRoleBit(role);
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    const ProtectedSpace& space = archetype.protectedSpaces[index];
    if ((space.affectedRoles & roleBit) != 0) {
      result = static_cast<StepMask>(result | space.steps);
    }
  }
  return result;
}

bool acidAllowsSparseSemanticBar(RhythmFamily family) {
  return family == RhythmFamily::SparsePulse;
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

bool checkAdversarialProjectionFixtures() {
  bool ok = true;
  const RhythmArchetype* source = archetypeForId(405);
  if (source == nullptr ||
      !expressiveArticulationSpace(projectArticulationSpace(*source))) {
    std::puts("G4_CC1A_FAIL fixture=positive_source_405");
    return false;
  }
  const ArticulationSpace sourceSpace = projectArticulationSpace(*source);

  // H1 and H2 must remain distinct. A single uniform line can be legal inside
  // an owner space that still exposes detached and connected possibilities.
  const PatternObservation uniformDetached{8, 0};
  const PatternObservation staccatoDominant{7, 1};
  const PatternObservation connectedDominant{1, 7};
  const PatternObservation mixed{4, 4};
  if (perPatternContrast(uniformDetached) ||
      !perPatternContrast(staccatoDominant) ||
      !perPatternContrast(connectedDominant) ||
      !perPatternContrast(mixed) ||
      !patternAllowedByOwnerSpace(sourceSpace, uniformDetached) ||
      !patternAllowedByOwnerSpace(sourceSpace, staccatoDominant) ||
      !patternAllowedByOwnerSpace(sourceSpace, connectedDominant) ||
      !patternAllowedByOwnerSpace(sourceSpace, mixed)) {
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

  // Semantic flattening: preserve legal event coordinates but collapse every
  // bass onset to the implicit Normal/re-articulated class.
  for (uint8_t i = 0; i < mutant.laneCount; ++i) {
    if (lanes[i].role != RhythmRole::BassRhythm) continue;
    lanes[i].shortGate = 0;
    lanes[i].heldGate = 0;
    lanes[i].tieGate = 0;
  }
  if (expressiveArticulationSpace(projectArticulationSpace(mutant))) {
    std::puts("G4_CC1A_FAIL fixture=flatten_articulation");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=flatten_articulation");
  }

  // Missing semantic bass dimension is distinct from a flattened bass lane.
  for (uint8_t i = 0; i < source->laneCount; ++i) lanes[i] = source->lanes[i];
  for (uint8_t i = 0; i < mutant.laneCount; ++i) {
    if (lanes[i].role == RhythmRole::BassRhythm) {
      lanes[i].role = RhythmRole::Percussion;
      break;
    }
  }
  const ArticulationSpace missingBass = projectArticulationSpace(mutant);
  if (missingBass.bassExists || expressiveArticulationSpace(missingBass)) {
    std::puts("G4_CC1A_FAIL fixture=missing_bass_semantic_dimension");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=missing_bass_semantic_dimension");
  }

  // Label independence: labels are report-only data and never enter projection.
  const char* label = "ACID";
  const bool beforeLabelMutation = expressiveArticulationSpace(sourceSpace);
  label = "NOT_ACID";
  const bool afterLabelMutation = expressiveArticulationSpace(sourceSpace);
  if (label == nullptr || beforeLabelMutation != afterLabelMutation) {
    std::puts("G4_CC1A_FAIL fixture=label_independence");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=label_independence");
  }

  // Kick relationship and grid-window mutation must not affect the new bass
  // articulation projection.
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
  if (expressiveArticulationSpace(projectArticulationSpace(mutant)) !=
      expressiveArticulationSpace(sourceSpace)) {
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
  if (expressiveArticulationSpace(projectArticulationSpace(mutant)) !=
      expressiveArticulationSpace(sourceSpace)) {
    std::puts("G4_CC1A_FAIL fixture=grid_constant_independence");
    ok = false;
  } else {
    std::puts("G4_CC1A_PASS fixture=grid_constant_independence");
  }

  return ok;
}

bool bassIdentityCanProduceContinuation(BassRhythmId id,
                                        RhythmArchetypeId archetypeId) {
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
    request.allowEmptyBar = acidAllowsSparseSemanticBar(definition->family);
    const BassRhythmResult result = realizeBassRhythm(request);
    if ((result.status == BassRhythmStatus::Ok ||
         result.status == BassRhythmStatus::ValidButEmpty) &&
        result.plan.continuations != 0) {
      return true;
    }
  }
  return false;
}

bool checkCandidateSpace(CandidateEvidence& evidence) {
  bool ok = true;
  uint16_t uniqueBassMask = 0;
  const uint8_t recipeCount = availableRecipeCount(GenerativeMode::Acid);
  for (uint8_t ordinal = 0; ordinal < recipeCount; ++ordinal) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ordinal, recipe)) {
      std::printf("G4_CC1A_FAIL owner_enumeration ordinal=%u\n", ordinal);
      return false;
    }
    ++evidence.owners;
    const GenreSettings settings = settingsFor(recipe);
    const RhythmCompatibilityView compatibility = rhythmCompatibilityFor(settings);
    if (compatibility.candidates == nullptr || compatibility.count == 0) {
      std::printf("G4_CC1A_FAIL empty_admission recipe=%u\n", recipe);
      return false;
    }

    ArticulationSpace ownerSpace{};
    for (uint8_t index = 0; index < compatibility.count; ++index) {
      const RhythmCompatibilityCandidate candidate = compatibility.candidates[index];
      ++evidence.candidateEdges;
      const RhythmArchetype* archetype = archetypeForId(candidate.archetypeId);
      if (archetype == nullptr) return false;
      const ArticulationSpace projected = projectArticulationSpace(*archetype);
      mergeArticulationSpace(ownerSpace, projected);
      if (expressiveArticulationSpace(projected))
        ++evidence.candidateEdgesWithExpressiveSpace;

      const bool original = candidatePassesCapability(candidate);
      RhythmCompatibilityCandidate reweighted = candidate;
      reweighted.weight = static_cast<uint8_t>(candidate.weight ^ 0xA5u);
      const bool afterWeightMutation = candidatePassesCapability(reweighted);
      if (original != afterWeightMutation)
        ++evidence.weightIndependenceViolations;
    }
    if (expressiveArticulationSpace(ownerSpace))
      ++evidence.ownersWithUpstreamExpressiveSpace;

    const GenerationProfileView profile = generationProfileFor(settings);
    if (profile.bassRhythms.candidates == nullptr ||
        profile.bassRhythms.count == 0) {
      std::printf("G4_CC1A_FAIL empty_bass_profile recipe=%u\n", recipe);
      return false;
    }
    const RhythmArchetypeId representativeArchetype =
        compatibility.candidates[0].archetypeId;
    for (uint8_t index = 0; index < profile.bassRhythms.count; ++index) {
      ++evidence.profileBassIdentityEdges;
      const uint8_t rawId = profile.bassRhythms.candidates[index].id;
      if (rawId < 16) uniqueBassMask = static_cast<uint16_t>(
          uniqueBassMask | static_cast<uint16_t>(1u << rawId));
      if (bassIdentityCanProduceContinuation(
              static_cast<BassRhythmId>(rawId), representativeArchetype)) {
        ++evidence.continuationCapableProfileBassEdges;
      }
    }

    const TonalGenerationProfile tonal = tonalGenerationProfileFor(settings);
    const uint16_t plain =
        bassArticulationStyleBit(BassArticulationStyleId::Plain);
    if (tonal.bassPolicy.allowedArticulations != plain)
      ++evidence.ownersWithMoreThanPlainPitchArticulation;

    std::printf(
        "G4_CC1A_OWNER recipe=%u name=%s candidates=%u upstream_expressive=%u bass_ids=%u allowed_articulation_mask=%04x\n",
        recipe, ownerName(recipe), compatibility.count,
        expressiveArticulationSpace(ownerSpace) ? 1u : 0u,
        profile.bassRhythms.count,
        tonal.bassPolicy.allowedArticulations);
  }

  evidence.uniqueProfileBassIdentities = popcount16(uniqueBassMask);
  std::printf(
      "G4_CC1A_CANDIDATE owners=%u candidate_edges=%u expressive_edges=%u upstream_expressive_owners=%u bass_identity_edges=%u unique_bass_ids=%u continuation_capable_bass_edges=%u non_plain_articulation_owners=%u weight_independence_violations=%u\n",
      evidence.owners, evidence.candidateEdges,
      evidence.candidateEdgesWithExpressiveSpace,
      evidence.ownersWithUpstreamExpressiveSpace,
      evidence.profileBassIdentityEdges, evidence.uniqueProfileBassIdentities,
      evidence.continuationCapableProfileBassEdges,
      evidence.ownersWithMoreThanPlainPitchArticulation,
      evidence.weightIndependenceViolations);

  if (evidence.owners != kOwnerExpected ||
      evidence.candidateEdges != kCandidateEdgeExpected ||
      evidence.candidateEdgesWithExpressiveSpace != kCandidateEdgeExpected ||
      evidence.ownersWithUpstreamExpressiveSpace != kOwnerExpected ||
      evidence.weightIndependenceViolations != 0) {
    std::puts("G4_CC1A_FAIL candidate_space");
    ok = false;
  }
  return ok;
}

bool reconstructBassPlans(const GenreSettings& settings,
                          const StrongRhythmFrozenSelection& selection,
                          const DrumPatternSet& drums,
                          BassRhythmPlan& rhythmPlan,
                          BassPitchBehaviorPlan& pitchPlan) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(
          selection.composition.rhythmArchetypeId);
  const RhythmArchetype* archetype =
      archetypeForId(selection.composition.rhythmArchetypeId);
  if (definition == nullptr || archetype == nullptr) return false;

  BassRhythmRequest bassRequest{};
  bassRequest.requestedId = selection.composition.bassRhythm;
  bassRequest.family = definition->family;
  bassRequest.archetypeId = definition->archetypeId;
  bassRequest.kickOnsets = drumKickOnsets(drums);
  bassRequest.protectedSpace =
      protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
  bassRequest.generation = selection.realizationGeneration;
  bassRequest.barOrdinal = 0;
  bassRequest.allowEmptyBar = acidAllowsSparseSemanticBar(definition->family);
  const BassRhythmResult bass = realizeBassRhythm(bassRequest);
  if (bass.status != BassRhythmStatus::Ok &&
      bass.status != BassRhythmStatus::ValidButEmpty) {
    return false;
  }

  BassPitchBehaviorRequest pitchRequest{};
  pitchRequest.rhythmPlan = bass.plan;
  pitchRequest.archetypeId = definition->archetypeId;
  pitchRequest.generation = bassRequest.generation;
  pitchRequest.barOrdinal = 0;
  pitchRequest.policy = tonalGenerationProfileFor(settings).bassPolicy;
  const BassPitchBehaviorResult pitch = realizeBassPitchBehavior(pitchRequest);
  if (pitch.status != BassPitchBehaviorStatus::Ok &&
      pitch.status != BassPitchBehaviorStatus::ValidButEmpty) {
    return false;
  }

  rhythmPlan = bass.plan;
  pitchPlan = pitch.plan;
  return true;
}

uint8_t synthActiveCells(const SynthPattern& pattern) {
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

  const uint8_t recipeCount = availableRecipeCount(GenerativeMode::Acid);
  for (uint8_t ownerIndex = 0; ownerIndex < recipeCount; ++ownerIndex) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ownerIndex, recipe))
      return false;
    const GenreSettings settings = settingsFor(recipe);

    for (uint16_t identity = 1; identity <= kIdentityCount; ++identity) {
      for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
        const RealizationLevel level = levels[levelIndex];
        ++evidence.attempted;
        StrongRhythmMigrationContext context = migrationContextFor(level);
        StrongRhythmFrozenSelection selection{};
        const StrongRhythmMigrationResult selected =
            resolveStrongRhythmFrozenSelection(
                settings, context, identity, selection);
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

        BassRhythmPlan rhythmPlan{};
        BassPitchBehaviorPlan pitchPlan{};
        if (!reconstructBassPlans(
                settings, selection, drums, rhythmPlan, pitchPlan)) {
          ++evidence.failed;
          continue;
        }
        if (rhythmPlan.id != realized.bassRhythmId ||
            pitchPlan.contour != realized.bassPitchContour) {
          std::printf(
              "G4_CC1A_FAIL reconstruction recipe=%u identity=%u level=%u\n",
              recipe, identity, static_cast<unsigned>(level));
          ++evidence.failed;
          continue;
        }

        const uint8_t attacks = popcount16(pitchPlan.onsets);
        const uint8_t continuations = popcount16(pitchPlan.continuations);
        const uint8_t slideInto = popcount16(pitchPlan.slideIntoOnsets);
        const uint8_t activeCells = synthActiveCells(synthA);
        if (activeCells != static_cast<uint8_t>(attacks + continuations)) {
          std::printf(
              "G4_CC1A_FAIL active_cell_projection recipe=%u identity=%u level=%u attacks=%u continuations=%u active=%u\n",
              recipe, identity, static_cast<unsigned>(level), attacks,
              continuations, activeCells);
          ++evidence.failed;
          continue;
        }

        ++evidence.successful;
        MaterializedBucket& bucket = evidence.buckets[ownerIndex][levelIndex];
        ++bucket.rows;
        const uint8_t connectedSignals = static_cast<uint8_t>(
            continuations + slideInto);
        const uint8_t classCount = static_cast<uint8_t>(
            (attacks != 0 ? 1u : 0u) + (connectedSignals != 0 ? 1u : 0u));
        const bool allDetached = attacks != 0 && connectedSignals == 0;
        const bool allConnected = attacks == 1 && connectedSignals != 0;
        const bool mixed = attacks > 1 && connectedSignals != 0;

        if (allDetached) {
          ++bucket.allDetached;
          ++evidence.rowsAllDetached;
        }
        if (allConnected) {
          ++bucket.allConnected;
          ++evidence.rowsAllConnected;
        }
        if (mixed) {
          ++bucket.mixed;
          ++evidence.rowsMixed;
        }
        if (continuations != 0) {
          ++bucket.withContinuation;
          ++evidence.rowsWithContinuation;
        }
        if (slideInto != 0) {
          ++bucket.withSlide;
          ++evidence.rowsWithSlide;
        }
        if (classCount > 1) {
          ++bucket.withMultipleClasses;
          ++evidence.rowsWithMultipleClasses;
        }

        if (census.is_open()) {
          census << ownerName(recipe) << '\t'
                 << static_cast<unsigned>(recipe) << '\t'
                 << identity << '\t'
                 << levelName(level) << '\t'
                 << selection.composition.rhythmArchetypeId << '\t'
                 << static_cast<unsigned>(rhythmPlan.id) << '\t'
                 << bassArticulationStyleName(pitchPlan.articulation) << '\t'
                 << static_cast<unsigned>(attacks) << '\t'
                 << static_cast<unsigned>(continuations) << '\t'
                 << static_cast<unsigned>(slideInto) << '\t'
                 << static_cast<unsigned>(activeCells) << '\t'
                 << static_cast<unsigned>(classCount) << '\t'
                 << 'A' << static_cast<unsigned>(attacks)
                 << "_C" << static_cast<unsigned>(continuations)
                 << "_S" << static_cast<unsigned>(slideInto) << '\n';
        }
      }
    }
  }

  for (uint8_t ownerIndex = 0; ownerIndex < recipeCount; ++ownerIndex) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ownerIndex, recipe))
      return false;
    for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
      const MaterializedBucket& bucket = evidence.buckets[ownerIndex][levelIndex];
      std::printf(
          "G4_CC1A_DIST owner=%s level=%s rows=%u all_detached=%u all_connected=%u mixed=%u continuation=%u slide=%u multi_class=%u\n",
          ownerName(recipe), levelName(levels[levelIndex]), bucket.rows,
          bucket.allDetached, bucket.allConnected, bucket.mixed,
          bucket.withContinuation, bucket.withSlide,
          bucket.withMultipleClasses);
    }
  }

  std::printf(
      "G4_CC1A_MATERIAL attempted=%u successful=%u failed=%u all_detached=%u all_connected=%u mixed=%u continuation=%u slide=%u multi_class=%u\n",
      evidence.attempted, evidence.successful, evidence.failed,
      evidence.rowsAllDetached, evidence.rowsAllConnected,
      evidence.rowsMixed, evidence.rowsWithContinuation,
      evidence.rowsWithSlide, evidence.rowsWithMultipleClasses);
  return evidence.attempted == kMaterializationExpected &&
         evidence.successful + evidence.failed == evidence.attempted;
}

}  // namespace

int main(int argc, char** argv) {
  bool ok = true;
  ok = checkAdversarialProjectionFixtures() && ok;

  CandidateEvidence candidate{};
  ok = checkCandidateSpace(candidate) && ok;

  MaterializedEvidence materialized{};
  const char* censusPath = argc > 1 ? argv[1] : nullptr;
  ok = collectMaterializedEvidence(materialized, censusPath) && ok;

  if (candidate.weightIndependenceViolations == 0)
    std::puts("G4_CC1A_PASS fixture=weight_independence");
  else
    ok = false;

  if (candidate.ownersWithUpstreamExpressiveSpace == kOwnerExpected &&
      materialized.rowsWithMultipleClasses == 0) {
    std::puts("G4_CC1A_PASS observation=candidate_capability_differs_from_materialized_usage");
  } else {
    std::puts("G4_CC1A_FAIL observation=candidate_capability_differs_from_materialized_usage");
    ok = false;
  }

  if (materialized.failed != 0 ||
      materialized.successful != kMaterializationExpected) {
    std::puts("G4_CC1A_FAIL materialized_corpus");
    ok = false;
  }

  // RED hypothesis: before ratifying an owner-space articulation contract,
  // require evidence that the authoritative Acid Synth-A path can express at
  // least one connected/extended realization. The first run is expected to
  // decide this from production rather than assume the answer from LaneGrammar.
  if (materialized.rowsWithContinuation == 0 &&
      materialized.rowsWithSlide == 0) {
    std::puts("G4_CC1A_RED hypothesis=authoritative_acid_bass_space_has_no_connected_expression");
    ok = false;
  }

  if (!ok) {
    std::puts("G4-CC1A Acid structural minimum: RED");
    return 1;
  }

  std::puts("G4-CC1A Acid structural minimum: PASS");
  return 0;
}
