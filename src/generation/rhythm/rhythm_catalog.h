#pragma once

#include <cstdint>

#include "rhythm_types.h"

namespace GroovePuterRhythm {

struct RhythmCatalogView {
  const RhythmArchetype* archetypes = nullptr;
  uint16_t archetypeCount = 0;
  const BarTrajectory* trajectories = nullptr;
  uint8_t trajectoryCount = 0;
};

enum class CatalogValidationError : uint8_t {
  None = 0,
  NullArchetypeArray,
  EmptyArchetypeCatalog,
  DuplicateArchetypeId,
  InvalidArchetypeId,
  InvalidFamily,
  InvalidPhraseBarsMask,
  InvalidActiveRoleMask,
  MissingLaneArray,
  InvalidLaneCount,
  InvalidLaneRole,
  DuplicateLaneRole,
  LaneRoleNotActive,
  ActiveRoleMissingLane,
  OverlappingLaneZones,
  InvalidLaneDensity,
  TooManyStructuralAnchors,
  MissingProtectedSpaceArray,
  TooManyProtectedSpaces,
  EmptyProtectedSpace,
  InvalidProtectedSpaceRoles,
  ProtectedSpaceAnchorConflict,
  MissingRelationshipArray,
  TooManyRelationships,
  InvalidRelationshipRole,
  RelationshipRoleNotActive,
  SameRoleRelationship,
  InvalidRelationshipOp,
  InvalidConstraintStrength,
  InvalidRelationshipScope,
  EmptyRelationshipZone,
  InvalidRelationshipWeight,
  InvalidRelationshipOffsets,
  InvalidRelationshipCardinality,
  ImpossibleHardRelationship,
  HardFillGapsUnsupported,
  MissingAnchorTransformArray,
  TooManyAnchorTransformRules,
  InvalidAnchorTransformRole,
  AnchorTransformRoleNotActive,
  InvalidAnchorTransformBarFunction,
  InvalidAnchorTransformIntent,
  EmptyAnchorTransformRule,
  AnchorTransformOutsideCanonical,
  MissingTrajectoryRefArray,
  TooManyTrajectoryRefs,
  InvalidTrajectoryRef,
  DuplicateTrajectoryRef,
  UnknownTrajectoryId,
  InvalidTrajectoryWeight,
  InvalidRealizationLevelMask,
  InvalidTimingCompatibility,
  InvalidTimingEligibilityRoles,
  InvalidDensityContract,
  InvalidMutationPolicy,
  MissingTrajectoryArray,
  InvalidTrajectoryId,
  DuplicateTrajectoryId,
  InvalidTrajectoryBarCount,
  InvalidTrajectoryBarFunction,
  TrajectoryLengthNotAllowed,
  TrajectoryLevelConflict,
  MissingTrajectoryCoverage,
};

struct CatalogValidationResult {
  CatalogValidationError error = CatalogValidationError::None;
  uint16_t archetypeIndex = kNoArchetypeIndex;
  uint8_t itemIndex = kNoItemIndex;

  explicit operator bool() const {
    return error == CatalogValidationError::None;
  }
};

CatalogValidationResult validateRhythmCatalog(const RhythmCatalogView& catalog);

}  // namespace GroovePuterRhythm
