#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/generation_context.h"
#include "src/generation/harmony/harmonic_route.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"

using namespace GroovePuterHarmony;
using namespace GroovePuterRhythm;

namespace {
HarmonicState state(uint8_t root, QualityId qualityId) {
  const auto* quality = qualityDefinition(qualityId);
  return {qualityId, transposePitchSet(quality->intervalMask, root), root,
          kNoBassPc};
}

TonalContext context(uint8_t tonic, ScaleId scaleId) {
  const auto* scale = scaleDefinition(scaleId);
  return {scaleId, scaleId, transposePitchSet(scale->intervalMask, tonic),
          transposePitchSet(scale->intervalMask, tonic), tonic, tonic};
}

RouteRequest baseRequest() {
  RouteRequest request{};
  request.candidateRequest.current = state(7, kQualityMajor);
  request.candidateRequest.currentContext = context(0, 1);
  request.candidateRequest.home = state(0, kQualityMajor);
  request.candidateRequest.homeContext = context(0, 1);
  request.candidateRequest.style = HarmonicStyleId::Ambient;
  request.candidateRequest.section = SectionIntent::Return;
  request.candidateRequest.runtimeQualityMask = 0x03ffu;
  request.candidateRequest.runtimeScaleMask = 0x03ffu;
  request.projectSeed = 42;
  request.songIdentity = 0x1234abcdu;
  request.modelVersion = 7;
  request.availableTimingSlots = 4;
  return request;
}

bool sameChunk(const ResolvedRouteChunk& a, const ResolvedRouteChunk& b) {
  if (a.count != b.count) return false;
  for (uint8_t i = 0; i < a.count; ++i) {
    const auto& left = a.events[i];
    const auto& right = b.events[i];
    if (left.state.rootPc != right.state.rootPc ||
        left.state.qualityId != right.state.qualityId ||
        left.tonal.scaleId != right.tonal.scaleId ||
        left.tonal.tonicPc != right.tonal.tonicPc ||
        left.relationTags != right.relationTags ||
        left.cadenceClass != right.cadenceClass ||
        left.timingSlotOrdinal != right.timingSlotOrdinal) return false;
  }
  return true;
}
}  // namespace

int main() {
  static_assert(sizeof(RouteScratch) <= 4096);
  RouteRequest request = baseRequest();
  RecentHarmonyMemory recent{};
  RouteScratch scratch{};
  RouteCandidateWorkspace workspace{};
  ResolvedRouteChunk route{};
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  assert(route.count == 1);
  assert(route.events[0].state.rootPc == request.candidateRequest.home.rootPc);
  assert(route.events[0].state.qualityId == request.candidateRequest.home.qualityId);
  assert(route.events[0].tonal.scaleId == request.candidateRequest.homeContext.scaleId);
  assert(route.events[0].tonal.tonicPc == request.candidateRequest.homeContext.tonicPc);
  assert(route.events[0].cadenceClass == CadenceClass::DirectHome);

  // A distant tritone state has no direct structural edge to home. RETURN
  // uses bounded legal neighbors (F# -> G -> C) instead of root-only jumping.
  request = baseRequest();
  request.candidateRequest.current = state(6, kQualityMajor);
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  assert(route.count == 2);
  assert(scratch.diagnosticCount == route.count);
  assert(scratch.diagnostics[1].score > 0);
  assert(route.events[0].state.rootPc != 0);
  assert(route.events[1].state.rootPc == 0);
  request.availableTimingSlots = 1;
  ResolvedRouteChunk insufficient{};
  insufficient.count = 1;
  insufficient.events[0].state = state(9, kQualityMinor);
  const HarmonyStatus insufficientStatus =
      planRouteChunk(request, recent, scratch, workspace, insufficient);
  assert(insufficientStatus == HarmonyStatus::CapacityExceeded);
  assert(insufficient.count == 1 && insufficient.events[0].state.rootPc == 9);

  request = baseRequest();
  request.candidateRequest.section = SectionIntent::Develop;
  request.candidateRequest.current = state(0, kQualityMajor);
  request.availableTimingSlots = 4;
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  ResolvedRouteChunk developRepeat{};
  assert(planRouteChunk(request, recent, scratch, workspace, developRepeat) ==
         HarmonyStatus::Ready);
  assert(sameChunk(route, developRepeat));
  assert(route.count == 4);
  assert(scratch.diagnosticCount == route.count);
  for (uint8_t i = 0; i < route.count; ++i)
    assert(route.events[i].timingSlotOrdinal == i);
  uint8_t expectedChanges = 0;
  HarmonicState previousState = request.candidateRequest.current;
  for (uint8_t i = 0; i < route.count; ++i) {
    if (route.events[i].state.rootPc != previousState.rootPc ||
        route.events[i].state.qualityId != previousState.qualityId)
      ++expectedChanges;
    previousState = route.events[i].state;
  }
  assert(route.endingChangesInSection == expectedChanges);
  request.candidateRequest.current = route.events[route.count - 1].state;
  request.candidateRequest.currentContext = route.events[route.count - 1].tonal;
  request.candidateRequest.changesInSection = route.endingChangesInSection;
  request.candidateRequest.modulationsInSection =
      route.endingModulationsInSection;
  request.globalEventOrdinal = 4;
  request.firstTimingSlot = 4;
  ResolvedRouteChunk continuation{};
  assert(planRouteChunk(request, recent, scratch, workspace, continuation) ==
         HarmonyStatus::Ready);
  assert(continuation.count == 4);
  assert(continuation.events[0].timingSlotOrdinal == 4);

  request = baseRequest();
  request.candidateRequest.section = SectionIntent::Keep;
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  for (uint8_t i = 0; i < route.count; ++i)
    assert(route.events[i].tonal.scaleId == request.candidateRequest.currentContext.scaleId &&
           route.events[i].tonal.tonicPc == request.candidateRequest.currentContext.tonicPc);
  request.candidateRequest.section = SectionIntent::Take;
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  for (uint8_t i = 0; i < route.count; ++i)
    assert(route.events[i].tonal.scaleId == request.candidateRequest.homeContext.scaleId &&
           route.events[i].tonal.tonicPc == request.candidateRequest.homeContext.tonicPc);

  request = baseRequest();
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  ResolvedRouteChunk repeated{};
  assert(planRouteChunk(request, recent, scratch, workspace, repeated) == HarmonyStatus::Ready);
  assert(sameChunk(route, repeated));
  commitFingerprint(recent, HarmonicFingerprintSet{1, 2, 3, 4, 5});
  ResolvedRouteChunk withMemory{};
  assert(planRouteChunk(request, recent, scratch, workspace, withMemory) == HarmonyStatus::Ready);
  assert(sameChunk(route, withMemory));

  // RETURN to the exact home context even when tonic/root match but current
  // interpretation differs; root-only success is invalid.
  request.candidateRequest.current = request.candidateRequest.home;
  request.candidateRequest.currentContext = context(0, 2);
  request.candidateRequest.section = SectionIntent::Return;
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  assert(route.count == 1);
  assert(route.events[0].tonal.scaleId == request.candidateRequest.homeContext.scaleId);

  // Primary scale/tonic equality is insufficient if the immutable home
  // reference differs; RETURN must restore the complete home context.
  request = baseRequest();
  request.candidateRequest.current = request.candidateRequest.home;
  request.candidateRequest.currentContext = context(0, 1);
  request.candidateRequest.currentContext.homeScaleId = 0;
  request.candidateRequest.currentContext.homeScaleMask =
      transposePitchSet(scaleDefinition(0)->intervalMask, 0);
  assert(planRouteChunk(request, recent, scratch, workspace, route) ==
         HarmonyStatus::Ready);
  assert(route.count == 1);
  assert(route.events[0].tonal.homeScaleId ==
         request.candidateRequest.homeContext.homeScaleId);

  for (uint8_t budget = 0; budget <= 4; ++budget) {
    request = baseRequest();
    request.availableTimingSlots = budget;
    if (budget == 0) {
      assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
      assert(route.count == 0);
    } else {
      const HarmonyStatus status = planRouteChunk(request, recent, scratch, workspace, route);
      assert(status == HarmonyStatus::Ready || status == HarmonyStatus::CapacityExceeded);
      if (status == HarmonyStatus::Ready) assert(route.count <= budget);
    }
  }
  request = baseRequest();
  request.availableTimingSlots = 32;
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Ready);
  assert(route.count <= kMaxRouteChunkEvents);

  // Full phrase assembly carries state, canonical function memory and section
  // budgets across bounded planner chunks. A repeated request is byte-field
  // deterministic and preserves phrase-global timing ordinals.
  request = baseRequest();
  request.candidateRequest.section = SectionIntent::Develop;
  request.candidateRequest.current = request.candidateRequest.home;
  request.candidateRequest.currentContext = request.candidateRequest.homeContext;
  request.availableTimingSlots = 12;
  ResolvedHarmonicEvent phraseA[32]{};
  ResolvedHarmonicEvent phraseB[32]{};
  uint8_t phraseACount = 0;
  uint8_t phraseBCount = 0;
  HarmonicState terminalA{};
  HarmonicState terminalB{};
  TonalContext terminalContextA{};
  TonalContext terminalContextB{};
  assert(planRoute(request, recent, scratch, workspace, phraseA, 32,
                   phraseACount, terminalA, terminalContextA) ==
         HarmonyStatus::Ready);
  assert(planRoute(request, recent, scratch, workspace, phraseB, 32,
                   phraseBCount, terminalB, terminalContextB) ==
         HarmonyStatus::Ready);
  assert(phraseACount == 12 && phraseBCount == phraseACount);
  for (uint8_t i = 0; i < phraseACount; ++i) {
    assert(phraseA[i].timingSlotOrdinal == i);
    assert(phraseA[i].state.rootPc == phraseB[i].state.rootPc);
    assert(phraseA[i].state.qualityId == phraseB[i].state.qualityId);
    assert(phraseA[i].tonal.scaleId == phraseB[i].tonal.scaleId);
    assert(phraseA[i].tonal.tonicPc == phraseB[i].tonal.tonicPc);
  }
  assert(terminalA.rootPc == phraseA[phraseACount - 1].state.rootPc);
  assert(terminalContextA.tonicPc ==
         phraseA[phraseACount - 1].tonal.tonicPc);

  // Failure preserves caller outputs, including the event array.
  ResolvedHarmonicEvent protectedPhrase[32]{};
  protectedPhrase[0].state = state(9, kQualityMinor);
  const HarmonicState protectedTerminal = protectedPhrase[0].state;
  uint8_t protectedCount = 77;
  RouteRequest invalidPhrase = request;
  invalidPhrase.availableTimingSlots = 33;
  assert(planRoute(invalidPhrase, recent, scratch, workspace, protectedPhrase,
                   32, protectedCount, terminalA, terminalContextA) ==
         HarmonyStatus::Invalid);
  assert(protectedCount == 77 && protectedPhrase[0].state.rootPc ==
         protectedTerminal.rootPc);
  RouteRequest invalidEmpty = request;
  invalidEmpty.availableTimingSlots = 0;
  invalidEmpty.candidateRequest.style = static_cast<HarmonicStyleId>(255);
  assert(planRoute(invalidEmpty, recent, scratch, workspace, nullptr, 0,
                   protectedCount, terminalA, terminalContextA) ==
         HarmonyStatus::Invalid);
  assert(protectedCount == 77);

  // Removing home scale capability makes exact RETURN unreachable. Failure
  // must leave the previous output untouched.
  request = baseRequest();
  request.candidateRequest.runtimeScaleMask = 1u << 1;
  request.candidateRequest.homeContext = context(0, 0);
  ResolvedRouteChunk sentinel{};
  sentinel.count = 1;
  sentinel.events[0].state = state(5, kQualityMinor);
  route = sentinel;
  const HarmonyStatus unreachable = planRouteChunk(request, recent, scratch, workspace, route);
  assert(unreachable == HarmonyStatus::ReturnUnreachable ||
         unreachable == HarmonyStatus::CapacityExceeded);
  assert(route.count == 1 && route.events[0].state.rootPc == 5);

  request = baseRequest();
  request.candidateRequest.style = static_cast<HarmonicStyleId>(255);
  assert(planRouteChunk(request, recent, scratch, workspace, route) == HarmonyStatus::Invalid);
  assert(route.count == 1 && route.events[0].state.rootPc == 5);

  std::cout << "HGS bounded harmonic routes: PASS\n";
  return 0;
}
