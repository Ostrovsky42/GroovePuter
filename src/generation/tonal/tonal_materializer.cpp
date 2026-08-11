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

HarmonicEvent harmonicEventAt(const TonalMaterializationRequest& request,
                              uint8_t eventIndex) {
  if (request.progression.eventCount == 0) return HarmonicEvent{};
  return request.progression.events[eventIndex];
}

bool semitoneIntentAt(const TonalMaterializationRequest& request,
                      uint8_t ordinal) {
  return (request.semitoneOffsetOrdinals &
          static_cast<uint16_t>(1u << ordinal)) != 0;
}

uint8_t normalizePitchClass(int value) {
  value %= 12;
  if (value < 0) value += 12;
  return static_cast<uint8_t>(value);
}

int eventSemitoneFromGlobalRoot(const TonalMaterializationRequest& request,
                                const HarmonicEvent& event) {
  return scaleDegreeToSemitone(request.scaleTypeValue, event.degree) +
         static_cast<int>(event.rootOffsetSemitones);
}

int targetSemitoneFromGlobalRoot(const TonalMaterializationRequest& request,
                                 const HarmonicEvent& event,
                                 uint8_t globalOrdinal) {
  const int eventSemitone = eventSemitoneFromGlobalRoot(request, event);
  if (semitoneIntentAt(request, globalOrdinal)) {
    return eventSemitone +
           static_cast<int>(request.tonalOffsets[globalOrdinal]);
  }

  return scaleDegreeToSemitone(
             request.scaleTypeValue,
             static_cast<int>(event.degree) +
                 static_cast<int>(request.tonalOffsets[globalOrdinal])) +
         static_cast<int>(event.rootOffsetSemitones);
}

bool buildEventProjectionRequest(const TonalMaterializationRequest& request,
                                 const uint8_t* onsetSteps,
                                 uint8_t onsetCount,
                                 uint8_t eventIndex,
                                 uint8_t* globalOrdinals,
                                 TonalProjectionRequest& projection) {
  const HarmonicEvent event = harmonicEventAt(request, eventIndex);
  const int eventSemitone = eventSemitoneFromGlobalRoot(request, event);

  projection.rootPitchClass = normalizePitchClass(
      static_cast<int>(request.rootPitchClass) + eventSemitone);
  projection.scaleTypeValue = request.scaleTypeValue;
  projection.minMidi = request.minMidi;
  projection.maxMidi = request.maxMidi;
  projection.maxAdjacentLeapSemitones = request.maxAdjacentLeapSemitones;

  uint8_t localOrdinal = 0;
  for (uint8_t globalOrdinal = 0;
       globalOrdinal < onsetCount; ++globalOrdinal) {
    if (harmonicEventIndexForStep(request, onsetSteps[globalOrdinal]) !=
        eventIndex) {
      continue;
    }

    const int targetSemitone =
        targetSemitoneFromGlobalRoot(request, event, globalOrdinal);
    const int relativeSemitone = targetSemitone - eventSemitone;
    if (relativeSemitone < -128 || relativeSemitone > 127) return false;

    globalOrdinals[localOrdinal] = globalOrdinal;
    projection.tonalOffsets[localOrdinal] =
        static_cast<int8_t>(relativeSemitone);
    ++localOrdinal;
  }

  projection.onsetCount = localOrdinal;
  projection.semitoneOffsetOrdinals = ordinalMask(localOrdinal);
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

  // A harmonic event owns only its local root target. Each event is projected
  // independently into the role corridor, so a later chord does not inherit the
  // previous event's register anchor. All scale-degree arithmetic above remains
  // relative to the one Scene/global ScaleType; this is not voice leading.
  const uint8_t eventCount = request.progression.eventCount == 0
      ? 1
      : request.progression.eventCount;
  for (uint8_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
    uint8_t globalOrdinals[kStepsPerBar]{};
    TonalProjectionRequest projectionRequest{};
    if (!buildEventProjectionRequest(request, onsetSteps, onsetCount,
                                     eventIndex, globalOrdinals,
                                     projectionRequest)) {
      return result;
    }
    if (projectionRequest.onsetCount == 0) continue;

    const TonalProjectionResult projection =
        projectTonalIntent(projectionRequest);
    result.projectionStatus = projection.status;
    if (projection.status != TonalProjectionStatus::Ok ||
        projection.noteCount != projectionRequest.onsetCount) {
      result.status = TonalMaterializationStatus::ProjectionFailed;
      return result;
    }

    for (uint8_t localOrdinal = 0;
         localOrdinal < projection.noteCount; ++localOrdinal) {
      result.plan.midiNotes[globalOrdinals[localOrdinal]] =
          projection.midiNotes[localOrdinal];
    }
  }

  result.projectionStatus = TonalProjectionStatus::Ok;
  result.status = TonalMaterializationStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm
