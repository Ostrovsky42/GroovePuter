#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_FINGERPRINT_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_FINGERPRINT_H

#include <cstdint>
#include <type_traits>

#include "harmonic_types.h"

namespace GroovePuterHarmony {

struct HarmonicFingerprintSet {
  uint64_t rootMotion = 0;
  uint64_t qualitySequence = 0;
  uint64_t semanticRelation = 0;
  uint64_t cadence = 0;
  uint64_t scaleContext = 0;
};
static_assert(sizeof(HarmonicFingerprintSet) == 40,
              "harmonic fingerprint set must remain five uint64 values");
static_assert(std::is_trivially_copyable<HarmonicFingerprintSet>::value,
              "fingerprint set must remain a fixed value type");

struct RecentHarmonyMemory {
  HarmonicFingerprintSet entries[8]{};
  uint8_t count = 0;
  uint8_t next = 0;
  uint8_t recentEdges[8]{};
  uint8_t edgeCount = 0;
  uint8_t edgeNext = 0;
};
static_assert(sizeof(RecentHarmonyMemory) <= 336,
              "recent harmony memory exceeded the bounded session budget");

HarmonyStatus fingerprintRoute(const ResolvedRouteView& route,
                              HarmonicFingerprintSet& out);
uint8_t harmonicRepeatPenalty(const RecentHarmonyMemory& memory,
                              const HarmonicFingerprintSet& candidate,
                              uint8_t repetitionTolerance);
uint8_t harmonicEdgeToken(const HarmonicState& from,
                          const TonalContext& fromTonal,
                          const HarmonicState& to,
                          const TonalContext& toTonal);
uint8_t harmonicRecentEdgePenalty(const RecentHarmonyMemory& memory,
                                  uint8_t edgeToken,
                                  uint8_t repetitionTolerance);
void commitFingerprint(RecentHarmonyMemory& memory,
                       const HarmonicFingerprintSet& accepted);
void commitRecentEdge(RecentHarmonyMemory& memory, uint8_t edgeToken);
HarmonyStatus commitAcceptedRoute(RecentHarmonyMemory& memory,
                                 const ResolvedRouteView& accepted);
void resetRecentHarmonyMemory(RecentHarmonyMemory& memory);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_FINGERPRINT_H
