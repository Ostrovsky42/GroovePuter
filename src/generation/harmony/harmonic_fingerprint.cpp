#include "harmonic_fingerprint.h"

#include "harmonic_quality.h"
#include "harmonic_scale.h"

namespace GroovePuterHarmony {
namespace {

constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;
constexpr uint8_t kFingerprintVersion = 1;

enum class FingerprintKind : uint8_t {
  RootMotion = 1,
  QualitySequence = 2,
  SemanticRelation = 3,
  Cadence = 4,
  ScaleContext = 5,
};

struct HashWriter {
  uint64_t value = kFnvOffset;

  void byte(uint8_t valueByte) {
    value ^= valueByte;
    value *= kFnvPrime;
  }

  void word16(uint16_t word) {
    byte(static_cast<uint8_t>(word));
    byte(static_cast<uint8_t>(word >> 8u));
  }

};

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

int8_t signedPcOffset(uint8_t pitch, uint8_t reference) {
  int8_t result = static_cast<int8_t>(pitch) - static_cast<int8_t>(reference);
  if (result > 5) result -= 12;
  if (result < -6) result += 12;
  return result;
}

void begin(HashWriter& hash, FingerprintKind kind, uint8_t count) {
  hash.byte(kFingerprintVersion);
  hash.byte(static_cast<uint8_t>(kind));
  hash.byte(count);
}

uint8_t relativeSlot(const ResolvedRouteView& route, uint8_t index) {
  return static_cast<uint8_t>(route.events[index].timingSlotOrdinal -
                              route.events[0].timingSlotOrdinal);
}

uint64_t fingerprintRootMotion(const ResolvedRouteView& route) {
  HashWriter hash{};
  begin(hash, FingerprintKind::RootMotion, route.count);
  for (uint8_t i = 0; i < route.count; ++i) {
    const ResolvedHarmonicEvent& event = route.events[i];
    hash.byte(static_cast<uint8_t>(signedPcOffset(event.state.rootPc,
                                                  route.home.rootPc)));
    const uint8_t bass = event.state.bassPc == kNoBassPc
                             ? 0x80u
                             : static_cast<uint8_t>(signedPcOffset(
                                   event.state.bassPc, route.home.rootPc));
    hash.byte(bass);
    hash.byte(relativeSlot(route, i));
  }
  return hash.value;
}

uint64_t fingerprintQuality(const ResolvedRouteView& route) {
  HashWriter hash{};
  begin(hash, FingerprintKind::QualitySequence, route.count);
  for (uint8_t i = 0; i < route.count; ++i) {
    hash.word16(route.events[i].state.qualityId);
    hash.byte(relativeSlot(route, i));
  }
  return hash.value;
}

uint64_t fingerprintSemantic(const ResolvedRouteView& route) {
  HashWriter hash{};
  begin(hash, FingerprintKind::SemanticRelation, route.count);
  for (uint8_t i = 0; i < route.count; ++i) {
    const ResolvedHarmonicEvent& event = route.events[i];
    hash.word16(event.relationTags);
    hash.byte(static_cast<uint8_t>(signedPcOffset(event.state.rootPc,
                                                  event.tonal.tonicPc)));
    hash.byte(event.tonal.scaleId);
    hash.byte(static_cast<uint8_t>(signedPcOffset(
        event.tonal.tonicPc, route.homeTonal.tonicPc)));
    hash.byte(relativeSlot(route, i));
  }
  return hash.value;
}

uint64_t fingerprintCadence(const ResolvedRouteView& route) {
  HashWriter hash{};
  begin(hash, FingerprintKind::Cadence, route.count);
  for (uint8_t i = 0; i < route.count; ++i) {
    hash.byte(static_cast<uint8_t>(route.events[i].cadenceClass));
    hash.byte(relativeSlot(route, i));
  }
  return hash.value;
}

uint64_t fingerprintScaleContext(const ResolvedRouteView& route) {
  HashWriter hash{};
  begin(hash, FingerprintKind::ScaleContext, route.count);
  hash.byte(route.homeTonal.scaleId);
  for (uint8_t i = 0; i < route.count; ++i) {
    const TonalContext& tonal = route.events[i].tonal;
    hash.byte(tonal.scaleId);
    hash.byte(static_cast<uint8_t>(signedPcOffset(
        tonal.tonicPc, route.homeTonal.tonicPc)));
    hash.byte(tonal.homeScaleId);
    hash.byte(static_cast<uint8_t>(signedPcOffset(
        tonal.homeTonicPc, route.homeTonal.tonicPc)));
    hash.byte(relativeSlot(route, i));
  }
  return hash.value;
}

bool validMemory(const RecentHarmonyMemory& memory) {
  return memory.count <= 8 && memory.next < 8 && memory.edgeCount <= 8 &&
         memory.edgeNext < 8;
}

uint8_t scaledWeight(uint8_t weight, uint8_t tolerance) {
  return static_cast<uint8_t>((static_cast<uint16_t>(weight) *
                               (255u - tolerance) + 127u) / 255u);
}

}  // namespace

HarmonyStatus fingerprintRoute(const ResolvedRouteView& route,
                              HarmonicFingerprintSet& out) {
  if (route.count > 32 || (route.count > 0 && route.events == nullptr) ||
      !validState(route.home) || !validContext(route.homeTonal)) {
    return HarmonyStatus::Invalid;
  }
  uint8_t previousSlot = 0;
  for (uint8_t i = 0; i < route.count; ++i) {
    const ResolvedHarmonicEvent& event = route.events[i];
    if (!validState(event.state) || !validContext(event.tonal) ||
        static_cast<uint8_t>(event.cadenceClass) >=
            static_cast<uint8_t>(CadenceClass::Count) ||
        (event.relationTags & 0x8000u) != 0 ||
        event.timingSlotOrdinal >= 32 ||
        (i > 0 && event.timingSlotOrdinal <= previousSlot) ||
        event.tonal.homeScaleId != route.homeTonal.scaleId ||
        event.tonal.homeTonicPc != route.homeTonal.tonicPc) {
      return HarmonyStatus::Invalid;
    }
    previousSlot = event.timingSlotOrdinal;
  }

  HarmonicFingerprintSet result{};
  result.rootMotion = fingerprintRootMotion(route);
  result.qualitySequence = fingerprintQuality(route);
  result.semanticRelation = fingerprintSemantic(route);
  result.cadence = fingerprintCadence(route);
  result.scaleContext = fingerprintScaleContext(route);
  out = result;
  return HarmonyStatus::Ready;
}

uint8_t harmonicRepeatPenalty(const RecentHarmonyMemory& memory,
                              const HarmonicFingerprintSet& candidate,
                              uint8_t repetitionTolerance) {
  if (!validMemory(memory)) return 0;
  static constexpr uint8_t kWeights[] = {4, 3, 2, 2, 1};
  uint16_t penalty = 0;
  for (uint8_t age = 0; age < memory.count; ++age) {
    const uint8_t index = static_cast<uint8_t>(
        (memory.next + 8u - 1u - age) % 8u);
    const HarmonicFingerprintSet& prior = memory.entries[index];
    if (prior.rootMotion == candidate.rootMotion)
      penalty += scaledWeight(kWeights[0], repetitionTolerance);
    if (prior.semanticRelation == candidate.semanticRelation)
      penalty += scaledWeight(kWeights[1], repetitionTolerance);
    if (prior.qualitySequence == candidate.qualitySequence)
      penalty += scaledWeight(kWeights[2], repetitionTolerance);
    if (prior.cadence == candidate.cadence)
      penalty += scaledWeight(kWeights[3], repetitionTolerance);
    if (prior.scaleContext == candidate.scaleContext)
      penalty += scaledWeight(kWeights[4], repetitionTolerance);
  }
  return penalty > 255 ? 255 : static_cast<uint8_t>(penalty);
}

uint8_t harmonicEdgeToken(const HarmonicState& from,
                          const TonalContext& fromTonal,
                          const HarmonicState& to,
                          const TonalContext& toTonal) {
  if (!validState(from) || !validState(to) || !validContext(fromTonal) ||
      !validContext(toTonal)) return 0;
  HashWriter hash{};
  hash.byte(kFingerprintVersion);
  hash.byte(static_cast<uint8_t>(signedPcOffset(to.rootPc, from.rootPc)));
  hash.word16(from.qualityId);
  hash.word16(to.qualityId);
  hash.byte(fromTonal.scaleId);
  hash.byte(toTonal.scaleId);
  hash.byte(static_cast<uint8_t>(signedPcOffset(toTonal.tonicPc,
                                                fromTonal.tonicPc)));
  return static_cast<uint8_t>(hash.value);
}

uint8_t harmonicRecentEdgePenalty(const RecentHarmonyMemory& memory,
                                  uint8_t edgeToken,
                                  uint8_t repetitionTolerance) {
  if (!validMemory(memory)) return 0;
  const uint8_t weight = scaledWeight(2, repetitionTolerance);
  uint16_t penalty = 0;
  for (uint8_t age = 0; age < memory.edgeCount; ++age) {
    const uint8_t index = static_cast<uint8_t>(
        (memory.edgeNext + 8u - 1u - age) % 8u);
    if (memory.recentEdges[index] == edgeToken) penalty += weight;
  }
  return penalty > 255 ? 255 : static_cast<uint8_t>(penalty);
}

void commitFingerprint(RecentHarmonyMemory& memory,
                       const HarmonicFingerprintSet& accepted) {
  if (!validMemory(memory)) resetRecentHarmonyMemory(memory);
  memory.entries[memory.next] = accepted;
  memory.next = static_cast<uint8_t>((memory.next + 1u) % 8u);
  if (memory.count < 8) ++memory.count;
}

HarmonyStatus commitAcceptedRoute(RecentHarmonyMemory& memory,
                                 const ResolvedRouteView& accepted) {
  HarmonicFingerprintSet fingerprint{};
  const HarmonyStatus status = fingerprintRoute(accepted, fingerprint);
  if (status != HarmonyStatus::Ready) return status;
  commitFingerprint(memory, fingerprint);
  for (uint8_t i = 1; i < accepted.count; ++i) {
    commitRecentEdge(memory, harmonicEdgeToken(
        accepted.events[i - 1].state, accepted.events[i - 1].tonal,
        accepted.events[i].state, accepted.events[i].tonal));
  }
  return HarmonyStatus::Ready;
}

void commitRecentEdge(RecentHarmonyMemory& memory, uint8_t edgeToken) {
  if (!validMemory(memory)) resetRecentHarmonyMemory(memory);
  memory.recentEdges[memory.edgeNext] = edgeToken;
  memory.edgeNext = static_cast<uint8_t>((memory.edgeNext + 1u) % 8u);
  if (memory.edgeCount < 8) ++memory.edgeCount;
}

void resetRecentHarmonyMemory(RecentHarmonyMemory& memory) {
  memory = RecentHarmonyMemory{};
}

}  // namespace GroovePuterHarmony
