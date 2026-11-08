#include "harmonic_source.h"

#include "harmonic_quality.h"
#include "harmonic_scale.h"
#include "../tonal/tonal_projector.h"

namespace GroovePuterHarmony {
namespace {

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

bool projectQuality(QualityId qualityId,
                    GroovePuterRhythm::ChordQuality& out) {
  using GroovePuterRhythm::ChordQuality;
  switch (qualityId) {
    case kQualityMajor: out = ChordQuality::Triad; return true;
    case kQualityDominant7: out = ChordQuality::Dominant7; return true;
    case kQualityDiminished: out = ChordQuality::Diminished; return true;
    case kQualitySus4: out = ChordQuality::Sus4; return true;
    case kQualityMinor7: out = ChordQuality::Minor7; return true;
    case kQualityMajor7: out = ChordQuality::Major7; return true;
    case kQualityMinor9: out = ChordQuality::Minor9; return true;
    case kQualityMajor9: out = ChordQuality::Major9; return true;
    default: return false;
  }
}

bool validState(const HarmonicState& state) {
  if (state.rootPc > 11 ||
      (state.bassPc != kNoBassPc && state.bassPc > 11)) return false;
  const ChordQualityDefinition* quality = qualityDefinition(state.qualityId);
  return quality != nullptr &&
         state.pitchMask == transposePitchSet(quality->intervalMask,
                                              state.rootPc);
}

}  // namespace

MaterializerCapabilityView materializerCapabilityView() {
  MaterializerCapabilityView view{};
  static constexpr QualityId kProjectableQualities[] = {
      kQualityMajor, kQualityDominant7, kQualityDiminished, kQualitySus4,
      kQualityMinor7, kQualityMajor7, kQualityMinor9, kQualityMajor9};
  for (QualityId id : kProjectableQualities) view.qualityMask |= 1u << id;
  view.scaleMask = (1u << GroovePuterRhythm::kScaleTypeCount) - 1u;
  for (uint8_t scaleId = 0; scaleId < GroovePuterRhythm::kScaleTypeCount;
       ++scaleId) {
    for (uint8_t root = 0; root < 12; ++root) {
      for (uint8_t degree = 0; degree <= 6; ++degree) {
        const int offset = static_cast<int>(root) -
            GroovePuterRhythm::scaleDegreeToSemitone(scaleId, degree);
        if (offset >= -GroovePuterRhythm::kMaxRootOffsetSemitones &&
            offset <= GroovePuterRhythm::kMaxRootOffsetSemitones) {
          view.rootMaskByHomeScale[scaleId] |= static_cast<uint16_t>(1u << root);
          break;
        }
      }
    }
  }
  view.supportsSlashBass = 0;
  return view;
}

HarmonyStatus projectHarmonicState(
    const HarmonicState& state, const TonalContext& context,
    GroovePuterRhythm::HarmonicEvent& out) {
  if (!validContext(context) || !validState(state)) return HarmonyStatus::Invalid;
  if (state.bassPc != kNoBassPc) return HarmonyStatus::Unsupported;
  GroovePuterRhythm::ChordQuality legacyQuality{};
  if (!projectQuality(state.qualityId, legacyQuality))
    return HarmonyStatus::Unsupported;
  if (context.scaleId >= GroovePuterRhythm::kScaleTypeCount ||
      context.homeScaleId >= GroovePuterRhythm::kScaleTypeCount) {
    return HarmonyStatus::Unsupported;
  }

  const uint8_t rootFromHome = static_cast<uint8_t>(
      (state.rootPc + 12u - context.homeTonicPc) % 12u);
  if ((materializerCapabilityView().rootMaskByHomeScale[context.homeScaleId] &
       (1u << rootFromHome)) == 0) return HarmonyStatus::Unsupported;
  int8_t bestOffset = 127;
  uint8_t bestDegree = 0xff;
  for (uint8_t degree = 0; degree <= 6; ++degree) {
    const int offset = static_cast<int>(rootFromHome) -
        GroovePuterRhythm::scaleDegreeToSemitone(context.homeScaleId, degree);
    if (offset < -GroovePuterRhythm::kMaxRootOffsetSemitones ||
        offset > GroovePuterRhythm::kMaxRootOffsetSemitones) continue;
    const int magnitude = offset < 0 ? -offset : offset;
    const int bestMagnitude = bestOffset < 0 ? -bestOffset : bestOffset;
    if (bestDegree == 0xff || magnitude < bestMagnitude ||
        (magnitude == bestMagnitude && degree < bestDegree)) {
      bestOffset = static_cast<int8_t>(offset);
      bestDegree = degree;
    }
  }
  if (bestDegree == 0xff) return HarmonyStatus::Unsupported;

  GroovePuterRhythm::HarmonicEvent result{};
  result.degree = bestDegree;
  result.quality = legacyQuality;
  result.rootOffsetSemitones = bestOffset;
  out = result;
  return HarmonyStatus::Ready;
}

HarmonyStatus harmonicEventAt(const HarmonicSourceView& source,
                              uint32_t globalHarmonicOrdinal,
                              GroovePuterRhythm::HarmonicEvent& out) {
  using GroovePuterRhythm::ChordProgressionStatus;
  if (source.strategy == HarmonicStrategy::Legacy) {
    if (source.legacySource == nullptr || source.graphSource != nullptr)
      return HarmonyStatus::Invalid;
    const GroovePuterRhythm::ChordProgressionEventResult event =
        GroovePuterRhythm::chordProgressionEventAt(*source.legacySource,
                                                   globalHarmonicOrdinal);
    if (event.status != ChordProgressionStatus::Ok &&
        event.status != ChordProgressionStatus::ValidButStatic) {
      return HarmonyStatus::Invalid;
    }
    out = event.event;
    return HarmonyStatus::Ready;
  }
  if (source.strategy != HarmonicStrategy::Graph ||
      source.graphSource == nullptr || source.legacySource != nullptr ||
      source.graphSource->materialId == 0 ||
      source.graphSource->publicationGeneration == 0 ||
      source.graphSource->modelVersion == 0 ||
      source.graphSource->authority != HarmonicAuthority::Graph ||
      source.graphSource->eventCount == 0 ||
      source.graphSource->eventCount > 32 ||
      globalHarmonicOrdinal >= source.graphSource->eventCount) {
    return HarmonyStatus::Invalid;
  }
  const ResolvedHarmonicEvent& event =
      source.graphSource->events[globalHarmonicOrdinal];
  GroovePuterRhythm::HarmonicEvent result{};
  const HarmonyStatus status =
      projectHarmonicState(event.state, event.tonal, result);
  if (status != HarmonyStatus::Ready) return status;
  out = result;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterHarmony
