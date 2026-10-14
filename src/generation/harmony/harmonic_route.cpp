#include "harmonic_route.h"

#include "src/generation/generation_context.h"
#include "harmonic_quality.h"
#include "harmonic_relation.h"
#include "harmonic_scale.h"

namespace GroovePuterHarmony {
namespace {

using GroovePuterRhythm::GenerationDomain;

bool sameContext(const TonalContext& a, const TonalContext& b) {
  return a.scaleId == b.scaleId && a.tonicPc == b.tonicPc &&
         a.scaleMask == b.scaleMask && a.homeScaleId == b.homeScaleId &&
         a.homeTonicPc == b.homeTonicPc &&
         a.homeScaleMask == b.homeScaleMask;
}

bool sameState(const HarmonicState& a, const HarmonicState& b) {
  return a.rootPc == b.rootPc && a.qualityId == b.qualityId &&
         a.bassPc == b.bassPc && a.pitchMask == b.pitchMask;
}

bool validState(const HarmonicState& state) {
  if (state.rootPc > 11 ||
      (state.bassPc != kNoBassPc && state.bassPc > 11)) return false;
  const ChordQualityDefinition* quality = qualityDefinition(state.qualityId);
  return quality != nullptr &&
         state.pitchMask == transposePitchSet(quality->intervalMask,
                                              state.rootPc);
}

bool validContext(const TonalContext& context) {
  if (context.tonicPc > 11 || context.homeTonicPc > 11) return false;
  const ScaleDefinition* scale = scaleDefinition(context.scaleId);
  const ScaleDefinition* home = scaleDefinition(context.homeScaleId);
  return scale != nullptr && home != nullptr &&
         context.scaleMask == transposePitchSet(scale->intervalMask,
                                                context.tonicPc) &&
         context.homeScaleMask == transposePitchSet(home->intervalMask,
                                                    context.homeTonicPc);
}

uint8_t rootDistance(uint8_t a, uint8_t b) {
  const uint8_t forward = static_cast<uint8_t>((a + 12u - b) % 12u);
  const uint8_t reverse = static_cast<uint8_t>((b + 12u - a) % 12u);
  return forward < reverse ? forward : reverse;
}

bool isGoal(const RouteSearchNode& node, const RouteRequest& request) {
  return sameState(node.state, request.candidateRequest.home) &&
         sameContext(node.tonal, request.candidateRequest.homeContext);
}

bool sameNode(const RouteSearchNode& node, const HarmonicState& state,
              const TonalContext& tonal) {
  return sameState(node.state, state) && sameContext(node.tonal, tonal);
}

uint16_t edgeCost(const Candidate& candidate,
                  const TonalContext& sourceContext,
                  const TonalContext& targetContext,
                  const RecentHarmonyMemory& recent,
                  const HarmonicState& sourceState,
                  uint8_t repetitionTolerance) {
  const HarmonicRelationFeatures& f = candidate.features;
  uint16_t cost = static_cast<uint16_t>(1u + f.pcVoiceLeadingCost +
      f.rootFifthsDistance * 2u + f.outOfScaleTargetTones * 4u +
      (sameContext(sourceContext, targetContext) ? 0u : 12u));
  if (f.commonToneCount >= 3 && cost > 2) cost -= 2;
  const uint8_t token = harmonicEdgeToken(
      sourceState, sourceContext, candidate.state, targetContext);
  cost = static_cast<uint16_t>(cost + harmonicRecentEdgePenalty(
      recent, token, repetitionTolerance));
  if (cost == 0) cost = 1;
  return cost;
}

bool isReturnNeighbor(const Candidate& candidate) {
  const HarmonicRelationFeatures& f = candidate.features;
  constexpr RelationTags kStructuralNeighborTags =
      relationTag(RelationTag::PrlP) | relationTag(RelationTag::PrlR) |
      relationTag(RelationTag::PrlL) | relationTag(RelationTag::DiatonicFunctional) |
      relationTag(RelationTag::DominantToTarget) |
      relationTag(RelationTag::SecondaryDominant) |
      relationTag(RelationTag::BorrowedParallel) |
      relationTag(RelationTag::ModalMixture) |
      relationTag(RelationTag::ChromaticMediant) |
      relationTag(RelationTag::AugmentedPortal) |
      relationTag(RelationTag::DiminishedPassing) |
      relationTag(RelationTag::PivotChord) |
      relationTag(RelationTag::FifthRelated);
  return f.commonToneCount > 0 || f.rootSemitoneDistance <= 2 ||
         (f.tags & kStructuralNeighborTags) != 0;
}

uint32_t nodeTieKey(const RouteSearchNode& node, uint8_t index) {
  return static_cast<uint32_t>(node.state.rootPc) |
         (static_cast<uint32_t>(node.state.qualityId) << 4u) |
         (static_cast<uint32_t>(node.tonal.scaleId) << 12u) |
         (static_cast<uint32_t>(node.tonal.tonicPc) << 16u) |
         (static_cast<uint32_t>(node.depth) << 20u) |
         (static_cast<uint32_t>(index) << 24u);
}

int16_t findNode(const RouteScratch& scratch, const HarmonicState& state,
                 const TonalContext& tonal) {
  for (uint8_t i = 0; i < scratch.nodeCount; ++i) {
    if (sameNode(scratch.nodes[i], state, tonal)) return i;
  }
  return -1;
}

int32_t scoreCandidate(const RouteRequest& request,
                       const CandidateRequest& currentRequest,
                       const RecentHarmonyMemory& recent,
                       const Candidate& candidate,
                       const TonalContext& candidateContext,
                       uint8_t eventIndex) {
  const GenreHarmonicPolicy* policy = policyFor(request.candidateRequest.style);
  if (policy == nullptr) return -1;
  const HarmonicRelationFeatures& f = candidate.features;
  const SongVariantGenome& g = request.candidateRequest.genome;
  const HarmonicFeatureWeights& w = policy->featureWeights;
  int32_t score = 48;
  score += static_cast<int32_t>(f.commonToneCount) * (w.commonTone + g.commonToneBias / 32u);
  score -= static_cast<int32_t>(f.pcVoiceLeadingCost) * (w.pcvlPenalty + g.smoothnessBias / 48u);
  score -= static_cast<int32_t>(f.rootFifthsDistance) * w.fifthsDistancePenalty;
  score -= static_cast<int32_t>(f.outOfScaleTargetTones) * w.outOfScalePenalty;
  if (f.tags & relationTag(RelationTag::DiatonicFunctional))
    score += w.functionalTag + g.functionalBias / 32u;
  if (f.tags & static_cast<RelationTags>(
                   relationTag(RelationTag::PrlP) |
                   relationTag(RelationTag::PrlR) |
                   relationTag(RelationTag::PrlL)))
    score += w.prlTag;
  if (f.tags & relationTag(RelationTag::ModalMixture))
    score += w.modalTag + g.modalBias / 32u;
  if (f.tags & relationTag(RelationTag::ChromaticMediant))
    score += w.chromaticMediantTag + g.chromaticRisk / 32u;
  if (f.tags & relationTag(RelationTag::AugmentedPortal))
    score += w.portalTag + g.portalRisk / 32u;
  if (f.tags & relationTag(RelationTag::FifthRelated)) score += 2;
  score += static_cast<int32_t>(candidate.corpusPrior) * w.corpus;
  score -= static_cast<int32_t>(qualityDefinition(candidate.state.qualityId)->complexityRank) *
           (w.complexity + g.extensionComplexity / 48u);

  const uint8_t homeDistance = rootDistance(candidate.state.rootPc,
                                             request.candidateRequest.home.rootPc);
  score -= static_cast<int32_t>(homeDistance) * g.homeGravity / 64u;
  if (sameState(candidate.state, request.candidateRequest.home) &&
      sameContext(candidateContext, request.candidateRequest.homeContext)) {
    score += g.homeGravity / 8u;
  }
  if (request.candidateRequest.section == SectionIntent::Keep &&
      sameContext(candidateContext, request.candidateRequest.currentContext))
    score += g.smoothnessBias / 16u;
  if (request.candidateRequest.section == SectionIntent::Break) {
    score += static_cast<int32_t>(f.outOfScaleTargetTones) * g.chromaticRisk / 64u;
    score += g.harmonicMobility / 16u;
  }

  ResolvedHarmonicEvent event{};
  event.state = candidate.state;
  event.tonal = candidateContext;
  event.relationTags = f.tags;
  event.timingSlotOrdinal = static_cast<uint8_t>(request.firstTimingSlot + eventIndex);
  const ResolvedRouteView oneEvent{&event, 1, request.candidateRequest.home,
                                   request.candidateRequest.homeContext};
  HarmonicFingerprintSet fingerprint{};
  if (fingerprintRoute(oneEvent, fingerprint) == HarmonyStatus::Ready) {
    score -= harmonicRepeatPenalty(recent, fingerprint,
                                   request.repetitionTolerance);
  }
  const uint8_t edgeToken = harmonicEdgeToken(
      currentRequest.current, currentRequest.currentContext,
      candidate.state, candidateContext);
  score -= harmonicRecentEdgePenalty(recent, edgeToken,
                                     request.repetitionTolerance);
  return score;
}

uint8_t selectCandidate(const RouteRequest& request,
                        const CandidateRequest& currentRequest,
                        const RecentHarmonyMemory& recent,
                        const CandidateList& candidates,
                        const TonalCandidateList& contexts,
                        uint32_t routeSeed, uint32_t tieSeed,
                        uint8_t eventIndex, int16_t& selectedScore) {
  uint8_t weights[kMaxHarmonicCandidates]{};
  int16_t scores[kMaxHarmonicCandidates]{};
  uint16_t total = 0;
  for (uint8_t i = 0; i < candidates.count; ++i) {
    const Candidate& candidate = candidates.items[i];
    const TonalContext& context = contexts.items[candidate.contextIndex];
    if ((request.candidateRequest.section == SectionIntent::Take ||
         request.candidateRequest.section == SectionIntent::Keep) &&
        !sameContext(context, request.candidateRequest.section == SectionIntent::Take
                                  ? request.candidateRequest.homeContext
                                  : request.candidateRequest.currentContext)) {
      continue;
    }
    const int32_t score = scoreCandidate(request, currentRequest, recent,
                                         candidate, context, eventIndex);
    scores[i] = static_cast<int16_t>(score < -32768 ? -32768
                                      : score > 32767 ? 32767 : score);
    const uint8_t weight = score <= 1 ? 1 : score >= 255 ? 255
                                               : static_cast<uint8_t>(score);
    weights[i] = weight;
    total = static_cast<uint16_t>(total + weight);
  }
  if (total == 0) return 0xff;
  const uint32_t coordinate =
      (static_cast<uint32_t>(request.globalEventOrdinal + eventIndex) << 16u) |
      eventIndex;
  const uint32_t choiceSeed = GroovePuterRhythm::deterministicValue(
      routeSeed, GroovePuterRhythm::deterministicValue(tieSeed, coordinate));
  uint16_t pick = static_cast<uint16_t>(choiceSeed % total);
  for (uint8_t i = 0; i < candidates.count; ++i) {
    if (weights[i] == 0) continue;
    if (pick < weights[i]) {
      selectedScore = scores[i];
      return i;
    }
    pick = static_cast<uint16_t>(pick - weights[i]);
  }
  return 0xff;
}

HarmonyStatus planReturn(const RouteRequest& request,
                         const RecentHarmonyMemory& recent,
                         RouteScratch& scratch,
                         RouteCandidateWorkspace& workspace,
                         ResolvedRouteChunk& output) {
  scratch.diagnosticCount = 0;
  scratch.nodeCount = 1;
  scratch.nodes[0] = {request.candidateRequest.current,
                      request.candidateRequest.currentContext, 0, -1, 0, 0, 0,
                      request.candidateRequest.changesInSection,
                      request.candidateRequest.modulationsInSection};
  if (isGoal(scratch.nodes[0], request)) {
    output.endingChangesInSection = request.candidateRequest.changesInSection;
    output.endingModulationsInSection =
        request.candidateRequest.modulationsInSection;
    return HarmonyStatus::Ready;
  }

  CandidateScratch& candidateScratch = workspace.candidateScratch;
  CandidateList& candidates = workspace.candidates;
  TonalCandidateList& contexts = workspace.contexts;
  const uint32_t tieSeed = deriveHarmonicDomainSeed(
      request.projectSeed, request.songIdentity, request.candidateRequest.style,
      request.modelVersion, GenerationDomain::HarmonicCandidateTieBreak);
  uint8_t expanded = 0;
  bool discoveryTruncated = false;
  int16_t goalIndex = -1;

  while (expanded < kMaxReturnSearchNodes) {
    int16_t currentIndex = -1;
    uint32_t currentTie = 0xffffffffu;
    for (uint8_t i = 0; i < scratch.nodeCount; ++i) {
      const RouteSearchNode& node = scratch.nodes[i];
      if (node.closed) continue;
      const uint32_t tie = GroovePuterRhythm::deterministicValue(
          tieSeed, nodeTieKey(node, i));
      if (currentIndex < 0 || node.cost < scratch.nodes[currentIndex].cost ||
          (node.cost == scratch.nodes[currentIndex].cost && tie < currentTie)) {
        currentIndex = i;
        currentTie = tie;
      }
    }
    if (currentIndex < 0) break;
    RouteSearchNode& current = scratch.nodes[currentIndex];
    if (isGoal(current, request)) {
      goalIndex = currentIndex;
      break;
    }
    current.closed = 1;
    ++expanded;
    if (current.depth >= 4) continue;

    CandidateRequest requestAtNode = request.candidateRequest;
    requestAtNode.current = current.state;
    requestAtNode.currentContext = current.tonal;
    requestAtNode.section = SectionIntent::Return;
    requestAtNode.changesInSection = current.changesInSection;
    requestAtNode.modulationsInSection = current.modulationsInSection;
    if (current.depth > 0) {
      CanonicalFunctionToken latest{};
      if (canonicalFunctionTokenFor(current.state, current.tonal, latest) ==
          HarmonyStatus::Ready) {
        requestAtNode.previous1 = latest;
        if (current.depth == 1) {
          requestAtNode.previous2 = request.candidateRequest.previous1;
        } else {
          const RouteSearchNode& before = scratch.nodes[current.parent];
          canonicalFunctionTokenFor(before.state, before.tonal,
                                    requestAtNode.previous2);
        }
      }
    }
    if (collectCandidates(requestAtNode, candidateScratch, candidates, contexts) !=
        HarmonyStatus::Ready) continue;

    for (uint8_t c = 0; c < candidates.count; ++c) {
      const Candidate& candidate = candidates.items[c];
      const TonalContext& tonal = contexts.items[candidate.contextIndex];
      if (!isReturnNeighbor(candidate)) continue;
      const uint16_t edge = edgeCost(candidate, current.tonal, tonal, recent,
                                      current.state, request.repetitionTolerance);
      const uint32_t totalCost = static_cast<uint32_t>(current.cost) + edge;
      if (totalCost > 0xffffu) continue;
      int16_t known = findNode(scratch, candidate.state, tonal);
      if (known >= 0) {
        RouteSearchNode& node = scratch.nodes[known];
        if (node.closed || totalCost >= node.cost) continue;
        node.cost = static_cast<uint16_t>(totalCost);
        node.parent = currentIndex;
        node.relationTags = candidate.features.tags;
        node.depth = static_cast<uint8_t>(current.depth + 1u);
        node.changesInSection = static_cast<uint8_t>(current.changesInSection +
            (sameState(candidate.state, current.state) ? 0u : 1u));
        node.modulationsInSection = static_cast<uint8_t>(current.modulationsInSection +
            (sameContext(tonal, current.tonal) ? 0u : 1u));
        continue;
      }
      if (scratch.nodeCount >= kMaxReturnSearchNodes) {
        // Discovery capacity and expansion budget are different bounds. Keep
        // settling the retained frontier, including an already-discovered home.
        discoveryTruncated = true;
        continue;
      }
      RouteSearchNode& node = scratch.nodes[scratch.nodeCount++];
      node = {candidate.state, tonal, static_cast<uint16_t>(totalCost), currentIndex,
              candidate.features.tags, static_cast<uint8_t>(current.depth + 1u), 0,
              static_cast<uint8_t>(current.changesInSection +
                  (sameState(candidate.state, current.state) ? 0u : 1u)),
              static_cast<uint8_t>(current.modulationsInSection +
                  (sameContext(tonal, current.tonal) ? 0u : 1u))};
    }
  }
  if (goalIndex < 0) return discoveryTruncated || expanded >= kMaxReturnSearchNodes
                                ? HarmonyStatus::CapacityExceeded
                                : HarmonyStatus::ReturnUnreachable;

  const RouteSearchNode& goal = scratch.nodes[goalIndex];
  if (goal.depth > request.availableTimingSlots || goal.depth > kMaxRouteChunkEvents ||
      static_cast<uint16_t>(request.firstTimingSlot) + goal.depth > 32) {
    return HarmonyStatus::CapacityExceeded;
  }
  int16_t path[kMaxRouteChunkEvents]{};
  int16_t cursor = goalIndex;
  for (uint8_t i = goal.depth; i > 0; --i) {
    path[i - 1] = cursor;
    cursor = scratch.nodes[cursor].parent;
    if (cursor < 0) return HarmonyStatus::Invalid;
  }
  for (uint8_t i = 0; i < goal.depth; ++i) {
    const RouteSearchNode& node = scratch.nodes[path[i]];
    ResolvedHarmonicEvent& event = output.events[i];
    event.state = node.state;
    event.tonal = node.tonal;
    event.relationTags = node.relationTags;
    event.timingSlotOrdinal = static_cast<uint8_t>(request.firstTimingSlot + i);
    if (i > 0) {
      HarmonicRelationFeatures features{};
      const RouteSearchNode& from = scratch.nodes[path[i - 1]];
      if (analyzeRelation(from.state, node.state, from.tonal, node.tonal,
                          candidateScratch.pcvl, features) == HarmonyStatus::Ready) {
        scratch.diagnostics[i].features = features;
      }
    }
    scratch.diagnostics[i].score = static_cast<int16_t>(node.cost);
    if (i + 1u == goal.depth) {
      event.cadenceClass = CadenceClass::DirectHome;
      if (node.relationTags & relationTag(RelationTag::DominantToTarget))
        event.cadenceClass = CadenceClass::DominantHome;
      else if (node.relationTags & relationTag(RelationTag::PivotChord))
        event.cadenceClass = CadenceClass::PivotHome;
      else if (node.relationTags & relationTag(RelationTag::AugmentedPortal))
        event.cadenceClass = CadenceClass::PortalHome;
    }
  }
  output.count = goal.depth;
  output.endingChangesInSection = goal.changesInSection;
  output.endingModulationsInSection = goal.modulationsInSection;
  scratch.diagnosticCount = goal.depth;
  return HarmonyStatus::Ready;
}

}  // namespace

HarmonyStatus planRouteChunk(const RouteRequest& request,
                             const RecentHarmonyMemory& recent,
                             RouteScratch& scratch,
                             RouteCandidateWorkspace& candidateWorkspace,
                             ResolvedRouteChunk& out) {
  const CandidateRequest& base = request.candidateRequest;
  if (request.availableTimingSlots > 32 || request.firstTimingSlot > 31 ||
      static_cast<uint16_t>(request.firstTimingSlot) +
              (request.availableTimingSlots < kMaxRouteChunkEvents
                   ? request.availableTimingSlots
                   : kMaxRouteChunkEvents) > 32 ||
      policyFor(base.style) == nullptr ||
      static_cast<uint8_t>(base.section) >
          static_cast<uint8_t>(SectionIntent::Return) ||
      base.structuralMotionAllowed > 1 || !validState(base.current) ||
      !validState(base.home) || !validContext(base.currentContext) ||
      !validContext(base.homeContext)) {
    return HarmonyStatus::Invalid;
  }
  ResolvedRouteChunk result{};
  result.endingChangesInSection = base.changesInSection;
  result.endingModulationsInSection = base.modulationsInSection;
  scratch.diagnosticCount = 0;
  if (request.availableTimingSlots == 0) {
    out = result;
    return HarmonyStatus::Ready;
  }
  if (base.section == SectionIntent::Return) {
    const HarmonyStatus status = planReturn(request, recent, scratch,
                                            candidateWorkspace, result);
    if (status != HarmonyStatus::Ready) return status;
    out = result;
    return HarmonyStatus::Ready;
  }

  CandidateScratch& candidateScratch = candidateWorkspace.candidateScratch;
  CandidateList& candidates = candidateWorkspace.candidates;
  TonalCandidateList& contexts = candidateWorkspace.contexts;
  CandidateRequest current = base;
  const uint8_t count = request.availableTimingSlots < kMaxRouteChunkEvents
                            ? request.availableTimingSlots
                            : kMaxRouteChunkEvents;
  const uint32_t routeSeed = deriveHarmonicDomainSeed(
      request.projectSeed, request.songIdentity, base.style, request.modelVersion,
      GenerationDomain::HarmonicRoute);
  const uint32_t tieSeed = deriveHarmonicDomainSeed(
      request.projectSeed, request.songIdentity, base.style, request.modelVersion,
      GenerationDomain::HarmonicCandidateTieBreak);
  uint8_t changesUsed = base.changesInSection;
  uint8_t modulationsUsed = base.modulationsInSection;
  for (uint8_t i = 0; i < count; ++i) {
    current.section = base.section;
    current.changesInSection = changesUsed;
    current.modulationsInSection = modulationsUsed;
    if (collectCandidates(current, candidateScratch, candidates, contexts) !=
        HarmonyStatus::Ready) return HarmonyStatus::Forbidden;
    const uint32_t eventRouteSeed = GroovePuterRhythm::deterministicValue(
        routeSeed, request.globalEventOrdinal + i);
    const uint32_t eventTieSeed = GroovePuterRhythm::deterministicValue(
        tieSeed, request.globalEventOrdinal + i);
    int16_t selectedScore = 0;
    const uint8_t selected = selectCandidate(
        request, current, recent, candidates, contexts, eventRouteSeed,
        eventTieSeed, i, selectedScore);
    if (selected >= candidates.count) return HarmonyStatus::Forbidden;
    const Candidate& candidate = candidates.items[selected];
    ResolvedHarmonicEvent& event = result.events[i];
    event.state = candidate.state;
    event.tonal = contexts.items[candidate.contextIndex];
    event.relationTags = candidate.features.tags;
    event.timingSlotOrdinal = static_cast<uint8_t>(request.firstTimingSlot + i);
    scratch.diagnostics[i].features = candidate.features;
    scratch.diagnostics[i].score = selectedScore;
    scratch.diagnostics[i].corpusPrior = candidate.corpusPrior;
    if (sameState(event.state, base.home) &&
        sameContext(event.tonal, base.homeContext)) {
      event.cadenceClass = sameState(current.current, event.state)
                               ? CadenceClass::HoldHome
                               : CadenceClass::DirectHome;
    }
    if (!sameState(event.state, current.current) && changesUsed < 255)
      ++changesUsed;
    if (!sameContext(event.tonal, current.currentContext) && modulationsUsed < 255)
      ++modulationsUsed;
    current.current = event.state;
    current.currentContext = event.tonal;
    current.previous2 = current.previous1;
    if (canonicalFunctionTokenFor(event.state, event.tonal,
                                  current.previous1) != HarmonyStatus::Ready)
      return HarmonyStatus::Invalid;
  }
  result.count = count;
  result.endingChangesInSection = changesUsed;
  result.endingModulationsInSection = modulationsUsed;
  scratch.diagnosticCount = count;
  out = result;
  scratch.nodeCount = 0;
  return HarmonyStatus::Ready;
}

HarmonyStatus planRoute(const RouteRequest& request,
                        const RecentHarmonyMemory& recent,
                        RouteScratch& scratch,
                        RouteCandidateWorkspace& candidateWorkspace,
                        ResolvedHarmonicEvent* outEvents,
                        uint8_t outCapacity,
                        uint8_t& outCount,
                        HarmonicState& outTerminal,
                        TonalContext& outTerminalContext) {
  const uint8_t total = request.availableTimingSlots;
  if (total > 32 || request.firstTimingSlot > 31 ||
      static_cast<uint16_t>(request.firstTimingSlot) + total > 32 ||
      outCapacity < total || (total != 0 && outEvents == nullptr)) {
    return HarmonyStatus::Invalid;
  }

  RouteRequest validationRequest = request;
  validationRequest.availableTimingSlots = 0;
  ResolvedRouteChunk validationResult{};
  const HarmonyStatus validationStatus = planRouteChunk(
      validationRequest, recent, scratch, candidateWorkspace, validationResult);
  if (validationStatus != HarmonyStatus::Ready) return validationStatus;

  ResolvedHarmonicEvent staged[32]{};
  uint8_t stagedCount = 0;
  CandidateRequest current = request.candidateRequest;
  uint32_t globalOrdinal = request.globalEventOrdinal;
  uint8_t changes = current.changesInSection;
  uint8_t modulations = current.modulationsInSection;
  while (stagedCount < total) {
    const uint8_t remaining = static_cast<uint8_t>(total - stagedCount);
    const uint8_t chunkSize = remaining < kMaxRouteChunkEvents
        ? remaining
        : kMaxRouteChunkEvents;
    RouteRequest chunkRequest = request;
    chunkRequest.candidateRequest = current;
    chunkRequest.candidateRequest.changesInSection = changes;
    chunkRequest.candidateRequest.modulationsInSection = modulations;
    chunkRequest.availableTimingSlots = chunkSize;
    chunkRequest.firstTimingSlot = static_cast<uint8_t>(
        request.firstTimingSlot + stagedCount);
    if (globalOrdinal > 0xffffu) return HarmonyStatus::Invalid;
    chunkRequest.globalEventOrdinal = static_cast<uint16_t>(globalOrdinal);

    ResolvedRouteChunk chunk{};
    const HarmonyStatus status = planRouteChunk(
        chunkRequest, recent, scratch, candidateWorkspace, chunk);
    if (status != HarmonyStatus::Ready) return status;

    if (current.section == SectionIntent::Return && chunk.count < chunkSize) {
      // The bounded RETURN search returns the exact bridge length. Once home
      // has been reached, preserve the remaining WHEN slots as a home hold.
      for (uint8_t i = 0; i < chunk.count; ++i)
        staged[stagedCount++] = chunk.events[i];
      current.current = request.candidateRequest.home;
      current.currentContext = request.candidateRequest.homeContext;
      while (stagedCount < total) {
        ResolvedHarmonicEvent& hold = staged[stagedCount];
        hold.state = current.current;
        hold.tonal = current.currentContext;
        hold.cadenceClass = CadenceClass::HoldHome;
        hold.timingSlotOrdinal = static_cast<uint8_t>(
            request.firstTimingSlot + stagedCount);
        ++stagedCount;
      }
      break;
    }
    if (chunk.count != chunkSize) return HarmonyStatus::CapacityExceeded;

    for (uint8_t i = 0; i < chunk.count; ++i) {
      const ResolvedHarmonicEvent& event = chunk.events[i];
      staged[stagedCount++] = event;
      current.current = event.state;
      current.currentContext = event.tonal;
      current.previous2 = current.previous1;
      if (canonicalFunctionTokenFor(event.state, event.tonal,
                                    current.previous1) != HarmonyStatus::Ready)
        return HarmonyStatus::Invalid;
    }
    changes = chunk.endingChangesInSection;
    modulations = chunk.endingModulationsInSection;
    globalOrdinal += chunk.count;
  }

  if (total == 0) {
    current.current = request.candidateRequest.current;
    current.currentContext = request.candidateRequest.currentContext;
  }
  for (uint8_t i = 0; i < total; ++i) outEvents[i] = staged[i];
  outCount = stagedCount;
  outTerminal = current.current;
  outTerminalContext = current.currentContext;
  return HarmonyStatus::Ready;
}

HarmonyStatus planGraphHarmony(
    const RouteRequest& request,
    const RecentHarmonyMemory& recent,
    RouteScratch& scratch,
    RouteCandidateWorkspace& candidateWorkspace,
    uint32_t materialId,
    uint32_t publicationGeneration,
    PreparedGraphHarmony& out,
    HarmonicState& outTerminal,
    TonalContext& outTerminalContext) {
  if (materialId == 0 || publicationGeneration == 0 ||
      request.modelVersion == 0 || request.availableTimingSlots == 0 ||
      request.availableTimingSlots > kMaxPreparedGraphEvents ||
      request.firstTimingSlot != 0) {
    return HarmonyStatus::Invalid;
  }

  ResolvedHarmonicEvent events[kMaxPreparedGraphEvents]{};
  uint8_t eventCount = 0;
  HarmonicState terminal{};
  TonalContext terminalContext{};
  const HarmonyStatus status = planRoute(
      request, recent, scratch, candidateWorkspace, events,
      kMaxPreparedGraphEvents, eventCount, terminal, terminalContext);
  if (status != HarmonyStatus::Ready) return status;
  if (eventCount != request.availableTimingSlots || eventCount == 0) {
    return HarmonyStatus::CapacityExceeded;
  }

  PreparedGraphHarmony staged{};
  staged.materialId = materialId;
  staged.publicationGeneration = publicationGeneration;
  staged.modelVersion = request.modelVersion;
  staged.authority = HarmonicAuthority::Graph;
  staged.eventCount = eventCount;
  for (uint8_t i = 0; i < eventCount; ++i) staged.events[i] = events[i];

  out = staged;
  outTerminal = terminal;
  outTerminalContext = terminalContext;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterHarmony
