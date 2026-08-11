#include "tonal_materializer.h"

namespace GroovePuterRhythm {
namespace {

uint8_t popcount(StepMask mask) {
  uint8_t count = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((mask & stepBit(step)) != 0) ++count;
  }
  return count;
}

uint8_t collectOnsetSteps(StepMask mask, uint8_t* steps) {
  uint8_t count = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((mask & stepBit(step)) == 0) continue;
    steps[count++] = step;
  }
  return count;
}

bool validContinuationTopology(StepMask onsets, StepMask continuations) {
  if ((onsets & continuations) != 0) return false;
  bool active = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((onsets & bit) != 0) {
      active = true;
      continue;
    }
    if ((continuations & bit) != 0) {
      if (!active) return false;
      continue;
    }
    active = false;
  }
  return true;
}

uint16_t ordinalMask(uint8_t count) {
  if (count == 0) return 0;
  if (count >= kStepsPerBar) return kAllSteps;
  return static_cast<uint16_t>((1u << count) - 1u);
}

bool validEvent(const HarmonicEvent& event) {
  return event.degree <= 6 &&
         static_cast<uint8_t>(event.quality) <
             static_cast<uint8_t>(ChordQuality::Count) &&
         event.rootOffsetSemitones >= -kMaxRootOffsetSemitones &&
         event.rootOffsetSemitones <= kMaxRootOffsetSemitones;
}

bool validRequest(const TonalMaterializationRequest& request,
                  uint8_t onsetCount) {
  if (!validContinuationTopology(request.onsets, request.continuations) ||
      request.rootPitchClass > 11 ||
      !isValidScaleTypeValue(request.scaleTypeValue) ||
      request.minMidi > request.maxMidi || request.maxMidi > 127 ||
      request.maxAdjacentLeapSemitones > 127 ||
      request.progression.eventCount > kMaxHarmonicEvents ||
      !isValidProgressionId(request.progression.id, false)) {
    return false;
  }

  if ((request.semitoneOffsetOrdinals &
       static_cast<uint16_t>(~ordinalMask(onsetCount))) != 0) {
    return false;
  }

  for (uint8_t index = 0; index < request.progression.eventCount; ++index) {
    if (!validEvent(request.progression.events[index])) return false;
  }

  // Non-static progression plans are generated one event per ChordRhythm onset.
  // Static/pedal plans intentionally keep one event for any number of chord
  // onsets, and an empty harmonic plan means an implicit scale root.
  if (request.progression.eventCount > 1 &&
      popcount(request.harmonicEventOnsets) != request.progression.eventCount) {
    return false;
  }
  return true;
}

uint8_t harmonicEventIndexForStep(const TonalMaterializationRequest& request,
                                  uint8_t step) {
  if (request.progression.eventCount <= 1) return 0;

  uint8_t eventOrdinal = 0;
  uint8_t selected = 0;
  for (uint8_t candidateStep = 0;
       candidateStep < kStepsPerBar; ++candidateStep) {
    if ((request.harmonicEventOnsets & stepBit(candidateStep)) == 0) continue;
    if (candidateStep > step) break;
    selected = eventOrdinal;
    if (eventOrdinal + 1u < request.progression.eventCount) ++eventOrdinal;
  }
  return selected;
}

HarmonicEvent harmonicEventForStep(const TonalMaterializationRequest& request,
                                   uint8_t step) {
  if (request.progression.eventCount == 0) return HarmonicEvent{};
  return request.progression.events[harmonicEventIndexForStep(request, step)];
}

bool semitoneIntentAt(const TonalMaterializationRequest& request,
                      uint8_t ordinal) {
  return (request.semitoneOffsetOrdinals &
          static_cast<uint16_t>(1u << ordinal)) != 0;
}

bool buildProjectionRequest(const TonalMaterializationRequest& request,
                            const uint8_t* onsetSteps,
                            uint8_t onsetCount,
                            TonalProjectionRequest& projection) {
  projection.onsetCount = onsetCount;
  projection.rootPitchClass = request.rootPitchClass;
  projection.scaleTypeValue = request.scaleTypeValue;
  projection.minMidi = request.minMidi;
  projection.maxMidi = request.maxMidi;
  projection.maxAdjacentLeapSemitones = request.maxAdjacentLeapSemitones;
  projection.semitoneOffsetOrdinals = ordinalMask(onsetCount);

  for (uint8_t ordinal = 0; ordinal < onsetCount; ++ordinal) {
    const HarmonicEvent event = harmonicEventForStep(request, onsetSteps[ordinal]);
    const int displacement = semitoneIntentAt(request, ordinal)
        ? scaleDegreeToSemitone(request.scaleTypeValue, event.degree) +
              static_cast<int>(request.tonalOffsets[ordinal]) +
              static_cast<int>(event.rootOffsetSemitones)
        : scaleDegreeToSemitone(
              request.scaleTypeValue,
              static_cast<int>(event.degree) +
                  static_cast<int>(request.tonalOffsets[ordinal])) +
              static_cast<int>(event.rootOffsetSemitones);
    if (displacement < -128 || displacement > 127) return false;
    projection.tonalOffsets[ordinal] = static_cast<int8_t>(displacement);
  }
  return true;
}

}  // namespace

TonalMaterializationResult materializeTonalIntent(
    const TonalMaterializationRequest& request) {
  TonalMaterializationResult result{};
  uint8_t onsetSteps[kStepsPerBar]{};
  const uint8_t onsetCount = collectOnsetSteps(request.onsets, onsetSteps);
  if (!validRequest(request, onsetCount)) return result;

  result.plan.onsets = request.onsets;
  result.plan.continuations = request.continuations;
  result.plan.onsetCount = onsetCount;
  for (uint8_t ordinal = 0; ordinal < onsetCount; ++ordinal)
    result.plan.onsetSteps[ordinal] = onsetSteps[ordinal];

  if (onsetCount == 0) {
    result.status = TonalMaterializationStatus::ValidButEmpty;
    result.projectionStatus = TonalProjectionStatus::ValidButEmpty;
    return result;
  }

  TonalProjectionRequest projectionRequest{};
  if (!buildProjectionRequest(request, onsetSteps, onsetCount,
                              projectionRequest)) {
    return result;
  }

  const TonalProjectionResult projection =
      projectTonalIntent(projectionRequest);
  result.projectionStatus = projection.status;
  if (projection.status != TonalProjectionStatus::Ok ||
      projection.noteCount != onsetCount) {
    result.status = TonalMaterializationStatus::ProjectionFailed;
    return result;
  }

  for (uint8_t ordinal = 0; ordinal < onsetCount; ++ordinal)
    result.plan.midiNotes[ordinal] = projection.midiNotes[ordinal];
  result.status = TonalMaterializationStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
