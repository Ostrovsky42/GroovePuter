#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/harmony/harmonic_source.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/tonal/tonal_projector.h"

using namespace GroovePuterHarmony;
using namespace GroovePuterRhythm;

namespace {
HarmonicState state(uint8_t root, QualityId qualityId,
                    uint8_t bassPc = kNoBassPc) {
  const auto* quality = qualityDefinition(qualityId);
  return {qualityId, transposePitchSet(quality->intervalMask, root), root,
          bassPc};
}

TonalContext context(uint8_t tonic, ScaleId scaleId) {
  const auto* scale = scaleDefinition(scaleId);
  return {scaleId, scaleId, transposePitchSet(scale->intervalMask, tonic),
          transposePitchSet(scale->intervalMask, tonic), tonic, tonic};
}

uint8_t noteRoot(const HarmonicEvent& event, ScaleTypeValue scale,
                 uint8_t tonic) {
  int pc = static_cast<int>(tonic) + scaleDegreeToSemitone(scale, event.degree) +
           event.rootOffsetSemitones;
  pc %= 12;
  if (pc < 0) pc += 12;
  return static_cast<uint8_t>(pc);
}
}  // namespace

int main() {
  const MaterializerCapabilityView caps = materializerCapabilityView();
  assert(caps.supportsSlashBass == 0);
  assert((caps.qualityMask & (1u << kQualityMajor)) != 0);
  assert((caps.qualityMask & (1u << 1)) == 0);
  assert(caps.scaleMask == 0x03ffu);

  const QualityId projectable[] = {kQualityMajor, kQualityDominant7,
      kQualityDiminished, kQualitySus4, kQualityMinor7, kQualityMajor7,
      kQualityMinor9, kQualityMajor9};
  for (ScaleId scaleId = 0; scaleId < kBootstrapScaleCount; ++scaleId) {
    for (uint8_t tonic = 0; tonic < 12; ++tonic) {
      const TonalContext tonal = context(tonic, scaleId);
      for (QualityId qualityId : projectable) {
        for (uint8_t root = 0; root < 12; ++root) {
          const HarmonicState source = state(root, qualityId);
          HarmonicEvent projected{};
          const HarmonyStatus projectionStatus =
              projectHarmonicState(source, tonal, projected);
          const uint8_t rootFromHome =
              static_cast<uint8_t>((root + 12u - tonic) % 12u);
          const bool supported =
              (caps.rootMaskByHomeScale[scaleId] & (1u << rootFromHome)) != 0;
          assert(projectionStatus == (supported ? HarmonyStatus::Ready
                                                : HarmonyStatus::Unsupported));
          if (!supported) continue;
          assert(projected.degree <= 6);
          assert(projected.rootOffsetSemitones >= -2 &&
                 projected.rootOffsetSemitones <= 2);
          assert(noteRoot(projected, tonal.homeScaleId,
                          tonal.homeTonicPc) == source.rootPc);
          QualityId roundTripQuality = 0xffffu;
          assert(legacyQualityToId(projected.quality, roundTripQuality));
          assert(roundTripQuality == source.qualityId);
        }
      }
    }
  }

  HarmonicEvent sentinel{};
  sentinel.degree = 5;
  sentinel.rootOffsetSemitones = -1;
  assert(projectHarmonicState(state(0, 1), context(0, 1), sentinel) ==
         HarmonyStatus::Unsupported);  // Legacy carrier has no minor triad.
  assert(sentinel.degree == 5 && sentinel.rootOffsetSemitones == -1);
  assert(projectHarmonicState(state(0, kQualityAugmented), context(0, 1), sentinel) ==
         HarmonyStatus::Unsupported);
  assert(projectHarmonicState(state(0, kQualityMajor, 7), context(0, 1), sentinel) ==
         HarmonyStatus::Unsupported);
  assert((caps.rootMaskByHomeScale[9] & (1u << 9)) == 0);
  assert(projectHarmonicState(state(9, kQualityMajor), context(0, 9), sentinel) ==
         HarmonyStatus::Unsupported);
  HarmonicState malformed = state(0, kQualityMajor);
  malformed.pitchMask ^= 1u;
  assert(projectHarmonicState(malformed, context(0, 1), sentinel) ==
         HarmonyStatus::Invalid);
  assert(sentinel.degree == 5 && sentinel.rootOffsetSemitones == -1);

  PreparedGraphHarmony graph{};
  graph.materialId = 10;
  graph.publicationGeneration = 2;
  graph.modelVersion = 7;
  graph.authority = HarmonicAuthority::Graph;
  graph.eventCount = 6;
  for (uint8_t i = 0; i < graph.eventCount; ++i) {
    graph.events[i].state = state(i, kQualityMajor);
    graph.events[i].tonal = context(0, 1);
    graph.events[i].timingSlotOrdinal = i * 2;
  }
  HarmonicSourceView graphView{HarmonicStrategy::Graph, nullptr, &graph};
  HarmonicEvent fifth{};
  assert(harmonicEventAt(graphView, 4, fifth) == HarmonyStatus::Ready);
  assert(noteRoot(fifth, 1, 0) == 4);
  assert(harmonicEventAt(graphView, 5, fifth) == HarmonyStatus::Ready);
  assert(noteRoot(fifth, 1, 0) == 5);
  const HarmonicEvent beforeOutOfRange = fifth;
  assert(harmonicEventAt(graphView, 6, fifth) == HarmonyStatus::Invalid);
  assert(fifth.degree == beforeOutOfRange.degree &&
         fifth.rootOffsetSemitones == beforeOutOfRange.rootOffsetSemitones);
  PreparedGraphHarmony untrustedGraph = graph;
  untrustedGraph.authority = HarmonicAuthority::Unknown;
  HarmonicSourceView untrustedView{HarmonicStrategy::Graph, nullptr,
                                   &untrustedGraph};
  assert(harmonicEventAt(untrustedView, 0, fifth) == HarmonyStatus::Invalid);
  untrustedGraph = graph;
  untrustedGraph.modelVersion = 0;
  assert(harmonicEventAt(untrustedView, 0, fifth) == HarmonyStatus::Invalid);

  ChordProgressionSource legacy{};
  legacy.id = ProgressionId::PopCycle;
  legacy.period = 4;
  for (uint8_t i = 0; i < 4; ++i) legacy.events[i].degree = i;
  HarmonicSourceView legacyView{HarmonicStrategy::Legacy, &legacy, nullptr};
  HarmonicEvent legacyFifth{};
  assert(harmonicEventAt(legacyView, 4, legacyFifth) == HarmonyStatus::Ready);
  assert(legacyFifth.degree == 0);  // Delegates legacy four-event cycle intact.
  HarmonicSourceView ambiguous{HarmonicStrategy::Legacy, &legacy, &graph};
  assert(harmonicEventAt(ambiguous, 0, legacyFifth) == HarmonyStatus::Invalid);

  std::cout << "HGS materializer projection and tagged source: PASS\n";
  return 0;
}
