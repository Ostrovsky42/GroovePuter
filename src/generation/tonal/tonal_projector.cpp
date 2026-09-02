#include "tonal_projector.h"

namespace GroovePuterRhythm {
namespace {

uint16_t validOrdinalMask(uint8_t count) {
  if (count == 0) return 0;
  if (count >= kStepsPerBar) return kAllSteps;
  return static_cast<uint16_t>((1u << count) - 1u);
}

bool validRequest(const TonalProjectionRequest& request) {
  if (request.onsetCount > kStepsPerBar ||
      request.rootPitchClass > 11 ||
      !isValidScaleTypeValue(request.scaleTypeValue) ||
      request.minMidi > request.maxMidi ||
      request.maxMidi > 127 ||
      request.maxAdjacentLeapSemitones > 127) {
    return false;
  }
  const uint16_t validMask = validOrdinalMask(request.onsetCount);
  return (request.semitoneOffsetOrdinals &
          static_cast<uint16_t>(~validMask)) == 0;
}

bool isSemitoneOrdinal(const TonalProjectionRequest& request,
                       uint8_t ordinal) {
  return (request.semitoneOffsetOrdinals &
          static_cast<uint16_t>(1u << ordinal)) != 0;
}

void resolveDisplacements(const TonalProjectionRequest& request,
                          int16_t* displacements) {
  for (uint8_t ordinal = 0; ordinal < request.onsetCount; ++ordinal) {
    const int displacement = isSemitoneOrdinal(request, ordinal)
        ? static_cast<int>(request.tonalOffsets[ordinal])
        : scaleDegreeToSemitone(request.scaleTypeValue,
                                request.tonalOffsets[ordinal]);
    displacements[ordinal] = static_cast<int16_t>(displacement);
  }
}

bool anchorFitsAllNotes(const TonalProjectionRequest& request,
                        int rootAnchor,
                        const int16_t* displacements) {
  for (uint8_t ordinal = 0; ordinal < request.onsetCount; ++ordinal) {
    const int candidate = rootAnchor + displacements[ordinal];
    if (candidate < request.minMidi || candidate > request.maxMidi ||
        candidate < 0 || candidate > 127) {
      return false;
    }
  }
  return true;
}

bool findFeasibleRootAnchor(const TonalProjectionRequest& request,
                            const int16_t* displacements,
                            bool& rootPitchClassPresent,
                            uint8_t& rootAnchor) {
  const int midpoint2 = static_cast<int>(request.minMidi) +
                        static_cast<int>(request.maxMidi);
  rootPitchClassPresent = false;
  bool foundFeasible = false;
  int bestDistance2 = 1000;
  uint8_t best = 0;

  for (int midi = request.minMidi; midi <= request.maxMidi; ++midi) {
    if ((midi % 12) != request.rootPitchClass) continue;
    rootPitchClassPresent = true;
    if (!anchorFitsAllNotes(request, midi, displacements)) continue;

    int distance2 = midi * 2 - midpoint2;
    if (distance2 < 0) distance2 = -distance2;
    if (!foundFeasible || distance2 < bestDistance2) {
      foundFeasible = true;
      bestDistance2 = distance2;
      best = static_cast<uint8_t>(midi);
    }
  }

  if (!foundFeasible) return false;
  rootAnchor = best;
  return true;
}

uint8_t absoluteDifference(uint8_t a, uint8_t b) {
  return a >= b ? static_cast<uint8_t>(a - b)
                : static_cast<uint8_t>(b - a);
}

}  // namespace

bool isValidScaleTypeValue(ScaleTypeValue scaleTypeValue) {
  return isCatalogScaleTypeValue(scaleTypeValue);
}

uint8_t scaleCardinality(ScaleTypeValue scaleTypeValue) {
  return scaleDefinitionFor(scaleTypeValue).count;
}

TonalProjectionResult projectTonalIntent(const TonalProjectionRequest& request) {
  TonalProjectionResult result{};
  if (!validRequest(request)) return result;
  if (request.onsetCount == 0) {
    result.status = TonalProjectionStatus::ValidButEmpty;
    return result;
  }

  int16_t displacements[kStepsPerBar]{};
  resolveDisplacements(request, displacements);

  bool rootPitchClassPresent = false;
  uint8_t rootAnchor = 0;
  if (!findFeasibleRootAnchor(request, displacements,
                              rootPitchClassPresent, rootAnchor)) {
    result.status = rootPitchClassPresent
        ? TonalProjectionStatus::NoteOutOfRegister
        : TonalProjectionStatus::RootOutOfRegister;
    return result;
  }

  uint8_t midiNotes[kStepsPerBar]{};
  for (uint8_t ordinal = 0; ordinal < request.onsetCount; ++ordinal) {
    const int candidate = static_cast<int>(rootAnchor) + displacements[ordinal];
    midiNotes[ordinal] = static_cast<uint8_t>(candidate);
    if (ordinal > 0 &&
        absoluteDifference(midiNotes[ordinal], midiNotes[ordinal - 1u]) >
            request.maxAdjacentLeapSemitones) {
      result.status = TonalProjectionStatus::LeapExceeded;
      return result;
    }
  }

  result.rootAnchorMidi = rootAnchor;
  result.noteCount = request.onsetCount;
  for (uint8_t ordinal = 0; ordinal < request.onsetCount; ++ordinal)
    result.midiNotes[ordinal] = midiNotes[ordinal];
  result.status = TonalProjectionStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
