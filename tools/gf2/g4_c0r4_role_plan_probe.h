#ifndef GROOVEPUTER_TOOLS_GF2_G4_C0R4_ROLE_PLAN_PROBE_H
#define GROOVEPUTER_TOOLS_GF2_G4_C0R4_ROLE_PLAN_PROBE_H

#include "../../src/generation/rhythm/rhythm_realizer.h"

#include <cstdint>

namespace GroovePuterRhythm {
namespace G4C0R4 {

struct RolePlanProbeSnapshot {
  RoleRhythmPlan bass{};
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  RealizationStatus status = RealizationStatus::InvalidConstraintSet;
  uint8_t callCount = 0;
  bool observed = false;
  bool overflow = false;
};

void resetRolePlanProbe();
RolePlanProbeSnapshot rolePlanProbeSnapshot();

}  // namespace G4C0R4
}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_TOOLS_GF2_G4_C0R4_ROLE_PLAN_PROBE_H
