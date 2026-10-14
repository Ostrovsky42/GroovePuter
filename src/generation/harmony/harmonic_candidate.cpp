#include "harmonic_candidate.h"

#include "harmonic_quality.h"
#include "harmonic_scale.h"

namespace GroovePuterHarmony {
namespace {

bool sameContext(const TonalContext& a, const TonalContext& b) {
  return a.scaleId == b.scaleId && a.tonicPc == b.tonicPc &&
         a.scaleMask == b.scaleMask && a.homeScaleId == b.homeScaleId &&
         a.homeTonicPc == b.homeTonicPc &&
         a.homeScaleMask == b.homeScaleMask;
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

bool sameState(const HarmonicState& a, const HarmonicState& b) {
  return a.rootPc == b.rootPc && a.qualityId == b.qualityId &&
         a.bassPc == b.bassPc && a.pitchMask == b.pitchMask;
}

bool contains(const CandidateList& list, const HarmonicState& state,
              uint8_t contextIndex) {
  for (uint8_t i = 0; i < list.count; ++i) {
    const Candidate& candidate = list.items[i];
    if (sameState(candidate.state, state) &&
        candidate.contextIndex == contextIndex) return true;
  }
  return false;
}

bool containsContext(const TonalCandidateList& list,
                     const TonalContext& context) {
  for (uint8_t i = 0; i < list.count; ++i) {
    if (sameContext(list.items[i], context)) return true;
  }
  return false;
}

bool appendContext(const CandidateRequest& request,
                   const GenreHarmonicPolicy& policy,
                   const TonalContext& context,
                   TonalCandidateList& list) {
  if (list.count >= kMaxTonalCandidates || !validContext(context) ||
      context.scaleId >= 32 ||
      (request.runtimeScaleMask & (1u << context.scaleId)) == 0 ||
      containsContext(list, context)) return false;
  const bool sameAsCurrent = sameContext(context, request.currentContext);
  const uint8_t modulations = static_cast<uint8_t>(
      request.modulationsInSection + (sameAsCurrent ? 0u : 1u));
  const HarmonicCandidatePolicyFacts facts{
      static_cast<uint8_t>(request.current.qualityId),
      static_cast<uint8_t>(context.scaleId), request.section, 0,
      request.changesInSection, modulations, 0, 0,
      static_cast<uint8_t>(sameAsCurrent), request.structuralMotionAllowed,
      0};
  if (!policyAllowsCandidate(policy, facts)) return false;
  list.items[list.count++] = context;
  return true;
}

void addRoot(CandidateScratch& scratch, uint8_t root) {
  root %= 12u;
  if (root > 11) return;
  for (uint8_t i = 0; i < scratch.rootCount; ++i) {
    if (scratch.roots[i] == root) return;
  }
  if (scratch.rootCount < 12) scratch.roots[scratch.rootCount++] = root;
}

void buildRoots(const CandidateRequest& request, CandidateScratch& scratch) {
  scratch.rootCount = 0;
  addRoot(scratch, request.current.rootPc);
  addRoot(scratch, request.home.rootPc);
  for (uint8_t offset = 0; offset < 12; ++offset) {
    if (request.currentContext.scaleMask & (1u << offset))
      addRoot(scratch, static_cast<uint8_t>(request.currentContext.tonicPc + offset));
  }
  for (uint8_t offset = 0; offset < 12; ++offset) {
    if (request.homeContext.scaleMask & (1u << offset))
      addRoot(scratch, static_cast<uint8_t>(request.homeContext.tonicPc + offset));
  }
  static constexpr uint8_t kColorIntervals[] = {7, 5, 4, 3, 9, 1,
                                                 2, 6, 11, 8, 10};
  for (uint8_t interval : kColorIntervals) {
    addRoot(scratch, static_cast<uint8_t>(request.current.rootPc + interval));
    addRoot(scratch, static_cast<uint8_t>(request.home.rootPc + interval));
  }
}

uint8_t qualityShortlist(const CandidateRequest& request,
                         const GenreHarmonicPolicy& policy,
                         const TonalContext& context, uint8_t root,
                         QualityId (&out)[3]) {
  uint8_t count = 0;
  const auto append = [&](QualityId id, uint8_t& mutableCount) {
    if (id >= 32 || id >= kBootstrapQualityCount ||
        (request.runtimeQualityMask & (1u << id)) == 0 ||
        (policy.qualityMask & (1u << id)) == 0) return;
    for (uint8_t i = 0; i < mutableCount; ++i) if (out[i] == id) return;
    if (mutableCount < 3) out[mutableCount++] = id;
  };
  if (root == request.current.rootPc) append(request.current.qualityId, count);
  if (root == request.home.rootPc) append(request.home.qualityId, count);

  // Prefer a scale-contained triad, then fill the remaining bounded shortlist
  // by stable complexity/ID order. The full imported vocabulary is never scanned.
  QualityId bestTriad = 0xffffu;
  uint8_t bestComplexity = 0xffu;
  for (QualityId id = 0; id < kBootstrapQualityCount; ++id) {
    const ChordQualityDefinition* quality = qualityDefinition(id);
    if ((policy.qualityMask & (1u << id)) == 0 ||
        (request.runtimeQualityMask & (1u << id)) == 0 ||
        (transposePitchSet(quality->intervalMask, root) &
         ~context.scaleMask) != 0) continue;
    if (quality->cardinality <= 4 && quality->complexityRank < bestComplexity) {
      bestTriad = id;
      bestComplexity = quality->complexityRank;
    }
  }
  if (bestTriad != 0xffffu) append(bestTriad, count);
  for (uint8_t complexity = 0; complexity <= 4 && count < 3; ++complexity) {
    for (QualityId id = 0; id < kBootstrapQualityCount && count < 3; ++id) {
      const ChordQualityDefinition* quality = qualityDefinition(id);
      if (quality->complexityRank == complexity) append(id, count);
    }
  }
  return count;
}

CanonicalFunctionToken makeToken(const HarmonicState& state,
                                 const TonalContext& context) {
  CanonicalFunctionToken token{};
  const ChordQualityDefinition* quality = qualityDefinition(state.qualityId);
  int8_t rootOffset = static_cast<int8_t>(state.rootPc) -
                      static_cast<int8_t>(context.tonicPc);
  if (rootOffset > 5) rootOffset -= 12;
  if (rootOffset < -6) rootOffset += 12;
  token.rootOffset = rootOffset;
  // These stable groups match the offline codec, not QualityFamily IDs.
  switch (quality->family) {
    case QualityFamily::MajorMinor:
      token.qualityGroup = quality->triadicSkeleton == 0x091u ? 0 :
                           quality->triadicSkeleton == 0x089u ? 1 : 8;
      break;
    case QualityFamily::Dominant: token.qualityGroup = 2; break;
    case QualityFamily::Diminished: token.qualityGroup = 3; break;
    case QualityFamily::Augmented: token.qualityGroup = 4; break;
    case QualityFamily::Suspended:
      token.qualityGroup = quality->triadicSkeleton == 0x0a1u ? 5 :
                           quality->triadicSkeleton == 0x085u ? 6 :
                           quality->triadicSkeleton == 0x081u ? 7 : 8;
      break;
  }
  token.extensionFlags = static_cast<uint16_t>(
      quality->intervalMask & ~quality->triadicSkeleton & kPitchSet12Mask);
  token.slashBassClass = state.bassPc == kNoBassPc
                             ? kNoSlashBassClass
                             : static_cast<uint8_t>((state.bassPc + 12u - state.rootPc) % 12u);
  const uint8_t relativePc = static_cast<uint8_t>((state.rootPc + 12u - context.tonicPc) % 12u);
  token.degree = canonicalRootDegree(relativePc);
  token.alterationFlags = canonicalRootAlteration(relativePc);
  const PitchSet12 skeleton = quality->triadicSkeleton;
  if ((skeleton & (1u << 6u)) && !(skeleton & (1u << 7u))) token.alterationFlags |= 1u << 3u;
  if ((skeleton & (1u << 8u)) && !(skeleton & (1u << 7u))) token.alterationFlags |= 1u << 4u;
  if (skeleton == 0x085u) token.alterationFlags |= 1u << 5u;
  if (skeleton == 0x0a1u) token.alterationFlags |= 1u << 6u;
  if (skeleton == 0x081u) token.alterationFlags |= 1u << 7u;
  return token;
}

}  // namespace

HarmonyStatus collectCandidates(const CandidateRequest& request,
                                CandidateScratch& scratch,
                                CandidateList& candidates,
                                TonalCandidateList& contexts) {
  const GenreHarmonicPolicy* policy = policyFor(request.style);
  if (policy == nullptr || request.structuralMotionAllowed > 1 ||
      static_cast<uint8_t>(request.section) >
          static_cast<uint8_t>(SectionIntent::Return) ||
      !validState(request.current) || !validState(request.home) ||
      !validContext(request.currentContext) || !validContext(request.homeContext)) {
    return HarmonyStatus::Invalid;
  }
  CandidateList nextCandidates{};
  TonalCandidateList nextContexts{};
  if (request.runtimeQualityMask == 0 || request.runtimeScaleMask == 0) {
    candidates = nextCandidates;
    contexts = nextContexts;
    return HarmonyStatus::Ready;
  }

  appendContext(request, *policy, request.currentContext, nextContexts);
  appendContext(request, *policy, request.homeContext, nextContexts);
  const uint8_t tonicStarts[2] = {request.currentContext.tonicPc,
                                  request.homeContext.tonicPc};
  static constexpr uint8_t kRelatedTonicOffsets[] = {7, 5, 9, 2};
  for (uint8_t tonicStart : tonicStarts) {
    for (uint8_t scaleId = 0; scaleId < kBootstrapScaleCount; ++scaleId) {
      if ((policy->scaleMask & (1u << scaleId)) == 0 ||
          (request.runtimeScaleMask & (1u << scaleId)) == 0) continue;
      const ScaleDefinition* scale = scaleDefinition(scaleId);
      TonalContext context = request.currentContext;
      context.scaleId = scaleId;
      context.tonicPc = tonicStart;
      context.scaleMask = transposePitchSet(scale->intervalMask, tonicStart);
      appendContext(request, *policy, context, nextContexts);
      for (uint8_t offset : kRelatedTonicOffsets) {
        if (nextContexts.count >= kMaxTonalCandidates) break;
        context.tonicPc = static_cast<uint8_t>((tonicStart + offset) % 12u);
        context.scaleMask = transposePitchSet(scale->intervalMask, context.tonicPc);
        appendContext(request, *policy, context, nextContexts);
      }
    }
  }

  if (nextContexts.count == 0) {
    candidates = nextCandidates;
    contexts = nextContexts;
    return HarmonyStatus::Ready;
  }
  buildRoots(request, scratch);

  const auto appendState = [&](const HarmonicState& state, uint8_t contextIndex) {
    if (nextCandidates.count >= kMaxHarmonicCandidates ||
        contextIndex >= nextContexts.count ||
        contains(nextCandidates, state, contextIndex)) return;
    HarmonicRelationFeatures features{};
    if (analyzeRelation(request.current, state, request.currentContext,
                        nextContexts.items[contextIndex], scratch.pcvl,
                        features) != HarmonyStatus::Ready) return;
    const uint8_t changes = static_cast<uint8_t>(request.changesInSection +
                                                 (sameState(state, request.current) ? 0u : 1u));
    const bool contextChanged = !sameContext(nextContexts.items[contextIndex],
                                             request.currentContext);
    const uint8_t modulations = static_cast<uint8_t>(request.modulationsInSection +
                                                     (contextChanged ? 1u : 0u));
    const uint8_t usesPortal =
        (features.tags & relationTag(RelationTag::AugmentedPortal)) != 0;
    const HarmonicCandidatePolicyFacts facts{
        static_cast<uint8_t>(state.qualityId),
        static_cast<uint8_t>(nextContexts.items[contextIndex].scaleId),
        request.section, features.outOfScaleTargetTones, changes, modulations,
        0, usesPortal, static_cast<uint8_t>(sameState(state, request.current) &&
            !contextChanged), request.structuralMotionAllowed, 0};
    if (!policyAllowsCandidate(*policy, facts)) return;
    CanonicalFunctionToken target = makeToken(state, nextContexts.items[contextIndex]);
    const uint8_t prior = corpusPrior(request.previous2, request.previous1,
                                      target, request.corpus);
    Candidate& output = nextCandidates.items[nextCandidates.count++];
    output.state = state;
    output.features = features;
    output.corpusPrior = prior;
    output.contextIndex = contextIndex;
  };

  // Reserve HOLD first and exact home for RETURN before optional candidates.
  if ((request.runtimeQualityMask & (1u << request.current.qualityId)) != 0) {
    appendState(request.current, 0);
  }
  if (request.section == SectionIntent::Return) {
    uint8_t homeContextIndex = kMaxTonalCandidates;
    for (uint8_t i = 0; i < nextContexts.count; ++i) {
      if (sameContext(nextContexts.items[i], request.homeContext)) {
        homeContextIndex = i;
        break;
      }
    }
    if (homeContextIndex < nextContexts.count)
      appendState(request.home, homeContextIndex);
  }

  // Fill in stable rounds: first shortlist quality per root/context, then the
  // second and third alternatives. This prevents early roots consuming all 32.
  for (uint8_t rank = 0; rank < 3 && nextCandidates.count < kMaxHarmonicCandidates;
       ++rank) {
    for (uint8_t rootIndex = 0;
         rootIndex < scratch.rootCount && nextCandidates.count < kMaxHarmonicCandidates;
         ++rootIndex) {
      const uint8_t root = scratch.roots[rootIndex];
      for (uint8_t contextIndex = 0;
           contextIndex < nextContexts.count && nextCandidates.count < kMaxHarmonicCandidates;
           ++contextIndex) {
        QualityId shortlist[3]{};
        const uint8_t qualityCount = qualityShortlist(
            request, *policy, nextContexts.items[contextIndex], root, shortlist);
        if (rank >= qualityCount) continue;
        const ChordQualityDefinition* quality = qualityDefinition(shortlist[rank]);
        HarmonicState state{shortlist[rank],
                            transposePitchSet(quality->intervalMask, root),
                            root, kNoBassPc};
        appendState(state, contextIndex);
      }
    }
  }

  candidates = nextCandidates;
  contexts = nextContexts;
  return HarmonyStatus::Ready;
}

HarmonyStatus canonicalFunctionTokenFor(const HarmonicState& state,
                                        const TonalContext& context,
                                        CanonicalFunctionToken& out) {
  if (!validState(state) || !validContext(context)) return HarmonyStatus::Invalid;
  const CanonicalFunctionToken result = makeToken(state, context);
  out = result;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterHarmony
