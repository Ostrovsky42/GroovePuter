#include "harmonic_relation.h"

namespace GroovePuterHarmony {
namespace {

constexpr PitchSet12 kMajorTriadSkeleton = 0x0091u;
constexpr PitchSet12 kMinorTriadSkeleton = 0x0089u;

bool validState(const HarmonicState& state,
                const ChordQualityDefinition*& quality) {
  if (state.rootPc >= 12 ||
      (state.bassPc != kNoBassPc && state.bassPc >= 12) ||
      !isValidPitchSet12(state.pitchMask)) {
    return false;
  }
  quality = qualityDefinition(state.qualityId);
  if (quality == nullptr || quality->cardinality == 0 ||
      !isValidPitchSet12(quality->intervalMask) ||
      !isValidPitchSet12(quality->triadicSkeleton)) {
    return false;
  }
  return state.pitchMask == transposePitchSet(quality->intervalMask, state.rootPc);
}

bool validContext(const TonalContext& context) {
  if (context.tonicPc >= 12 || context.homeTonicPc >= 12 ||
      !isValidPitchSet12(context.scaleMask) ||
      !isValidPitchSet12(context.homeScaleMask)) {
    return false;
  }
  const ScaleDefinition* scale = scaleDefinition(context.scaleId);
  const ScaleDefinition* homeScale = scaleDefinition(context.homeScaleId);
  return scale != nullptr && homeScale != nullptr &&
         context.scaleMask == transposePitchSet(scale->intervalMask, context.tonicPc) &&
         context.homeScaleMask ==
             transposePitchSet(homeScale->intervalMask, context.homeTonicPc);
}

bool sameScale(const TonalContext& a, const TonalContext& b) {
  return a.scaleId == b.scaleId && a.tonicPc == b.tonicPc &&
         a.scaleMask == b.scaleMask;
}

bool sameTonic(const TonalContext& a, const TonalContext& b) {
  return a.tonicPc == b.tonicPc;
}

bool subset(PitchSet12 pitches, PitchSet12 scale) {
  return (pitches & static_cast<PitchSet12>(~scale)) == 0;
}

uint8_t countOutside(PitchSet12 pitches, PitchSet12 scale) {
  return popcountPitchSet12(static_cast<PitchSet12>(
      pitches & static_cast<PitchSet12>(~scale)));
}

uint8_t intervalVectorDistance(PitchSet12 a, PitchSet12 b) {
  IntervalClassVector left{};
  IntervalClassVector right{};
  (void)intervalClassVector(a, left);
  (void)intervalClassVector(b, right);
  uint8_t distance = 0;
  for (uint8_t index = 0; index < 6; ++index) {
    const uint8_t delta = left.counts[index] > right.counts[index]
                              ? static_cast<uint8_t>(left.counts[index] -
                                                     right.counts[index])
                              : static_cast<uint8_t>(right.counts[index] -
                                                     left.counts[index]);
    distance = static_cast<uint8_t>(distance + delta);
  }
  return distance;
}

enum class TriadKind : uint8_t { None, Major, Minor };

TriadKind triadKind(const ChordQualityDefinition& quality) {
  if (quality.family != QualityFamily::MajorMinor) return TriadKind::None;
  if (quality.triadicSkeleton == kMajorTriadSkeleton) return TriadKind::Major;
  if (quality.triadicSkeleton == kMinorTriadSkeleton) return TriadKind::Minor;
  return TriadKind::None;
}

bool prlExtensionsCompatible(const HarmonicState& from,
                             const ChordQualityDefinition& fromQuality,
                             const HarmonicState& to,
                             const ChordQualityDefinition& toQuality,
                             const TonalContext& sourceContext,
                             const TonalContext& targetContext) {
  PitchSet12 fromSkeleton = transposePitchSet(fromQuality.triadicSkeleton,
                                               from.rootPc);
  PitchSet12 toSkeleton = transposePitchSet(toQuality.triadicSkeleton, to.rootPc);
  PitchSet12 fromExtensions =
      static_cast<PitchSet12>(from.pitchMask & static_cast<PitchSet12>(~fromSkeleton));
  PitchSet12 toExtensions =
      static_cast<PitchSet12>(to.pitchMask & static_cast<PitchSet12>(~toSkeleton));
  const PitchSet12 unshared =
      static_cast<PitchSet12>((fromExtensions ^ toExtensions) &
                              static_cast<PitchSet12>(~(fromExtensions & toExtensions)));
  const PitchSet12 contextual =
      static_cast<PitchSet12>(sourceContext.scaleMask | targetContext.scaleMask);
  return subset(unshared, contextual);
}

bool isPrlMatch(TriadKind fromKind,
                TriadKind toKind,
                uint8_t rootDelta,
                RelationTag& out) {
  if (fromKind == toKind || fromKind == TriadKind::None ||
      toKind == TriadKind::None) {
    return false;
  }
  if (rootDelta == 0) {
    out = RelationTag::PrlP;
    return true;
  }
  if (fromKind == TriadKind::Major) {
    if (rootDelta == 9) {
      out = RelationTag::PrlR;
      return true;
    }
    if (rootDelta == 4) {
      out = RelationTag::PrlL;
      return true;
    }
  } else {
    if (rootDelta == 3) {
      out = RelationTag::PrlR;
      return true;
    }
    if (rootDelta == 8) {
      out = RelationTag::PrlL;
      return true;
    }
  }
  return false;
}

bool hasParallelBorrowedTarget(const HarmonicState& target,
                               const TonalContext& context,
                               bool contextIsSame) {
  if (!contextIsSame) return false;
  ScaleId parallelId;
  if (context.scaleId == 0) {
    parallelId = 1;
  } else if (context.scaleId == 1) {
    parallelId = 0;
  } else {
    return false;
  }
  const ScaleDefinition* parallel = scaleDefinition(parallelId);
  if (parallel == nullptr) return false;
  const PitchSet12 parallelMask =
      transposePitchSet(parallel->intervalMask, context.tonicPc);
  return countOutside(target.pitchMask, context.scaleMask) > 0 &&
         subset(target.pitchMask, parallelMask);
}

bool isChromaticMediantRootDelta(uint8_t rootDelta) {
  return rootDelta == 3 || rootDelta == 4 || rootDelta == 8 || rootDelta == 9;
}

bool augmentedPortal(const HarmonicState& from,
                     const ChordQualityDefinition& fromQuality,
                     const HarmonicState& to,
                     const ChordQualityDefinition& toQuality,
                     uint8_t commonCount,
                     uint8_t rootDelta) {
  if (commonCount < 2 || !isChromaticMediantRootDelta(rootDelta)) return false;
  const bool fromPortal = fromQuality.family == QualityFamily::Augmented &&
                          (from.pitchMask & (1u << to.rootPc)) != 0;
  const bool toPortal = toQuality.family == QualityFamily::Augmented &&
                        (to.pitchMask & (1u << from.rootPc)) != 0;
  return fromPortal || toPortal;
}

bool diminishedPassing(const ChordQualityDefinition& fromQuality,
                       const ChordQualityDefinition& toQuality,
                       uint8_t rootSemitoneDistance,
                       uint8_t commonCount) {
  const bool diminished =
      fromQuality.family == QualityFamily::Diminished ||
      toQuality.family == QualityFamily::Diminished;
  return diminished && rootSemitoneDistance == 1 && commonCount > 0;
}

}  // namespace

HarmonyStatus analyzeRelation(const HarmonicState& from,
                              const HarmonicState& to,
                              const TonalContext& sourceContext,
                              const TonalContext& targetContext,
                              PcVoiceLeadingScratch& scratch,
                              HarmonicRelationFeatures& out) {
  const ChordQualityDefinition* fromQuality = nullptr;
  const ChordQualityDefinition* toQuality = nullptr;
  if (!validState(from, fromQuality) || !validState(to, toQuality) ||
      !validContext(sourceContext) || !validContext(targetContext)) {
    return HarmonyStatus::Invalid;
  }

  HarmonicRelationFeatures result{};
  if (commonTones(from.pitchMask, to.pitchMask, result.commonToneCount) !=
          HarmonyStatus::Ready ||
      setDifference(from.pitchMask, to.pitchMask, result.setDifferenceCount) !=
          HarmonyStatus::Ready ||
      pcVoiceLeadingCost(from.pitchMask, to.pitchMask, scratch,
                         result.pcVoiceLeadingCost) != HarmonyStatus::Ready) {
    return HarmonyStatus::Invalid;
  }
  result.intervalVectorDistance =
      intervalVectorDistance(from.pitchMask, to.pitchMask);
  if (rootSemitoneDistance(from.rootPc, to.rootPc,
                           result.rootSemitoneDistance) != HarmonyStatus::Ready ||
      rootFifthsDistance(from.rootPc, to.rootPc,
                         result.rootFifthsDistance) != HarmonyStatus::Ready) {
    return HarmonyStatus::Invalid;
  }
  result.sourceOutOfScaleTones =
      countOutside(from.pitchMask, sourceContext.scaleMask);
  result.outOfScaleTargetTones =
      countOutside(to.pitchMask, targetContext.scaleMask);
  const int16_t complexity =
      static_cast<int16_t>(toQuality->complexityRank) - fromQuality->complexityRank;
  result.complexityDelta = static_cast<int8_t>(
      complexity < -127 ? -127 : (complexity > 127 ? 127 : complexity));
  const uint8_t fromBass =
      from.bassPc == kNoBassPc ? from.rootPc : from.bassPc;
  const uint8_t toBass = to.bassPc == kNoBassPc ? to.rootPc : to.bassPc;
  if (rootSemitoneDistance(fromBass, toBass, result.bassMotionDistance) !=
      HarmonyStatus::Ready) {
    return HarmonyStatus::Invalid;
  }

  const bool sameScaleContext = sameScale(sourceContext, targetContext);
  const bool sameTonicContext = sameTonic(sourceContext, targetContext);
  if (sameScaleContext) result.contextFlags |= RelationSameScale;
  if (sameTonicContext) result.contextFlags |= RelationSameTonic;
  if (commonTones(sourceContext.scaleMask, targetContext.scaleMask,
                  result.scaleCommonTones) != HarmonyStatus::Ready ||
      setDifference(sourceContext.scaleMask, targetContext.scaleMask,
                    result.scaleDifferenceCount) != HarmonyStatus::Ready) {
    return HarmonyStatus::Invalid;
  }

  const PitchSet12 scaleIntersection =
      static_cast<PitchSet12>(sourceContext.scaleMask & targetContext.scaleMask);
  const uint8_t fromPivotCoverage =
      popcountPitchSet12(static_cast<PitchSet12>(from.pitchMask & scaleIntersection));
  const uint8_t toPivotCoverage =
      popcountPitchSet12(static_cast<PitchSet12>(to.pitchMask & scaleIntersection));
  result.pivotCompatibility =
      fromPivotCoverage > toPivotCoverage ? fromPivotCoverage : toPivotCoverage;

  const uint8_t rootDelta =
      static_cast<uint8_t>((to.rootPc + 12 - from.rootPc) % 12);
  const TriadKind fromKind = triadKind(*fromQuality);
  const TriadKind toKind = triadKind(*toQuality);
  RelationTag prl = RelationTag::PrlP;
  if (prlExtensionsCompatible(from, *fromQuality, to, *toQuality,
                              sourceContext, targetContext) &&
      isPrlMatch(fromKind, toKind, rootDelta, prl)) {
    result.tags |= relationTag(prl);
  }

  if (sameScaleContext && from.rootPc != to.rootPc &&
      subset(from.pitchMask, sourceContext.scaleMask) &&
      subset(to.pitchMask, targetContext.scaleMask)) {
    result.tags |= relationTag(RelationTag::DiatonicFunctional);
  }

  const bool dominantToTarget =
      fromQuality->family == QualityFamily::Dominant &&
      from.rootPc == static_cast<uint8_t>((to.rootPc + 7) % 12);
  if (dominantToTarget) {
    result.tags |= relationTag(RelationTag::DominantToTarget);
    if (result.sourceOutOfScaleTones > 0) {
      result.tags |= relationTag(RelationTag::SecondaryDominant);
    }
  }

  if (hasParallelBorrowedTarget(to, targetContext, sameScaleContext)) {
    result.tags |= relationTag(RelationTag::BorrowedParallel);
  }
  if (sameTonicContext && !sameScaleContext &&
      subset(from.pitchMask, sourceContext.scaleMask) &&
      subset(to.pitchMask, targetContext.scaleMask) &&
      countOutside(to.pitchMask, sourceContext.scaleMask) > 0) {
    result.tags |= relationTag(RelationTag::ModalMixture);
  }

  if (isChromaticMediantRootDelta(rootDelta) &&
      fromKind != TriadKind::None && toKind != TriadKind::None &&
      (result.sourceOutOfScaleTones > 0 ||
       result.outOfScaleTargetTones > 0)) {
    result.tags |= relationTag(RelationTag::ChromaticMediant);
  }
  if (result.rootSemitoneDistance == 6) {
    result.tags |= relationTag(RelationTag::TritoneRoot);
  }
  if (augmentedPortal(from, *fromQuality, to, *toQuality,
                      result.commonToneCount, rootDelta)) {
    result.tags |= relationTag(RelationTag::AugmentedPortal);
  }
  if (diminishedPassing(*fromQuality, *toQuality,
                        result.rootSemitoneDistance, result.commonToneCount)) {
    result.tags |= relationTag(RelationTag::DiminishedPassing);
  }
  if (result.commonToneCount >= 3) {
    result.tags |= relationTag(RelationTag::CommonToneHigh);
  }
  if (result.rootFifthsDistance == 1) {
    result.tags |= relationTag(RelationTag::FifthRelated);
  }
  const bool fromIsPivot = subset(from.pitchMask, scaleIntersection);
  const bool toIsPivot = subset(to.pitchMask, scaleIntersection);
  if (!sameScaleContext && (fromIsPivot || toIsPivot)) {
    result.tags |= relationTag(RelationTag::PivotChord);
  }

  out = result;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterHarmony
