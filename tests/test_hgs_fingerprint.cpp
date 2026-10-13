#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/harmony/harmonic_fingerprint.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_relation.h"
#include "src/generation/harmony/harmonic_scale.h"

using namespace GroovePuterHarmony;

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

void makeRoute(uint8_t tonic, ResolvedHarmonicEvent (&events)[4],
               ResolvedRouteView& view, ScaleId scaleId = 1) {
  const uint8_t roots[4] = {tonic, static_cast<uint8_t>((tonic + 9) % 12),
                            static_cast<uint8_t>((tonic + 5) % 12),
                            static_cast<uint8_t>((tonic + 7) % 12)};
  const QualityId qualities[4] = {kQualityMajor, kQualityMinor,
                                  kQualityMajor, kQualityMajor};
  for (uint8_t i = 0; i < 4; ++i) {
    events[i].state = state(roots[i], qualities[i]);
    events[i].tonal = context(tonic, scaleId);
    events[i].relationTags = i == 3 ? relationTag(RelationTag::DiatonicFunctional) : 0;
    events[i].cadenceClass = i == 3 ? CadenceClass::DirectHome : CadenceClass::None;
    events[i].timingSlotOrdinal = i * 4;
  }
  view = {events, 4, state(tonic, kQualityMajor), context(tonic, scaleId)};
}

}  // namespace

int main() {
  static_assert(sizeof(HarmonicFingerprintSet) == 40);
  static_assert(sizeof(RecentHarmonyMemory) <= 336);

  ResolvedHarmonicEvent cEvents[4]{};
  ResolvedHarmonicEvent dEvents[4]{};
  ResolvedRouteView cRoute{};
  ResolvedRouteView dRoute{};
  makeRoute(0, cEvents, cRoute);
  makeRoute(2, dEvents, dRoute);
  HarmonicFingerprintSet c{};
  HarmonicFingerprintSet d{};
  assert(fingerprintRoute(cRoute, c) == HarmonyStatus::Ready);
  assert(fingerprintRoute(dRoute, d) == HarmonyStatus::Ready);
  assert(c.rootMotion == d.rootMotion);
  assert(c.qualitySequence == d.qualitySequence);
  assert(c.semanticRelation == d.semanticRelation);
  assert(c.cadence == d.cadence);
  assert(c.scaleContext == d.scaleContext);

  ResolvedHarmonicEvent altered[4];
  for (uint8_t i = 0; i < 4; ++i) altered[i] = cEvents[i];
  altered[1].state = state(9, kQualityMajor);
  ResolvedRouteView alteredRoute{altered, 4, cRoute.home, cRoute.homeTonal};
  HarmonicFingerprintSet alteredFp{};
  assert(fingerprintRoute(alteredRoute, alteredFp) == HarmonyStatus::Ready);
  assert(alteredFp.rootMotion == c.rootMotion);
  assert(alteredFp.qualitySequence != c.qualitySequence);

  ResolvedHarmonicEvent modal[4];
  for (uint8_t i = 0; i < 4; ++i) modal[i] = cEvents[i];
  for (uint8_t i = 0; i < 4; ++i) modal[i].tonal = context(0, 2);
  ResolvedRouteView modalRoute{modal, 4, cRoute.home, context(0, 2)};
  HarmonicFingerprintSet modalFp{};
  assert(fingerprintRoute(modalRoute, modalFp) == HarmonyStatus::Ready);
  assert(modalFp.rootMotion == c.rootMotion);
  assert(modalFp.qualitySequence == c.qualitySequence);
  assert(modalFp.scaleContext != c.scaleContext);
  assert(modalFp.semanticRelation != c.semanticRelation);

  ResolvedHarmonicEvent cadence[4];
  for (uint8_t i = 0; i < 4; ++i) cadence[i] = cEvents[i];
  cadence[3].cadenceClass = CadenceClass::DominantHome;
  ResolvedRouteView cadenceRoute{cadence, 4, cRoute.home, cRoute.homeTonal};
  HarmonicFingerprintSet cadenceFp{};
  assert(fingerprintRoute(cadenceRoute, cadenceFp) == HarmonyStatus::Ready);
  assert(cadenceFp.cadence != c.cadence);

  HarmonicFingerprintSet sentinel{1, 2, 3, 4, 5};
  ResolvedRouteView invalid = cRoute;
  invalid.count = 33;
  assert(fingerprintRoute(invalid, sentinel) == HarmonyStatus::Invalid);
  assert(sentinel.rootMotion == 1 && sentinel.scaleContext == 5);
  invalid = cRoute;
  invalid.events = nullptr;
  assert(fingerprintRoute(invalid, sentinel) == HarmonyStatus::Invalid);
  invalid = cRoute;
  cadence[1].timingSlotOrdinal = cadence[0].timingSlotOrdinal;
  invalid.events = cadence;
  assert(fingerprintRoute(invalid, sentinel) == HarmonyStatus::Invalid);
  assert(sentinel.rootMotion == 1 && sentinel.scaleContext == 5);

  RecentHarmonyMemory memory{};
  const RecentHarmonyMemory emptyMemory = memory;
  const uint8_t previewPenalty = harmonicRepeatPenalty(memory, c, 0);
  assert(previewPenalty == 0);
  assert(memory.count == emptyMemory.count && memory.next == emptyMemory.next);
  commitFingerprint(memory, c);
  assert(memory.count == 1);
  assert(harmonicRepeatPenalty(memory, c, 0) == 12);
  assert(harmonicRepeatPenalty(memory, d, 0) == 12);
  for (uint8_t i = 0; i < 8; ++i) {
    HarmonicFingerprintSet distinct = c;
    distinct.rootMotion += i + 1;
    distinct.qualitySequence += i + 1;
    distinct.semanticRelation += i + 1;
    distinct.cadence += i + 1;
    distinct.scaleContext += i + 1;
    commitFingerprint(memory, distinct);
  }
  assert(memory.count == 8 && memory.next == 1);
  assert(harmonicRepeatPenalty(memory, c, 0) == 0);
  resetRecentHarmonyMemory(memory);
  assert(memory.count == 0 && memory.next == 0);

  RecentHarmonyMemory repeatMemory{};
  commitFingerprint(repeatMemory, c);
  assert(harmonicRepeatPenalty(repeatMemory, c, 255) == 0);

  std::cout << "HGS harmonic fingerprints: PASS\n";
  return 0;
}
