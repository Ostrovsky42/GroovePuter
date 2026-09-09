#include "g4_c0r4_role_plan_probe.h"

namespace GroovePuterRhythm {

// The production rhythm_realizer.cpp is compiled for the C0R4 research binary
// with realizeRhythmPhrase renamed to this symbol. The wrapper below therefore
// observes the exact production RhythmPhrasePlan consumed by migration instead
// of reconstructing planning bass from physical Pattern output.
RhythmRealizationResult g4C0R4RealRealizeRhythmPhrase(
    const RhythmRealizationRequest& request);

namespace G4C0R4 {
namespace {
RolePlanProbeSnapshot g_snapshot{};
}

void resetRolePlanProbe() {
  g_snapshot = RolePlanProbeSnapshot{};
}

RolePlanProbeSnapshot rolePlanProbeSnapshot() {
  return g_snapshot;
}

void capture(const RhythmRealizationRequest& request,
             const RhythmRealizationResult& result) {
  if (g_snapshot.callCount != 0) {
    g_snapshot.overflow = true;
    ++g_snapshot.callCount;
    return;
  }

  g_snapshot.callCount = 1;
  g_snapshot.archetypeId = request.archetypeId;
  g_snapshot.status = result.status;
  if ((result.status != RealizationStatus::Ok &&
       result.status != RealizationStatus::ValidButSparse) ||
      result.plan.barCount == 0) {
    return;
  }

  g_snapshot.bass =
      result.plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::BassRhythm)];
  g_snapshot.observed = true;
}

}  // namespace G4C0R4

RhythmRealizationResult realizeRhythmPhrase(
    const RhythmRealizationRequest& request) {
  const RhythmRealizationResult result = g4C0R4RealRealizeRhythmPhrase(request);
  G4C0R4::capture(request, result);
  return result;
}

}  // namespace GroovePuterRhythm
