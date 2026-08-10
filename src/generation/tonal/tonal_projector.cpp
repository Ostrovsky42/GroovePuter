#include "tonal_projector.h"

namespace GroovePuterRhythm {
namespace {

// Numeric ABI of the existing global ScaleType enum in scenes.h. A source
// regression parses that enum and pins this exact order so drift cannot be
// silent without introducing a second C++ scale enum here.
constexpr ScaleTypeValue kScaleMinor = 0;
constexpr ScaleTypeValue kScaleMajor = 1;
constexpr ScaleTypeValue kScaleDorian = 2;
constexpr ScaleTypeValue kScalePhrygian = 3;
constexpr ScaleTypeValue kScaleLydian = 4;
constexpr ScaleTypeValue kScaleMixolydian = 5;
constexpr ScaleTypeValue kScaleLocrian = 6;
constexpr ScaleTypeValue kScalePentatonicMajor = 7;
constexpr ScaleTypeValue kScalePentatonicMinor = 8;
constexpr ScaleTypeValue kScaleChromatic = 9;

struct ScaleDefinition {
  const int8_t* intervals = nullptr;
  uint8_t count = 0;
};

constexpr int8_t kMinor[] = {0, 2, 3, 5, 7, 8, 10};
constexpr int8_t kMajor[] = {0, 2, 4, 5, 7, 9, 11};
constexpr int8_t kDorian[] = {0, 2, 3, 5, 7, 9, 10};
constexpr int8_t kPhrygian[] = {0, 1, 3, 5, 7, 8, 10};
constexpr int8_t kLydian[] = {0, 2, 4, 6, 7, 9, 11};
constexpr int8_t kMixolydian[] = {0, 2, 4, 5, 7, 9, 10};
constexpr int8_t kLocrian[] = {0, 1, 3, 5, 6, 8, 10};
constexpr int8_t kMajorPentatonic[] = {0, 2, 4, 7, 9};
constexpr int8_t kMinorPentatonic[] = {0, 3, 5, 7, 10};
constexpr int8_t kChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

ScaleDefinition scaleDefinition(ScaleTypeValue scaleTypeValue) {
  switch (scaleTypeValue) {
    case kScaleMinor: return {kMinor, 7};
    case kScaleMajor: return {kMajor, 7};
    case kScaleDorian: return {kDorian, 7};
    case kScalePhrygian: return {kPhrygian, 7};
    case kScaleLydian: return {kLydian, 7};
    case kScaleMixolydian: return {kMixolydian, 7};
    case kScaleLocrian: return {kLocrian, 7};
    case kScalePentatonicMajor: return {kMajorPentatonic, 5};
    case kScalePentatonicMinor: return {kMinorPentatonic, 5};
    case kScaleChromatic: return {kChromatic, 12};
    default: return {};
  }
}

int floorDiv(int value, int divisor) {
  int quotient = value / divisor;
  const int remainder = value % divisor;
  if (remainder < 0) --quotient;
  return quotient;
}

int degreeToSemitone(int degree, const ScaleDefinition& scale) {
  const int octave = floorDiv(degree, scale.count);
  const int index = degree - octave * scale.count;
  return octave * 12 + scale.intervals[index];
}

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

bool findRootAnchor(const TonalProjectionRequest& request,
                    uint8_t& rootAnchor) {
  const int midpoint2 = static_cast<int>(request.minMidi) +
                        static_cast<int>(request.maxMidi);
  bool found = false;
  int bestDistance2 = 1000;
  uint8_t best = 0;

  for (int midi = request.minMidi; midi <= request.maxMidi; ++midi) {
    if ((midi % 12) != request.rootPitchClass) continue;
    int distance2 = midi * 2 - midpoint2;
    if (distance2 < 0) distance2 = -distance2;
    if (!found || distance2 < bestDistance2) {
      found = true;
      bestDistance2 = distance2;
      best = static_cast<uint8_t>(midi);
    }
  }

  if (!found) return false;
  rootAnchor = best;
  return true;
}

uint8_t absoluteDifference(uint8_t a, uint8_t b) {
  return a >= b ? static_cast<uint8_t>(a - b)
                : static_cast<uint8_t>(b - a);
}

}  // namespace

bool isValidScaleTypeValue(ScaleTypeValue scaleTypeValue) {
  return scaleDefinition(scaleTypeValue).intervals != nullptr;
}

uint8_t scaleCardinality(ScaleTypeValue scaleTypeValue) {
  return scaleDefinition(scaleTypeValue).count;
}

TonalProjectionResult projectTonalIntent(const TonalProjectionRequest& request) {
  TonalProjectionResult result{};
  if (!validRequest(request)) return result;
  if (request.onsetCount == 0) {
    result.status = TonalProjectionStatus::ValidButEmpty;
    return result;
  }

  const ScaleDefinition scale = scaleDefinition(request.scaleTypeValue);
  if (!findRootAnchor(request, result.rootAnchorMidi)) {
    result.status = TonalProjectionStatus::RootOutOfRegister;
    return result;
  }

  result.noteCount = request.onsetCount;
  for (uint8_t ordinal = 0; ordinal < request.onsetCount; ++ordinal) {
    const int displacement = isSemitoneOrdinal(request, ordinal)
        ? static_cast<int>(request.tonalOffsets[ordinal])
        : degreeToSemitone(request.tonalOffsets[ordinal], scale);
    const int candidate = static_cast<int>(result.rootAnchorMidi) + displacement;
    if (candidate < request.minMidi || candidate > request.maxMidi ||
        candidate < 0 || candidate > 127) {
      result.status = TonalProjectionStatus::NoteOutOfRegister;
      return result;
    }

    result.midiNotes[ordinal] = static_cast<uint8_t>(candidate);
    if (ordinal > 0 &&
        absoluteDifference(result.midiNotes[ordinal],
                           result.midiNotes[ordinal - 1u]) >
            request.maxAdjacentLeapSemitones) {
      result.status = TonalProjectionStatus::LeapExceeded;
      return result;
    }
  }

  result.status = TonalProjectionStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
