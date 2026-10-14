#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROUTE_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROUTE_H

#include <cstdint>

#include "harmonic_candidate.h"
#include "harmonic_fingerprint.h"
#include "harmonic_source.h"

namespace GroovePuterHarmony {

constexpr uint8_t kMaxRouteChunkEvents = 4;
constexpr uint8_t kMaxReturnSearchNodes = 128;

struct RouteRequest {
  CandidateRequest candidateRequest{};
  uint32_t projectSeed = 0;
  uint32_t songIdentity = 0;
  uint16_t modelVersion = 1;
  uint16_t globalEventOrdinal = 0;
  uint8_t firstTimingSlot = 0;
  uint8_t availableTimingSlots = 0;
  uint8_t repetitionTolerance = 0;
};

struct ResolvedRouteChunk {
  ResolvedHarmonicEvent events[kMaxRouteChunkEvents]{};
  uint8_t count = 0;
  uint8_t endingChangesInSection = 0;
  uint8_t endingModulationsInSection = 0;
};

struct RouteStepDiagnostic {
  HarmonicRelationFeatures features{};
  int16_t score = 0;
  uint8_t corpusPrior = 0;
};

struct RouteSearchNode {
  HarmonicState state{};
  TonalContext tonal{};
  uint16_t cost = 0;
  int16_t parent = -1;
  RelationTags relationTags = 0;
  uint8_t depth = 0;
  uint8_t closed = 0;
  uint8_t changesInSection = 0;
  uint8_t modulationsInSection = 0;
};

struct RouteScratch {
  RouteSearchNode nodes[kMaxReturnSearchNodes]{};
  RouteStepDiagnostic diagnostics[kMaxRouteChunkEvents]{};
  uint8_t nodeCount = 0;
  uint8_t diagnosticCount = 0;
};
static_assert(sizeof(RouteScratch) <= 4096,
              "route search scratch exceeded 4096 bytes");

struct RouteCandidateWorkspace {
  CandidateScratch candidateScratch{};
  CandidateList candidates{};
  TonalCandidateList contexts{};
};
static_assert(sizeof(RouteCandidateWorkspace) <= 5120,
              "candidate workspace exceeded its bounded memory budget");

HarmonyStatus planRouteChunk(const RouteRequest& request,
                             const RecentHarmonyMemory& recent,
                             RouteScratch& scratch,
                             RouteCandidateWorkspace& candidateWorkspace,
                             ResolvedRouteChunk& out);

// Assemble up to 32 phrase events by repeatedly invoking the bounded
// four-event planner. State, canonical transition tokens and section budgets
// carry across chunks. For RETURN, any slots after exact home is reached are
// filled with the home state. Outputs remain untouched on failure.
HarmonyStatus planRoute(const RouteRequest& request,
                        const RecentHarmonyMemory& recent,
                        RouteScratch& scratch,
                        RouteCandidateWorkspace& candidateWorkspace,
                        ResolvedHarmonicEvent* outEvents,
                        uint8_t outCapacity,
                        uint8_t& outCount,
                        HarmonicState& outTerminal,
                        TonalContext& outTerminalContext);

// Build the immutable materializer view for a complete phrase route. Identity
// metadata is supplied by the caller; a failed route leaves all outputs
// untouched.
HarmonyStatus planGraphHarmony(
    const RouteRequest& request,
    const RecentHarmonyMemory& recent,
    RouteScratch& scratch,
    RouteCandidateWorkspace& candidateWorkspace,
    uint32_t materialId,
    uint32_t publicationGeneration,
    PreparedGraphHarmony& out,
    HarmonicState& outTerminal,
    TonalContext& outTerminalContext);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROUTE_H
