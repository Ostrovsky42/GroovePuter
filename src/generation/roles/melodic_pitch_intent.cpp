#include "melodic_pitch_intent.h"

namespace GroovePuterRhythm {
namespace {

constexpr uint32_t kRhythmSalt = 0x15B00001u;
constexpr uint32_t kContourSalt = 0x15B10001u;
constexpr uint32_t kOperationSalt = 0x15B20002u;

int8_t clampDegree(int value, int8_t minimum, int8_t maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return static_cast<int8_t>(value);
}

uint8_t absoluteDifference(int8_t a, int8_t b) {
  const int difference = static_cast<int>(a) - static_cast<int>(b);
  return static_cast<uint8_t>(difference < 0 ? -difference : difference);
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

uint8_t collectOnsetSteps(StepMask onsets, uint8_t* destination) {
  uint8_t count = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((onsets & stepBit(step)) == 0) continue;
    destination[count++] = step;
  }
  return count;
}

uint8_t noteEndStep(uint8_t onsetStep, StepMask continuations) {
  uint8_t end = onsetStep;
  for (uint8_t step = static_cast<uint8_t>(onsetStep + 1u);
       step < kStepsPerBar; ++step) {
    if ((continuations & stepBit(step)) == 0) break;
    end = step;
  }
  return end;
}

StepMask spanMask(uint8_t first, uint8_t last) {
  StepMask mask = 0;
  for (uint8_t step = first; step <= last && step < kStepsPerBar; ++step)
    mask = static_cast<StepMask>(mask | stepBit(step));
  return mask;
}

void removeNote(uint8_t onsetStep, StepMask& onsets, StepMask& continuations) {
  const uint8_t end = noteEndStep(onsetStep, continuations);
  onsets = static_cast<StepMask>(onsets & ~stepBit(onsetStep));
  for (uint8_t step = static_cast<uint8_t>(onsetStep + 1u);
       step <= end && step < kStepsPerBar; ++step) {
    continuations = static_cast<StepMask>(continuations & ~stepBit(step));
  }
}

bool moveNote(uint8_t onsetStep, int8_t delta,
              StepMask allowedOnsetSteps,
              StepMask allowedContinuationSteps,
              StepMask& onsets, StepMask& continuations) {
  const uint8_t end = noteEndStep(onsetStep, continuations);
  const int targetStart = static_cast<int>(onsetStep) + delta;
  const int targetEnd = static_cast<int>(end) + delta;
  if (targetStart < 0 || targetEnd >= kStepsPerBar) return false;

  const StepMask oldSpan = spanMask(onsetStep, end);
  const StepMask newSpan = spanMask(static_cast<uint8_t>(targetStart),
                                    static_cast<uint8_t>(targetEnd));
  const StepMask newOnset = stepBit(static_cast<uint8_t>(targetStart));
  const StepMask newContinuations = static_cast<StepMask>(newSpan & ~newOnset);
  const StepMask occupied = static_cast<StepMask>(onsets | continuations);
  const StepMask otherOccupied = static_cast<StepMask>(occupied & ~oldSpan);
  if ((newOnset & static_cast<StepMask>(~allowedOnsetSteps)) != 0)
    return false;
  if ((newContinuations &
       static_cast<StepMask>(~allowedContinuationSteps)) != 0) {
    return false;
  }
  if ((newSpan & otherOccupied) != 0) return false;

  removeNote(onsetStep, onsets, continuations);
  onsets = static_cast<StepMask>(onsets | newOnset);
  continuations = static_cast<StepMask>(continuations | newContinuations);
  return true;
}

MelodicRhythmOperationId selectRhythmOperation(
    const MelodicPitchIntentRequest& request, uint8_t onsetCount) {
  if (request.requestedRhythmOperation != MelodicRhythmOperationId::Auto)
    return request.requestedRhythmOperation;
  if (onsetCount == 0) return MelodicRhythmOperationId::Preserve;

  MelodicRhythmOperationId candidates[5]{};
  uint8_t count = 0;
  switch (request.family) {
    case RhythmFamily::DubPulse:
    case RhythmFamily::SparsePulse:
      candidates[count++] = MelodicRhythmOperationId::Preserve;
      candidates[count++] = MelodicRhythmOperationId::ControlledRest;
      candidates[count++] = MelodicRhythmOperationId::ShiftInteriorLater;
      break;
    case RhythmFamily::HipHopBackbeat:
      candidates[count++] = MelodicRhythmOperationId::Preserve;
      candidates[count++] = MelodicRhythmOperationId::ControlledRest;
      candidates[count++] = MelodicRhythmOperationId::ShiftInteriorEarlier;
      candidates[count++] = MelodicRhythmOperationId::ShiftInteriorLater;
      candidates[count++] = MelodicRhythmOperationId::TerminalEcho;
      break;
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
    case RhythmFamily::Funk16:
    case RhythmFamily::MachineSyncopation:
      candidates[count++] = MelodicRhythmOperationId::Preserve;
      candidates[count++] = MelodicRhythmOperationId::ShiftInteriorEarlier;
      candidates[count++] = MelodicRhythmOperationId::ShiftInteriorLater;
      candidates[count++] = MelodicRhythmOperationId::TerminalEcho;
      break;
    case RhythmFamily::FourFloor:
      candidates[count++] = MelodicRhythmOperationId::Preserve;
      candidates[count++] = MelodicRhythmOperationId::ControlledRest;
      candidates[count++] = MelodicRhythmOperationId::ShiftInteriorLater;
      candidates[count++] = MelodicRhythmOperationId::TerminalEcho;
      break;
    case RhythmFamily::Count:
      return MelodicRhythmOperationId::Preserve;
  }

  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::MelodicRhythmSelection,
      kRhythmSalt ^ static_cast<uint32_t>(request.family));
  return candidates[deterministicValue(seed, request.barOrdinal) % count];
}

bool applyControlledRest(const MelodicPitchIntentRequest& request,
                         StepMask& onsets, StepMask& continuations) {
  uint8_t onsetSteps[kStepsPerBar]{};
  const uint8_t count = collectOnsetSteps(onsets, onsetSteps);
  if (count == 0) return false;
  if (count == 1 && !request.allowEmptyBar) return false;

  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::MelodicRhythmSelection, kRhythmSalt ^ 0x31u);
  const uint8_t firstCandidate = count > 1 ? 1u : 0u;
  const uint8_t candidateCount = static_cast<uint8_t>(count - firstCandidate);
  const uint8_t ordinal = static_cast<uint8_t>(
      firstCandidate +
      deterministicValue(seed, request.barOrdinal) % candidateCount);
  removeNote(onsetSteps[ordinal], onsets, continuations);
  return true;
}

bool applyInteriorShift(const MelodicPitchIntentRequest& request,
                        int8_t delta,
                        StepMask& onsets, StepMask& continuations) {
  uint8_t onsetSteps[kStepsPerBar]{};
  const uint8_t count = collectOnsetSteps(onsets, onsetSteps);
  if (count <= 1) return false;

  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::MelodicRhythmSelection,
      kRhythmSalt ^ (delta < 0 ? 0x41u : 0x42u));
  const uint8_t mutableCount = static_cast<uint8_t>(count - 1u);
  const uint8_t start = static_cast<uint8_t>(
      deterministicValue(seed, request.barOrdinal) % mutableCount);
  for (uint8_t attempt = 0; attempt < mutableCount; ++attempt) {
    const uint8_t ordinal = static_cast<uint8_t>(
        1u + ((start + attempt) % mutableCount));
    StepMask candidateOnsets = onsets;
    StepMask candidateContinuations = continuations;
    if (!moveNote(onsetSteps[ordinal], delta,
                  request.allowedOnsetSteps,
                  request.allowedContinuationSteps,
                  candidateOnsets, candidateContinuations)) {
      continue;
    }
    onsets = candidateOnsets;
    continuations = candidateContinuations;
    return true;
  }
  return false;
}

bool applyTerminalEcho(const MelodicPitchIntentRequest& request,
                       StepMask& onsets,
                       StepMask continuations) {
  uint8_t onsetSteps[kStepsPerBar]{};
  const uint8_t count = collectOnsetSteps(onsets, onsetSteps);
  if (count == 0 || count >= request.maxOnsets) return false;

  constexpr int8_t offsets[] = {2, -2, 3, -3, 1, -1};
  constexpr uint8_t kOffsetCount =
      static_cast<uint8_t>(sizeof(offsets) / sizeof(offsets[0]));
  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId,
      GenerationDomain::MelodicRhythmSelection, kRhythmSalt ^ 0x51u);
  const uint8_t rotation = static_cast<uint8_t>(
      deterministicValue(seed, request.barOrdinal) % kOffsetCount);
  const uint8_t terminal = onsetSteps[count - 1u];
  const StepMask occupied = static_cast<StepMask>(onsets | continuations);
  for (uint8_t attempt = 0; attempt < kOffsetCount; ++attempt) {
    const int8_t offset = offsets[(rotation + attempt) % kOffsetCount];
    const int target = static_cast<int>(terminal) + offset;
    if (target < 0 || target >= kStepsPerBar) continue;
    const StepMask bit = stepBit(static_cast<uint8_t>(target));
    if ((request.allowedOnsetSteps & bit) == 0 || (occupied & bit) != 0)
      continue;
    onsets = static_cast<StepMask>(onsets | bit);
    return true;
  }
  return false;
}

MelodicRhythmOperationId applyRhythmOperation(
    const MelodicPitchIntentRequest& request,
    MelodicRhythmOperationId requested,
    StepMask& onsets,
    StepMask& continuations) {
  bool applied = false;
  switch (requested) {
    case MelodicRhythmOperationId::Preserve:
      return MelodicRhythmOperationId::Preserve;
    case MelodicRhythmOperationId::ControlledRest:
      applied = applyControlledRest(request, onsets, continuations);
      break;
    case MelodicRhythmOperationId::ShiftInteriorEarlier:
      applied = applyInteriorShift(request, -1, onsets, continuations);
      break;
    case MelodicRhythmOperationId::ShiftInteriorLater:
      applied = applyInteriorShift(request, 1, onsets, continuations);
      break;
    case MelodicRhythmOperationId::TerminalEcho:
      applied = applyTerminalEcho(request, onsets, continuations);
      break;
    case MelodicRhythmOperationId::Auto:
    case MelodicRhythmOperationId::Count:
      break;
  }
  return applied ? requested : MelodicRhythmOperationId::Preserve;
}

MelodicContourId selectContour(const MelodicPitchIntentRequest& request,
                               uint8_t onsetCount) {
  if (request.requestedContour != MelodicContourId::Auto)
    return request.requestedContour;
  if (onsetCount <= 1) return MelodicContourId::Static;

  MelodicContourId candidates[8]{};
  uint8_t count = 0;
  switch (request.family) {
    case RhythmFamily::FourFloor:
      candidates[count++] = MelodicContourId::RepeatThenUp;
      candidates[count++] = MelodicContourId::RepeatThenDown;
      candidates[count++] = MelodicContourId::Arch;
      candidates[count++] = MelodicContourId::Neighbor;
      break;
    case RhythmFamily::MachineSyncopation:
    case RhythmFamily::Funk16:
      candidates[count++] = MelodicContourId::StepUp;
      candidates[count++] = MelodicContourId::StepDown;
      candidates[count++] = MelodicContourId::LeapReturn;
      candidates[count++] = MelodicContourId::Neighbor;
      break;
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
      candidates[count++] = MelodicContourId::LeapReturn;
      candidates[count++] = MelodicContourId::Arch;
      candidates[count++] = MelodicContourId::InvertedArch;
      candidates[count++] = MelodicContourId::RepeatThenDown;
      break;
    case RhythmFamily::HipHopBackbeat:
      candidates[count++] = MelodicContourId::Neighbor;
      candidates[count++] = MelodicContourId::RepeatThenUp;
      candidates[count++] = MelodicContourId::RepeatThenDown;
      candidates[count++] = MelodicContourId::LeapReturn;
      break;
    case RhythmFamily::DubPulse:
    case RhythmFamily::SparsePulse:
      candidates[count++] = MelodicContourId::Static;
      candidates[count++] = MelodicContourId::Neighbor;
      candidates[count++] = MelodicContourId::StepDown;
      candidates[count++] = MelodicContourId::Arch;
      break;
    case RhythmFamily::Count:
      return MelodicContourId::Static;
  }

  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId, GenerationDomain::LeadPitch,
      kContourSalt ^ static_cast<uint32_t>(request.family));
  return candidates[deterministicValue(seed, request.barOrdinal) % count];
}

MelodicMotifOperationId selectOperation(
    const MelodicPitchIntentRequest& request, uint8_t onsetCount) {
  if (request.requestedOperation != MelodicMotifOperationId::Auto)
    return request.requestedOperation;
  if (onsetCount <= 1) return MelodicMotifOperationId::None;

  MelodicMotifOperationId candidates[5]{};
  uint8_t count = 0;
  switch (request.family) {
    case RhythmFamily::DubPulse:
    case RhythmFamily::SparsePulse:
      candidates[count++] = MelodicMotifOperationId::None;
      candidates[count++] = MelodicMotifOperationId::TerminalReturn;
      candidates[count++] = MelodicMotifOperationId::PivotRepeat;
      break;
    case RhythmFamily::HipHopBackbeat:
      candidates[count++] = MelodicMotifOperationId::None;
      candidates[count++] = MelodicMotifOperationId::ChangeTerminal;
      candidates[count++] = MelodicMotifOperationId::PivotRepeat;
      candidates[count++] = MelodicMotifOperationId::TerminalReturn;
      break;
    case RhythmFamily::FourFloor:
    case RhythmFamily::MachineSyncopation:
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
    case RhythmFamily::Funk16:
      candidates[count++] = MelodicMotifOperationId::None;
      candidates[count++] = MelodicMotifOperationId::ChangeTerminal;
      candidates[count++] = MelodicMotifOperationId::InvertLocal;
      candidates[count++] = MelodicMotifOperationId::PivotRepeat;
      candidates[count++] = MelodicMotifOperationId::TerminalReturn;
      break;
    case RhythmFamily::Count:
      return MelodicMotifOperationId::None;
  }

  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId, GenerationDomain::MotifSelection,
      kOperationSalt ^ static_cast<uint32_t>(request.family));
  return candidates[deterministicValue(seed, request.barOrdinal) % count];
}

void buildContour(MelodicContourId contour, uint8_t count,
                  uint8_t maxLeapDegrees, int8_t* values) {
  for (uint8_t index = 0; index < count; ++index) values[index] = 0;
  if (count <= 1) return;

  const int8_t step = maxLeapDegrees == 0 ? 0 : 1;
  const int8_t leap =
      static_cast<int8_t>(maxLeapDegrees < 4 ? maxLeapDegrees : 4);
  switch (contour) {
    case MelodicContourId::Static:
      break;
    case MelodicContourId::StepUp:
      for (uint8_t index = 1; index < count; ++index)
        values[index] = static_cast<int8_t>(index * step);
      break;
    case MelodicContourId::StepDown:
      for (uint8_t index = 1; index < count; ++index)
        values[index] = static_cast<int8_t>(-static_cast<int>(index * step));
      break;
    case MelodicContourId::Arch:
    case MelodicContourId::InvertedArch: {
      const int direction = contour == MelodicContourId::Arch ? 1 : -1;
      if (count == 2) {
        values[1] = static_cast<int8_t>(direction * step);
        break;
      }
      const uint8_t midpoint = static_cast<uint8_t>((count - 1u) / 2u);
      for (uint8_t index = 1; index < count; ++index) {
        const uint8_t distance = index <= midpoint
            ? index
            : static_cast<uint8_t>(count - 1u - index);
        values[index] = static_cast<int8_t>(direction * distance * step);
      }
      break;
    }
    case MelodicContourId::LeapReturn:
      values[1] = leap;
      if (count >= 3) values[2] = 0;
      for (uint8_t index = 3; index < count; ++index)
        values[index] = static_cast<int8_t>((index & 1u) != 0u ? step : 0);
      break;
    case MelodicContourId::Neighbor:
      for (uint8_t index = 1; index < count; ++index) {
        const uint8_t phase = static_cast<uint8_t>(index % 4u);
        values[index] = phase == 1u ? step : phase == 3u ? -step : 0;
      }
      break;
    case MelodicContourId::RepeatThenUp:
      for (uint8_t index = 2; index < count; ++index)
        values[index] = static_cast<int8_t>((index - 1u) * step);
      break;
    case MelodicContourId::RepeatThenDown:
      for (uint8_t index = 2; index < count; ++index)
        values[index] = static_cast<int8_t>(
            -static_cast<int>((index - 1u) * step));
      break;
    case MelodicContourId::Auto:
    case MelodicContourId::Count:
      break;
  }
}

void applyOperation(const MelodicPitchIntentRequest& request,
                    MelodicMotifOperationId operation,
                    uint8_t count,
                    int8_t* values) {
  if (count == 0) return;
  switch (operation) {
    case MelodicMotifOperationId::None:
      break;
    case MelodicMotifOperationId::ChangeTerminal: {
      const uint32_t seed = deriveGenerationSeed(
          request.generation, request.archetypeId,
          GenerationDomain::MotifSelection, kOperationSalt ^ 0xA5u);
      const int8_t delta =
          (deterministicValue(seed, request.barOrdinal) & 1u) != 0u ? 1 : -1;
      values[count - 1u] = static_cast<int8_t>(values[count - 1u] + delta);
      break;
    }
    case MelodicMotifOperationId::InvertLocal:
      for (uint8_t index = 1; index < count; ++index)
        values[index] = static_cast<int8_t>(-values[index]);
      break;
    case MelodicMotifOperationId::PivotRepeat:
      if (count >= 3) values[count - 2u] = values[count - 3u];
      break;
    case MelodicMotifOperationId::TerminalReturn:
      values[count - 1u] = 0;
      break;
    case MelodicMotifOperationId::Auto:
    case MelodicMotifOperationId::Count:
      break;
  }
}

void enforceBounds(const MelodicPitchIntentRequest& request,
                   uint8_t count,
                   int8_t* values) {
  if (count == 0) return;
  values[0] = clampDegree(values[0], request.minDegreeOffset,
                          request.maxDegreeOffset);
  for (uint8_t index = 1; index < count; ++index) {
    int8_t value = clampDegree(values[index], request.minDegreeOffset,
                               request.maxDegreeOffset);
    if (absoluteDifference(value, values[index - 1u]) > request.maxLeapDegrees) {
      const int direction = value >= values[index - 1u] ? 1 : -1;
      value = static_cast<int8_t>(
          values[index - 1u] + direction * request.maxLeapDegrees);
      value = clampDegree(value, request.minDegreeOffset,
                          request.maxDegreeOffset);
    }
    values[index] = value;
  }
}

bool validRequest(const MelodicPitchIntentRequest& request) {
  if (request.archetypeId == kNoArchetypeId ||
      static_cast<uint8_t>(request.family) >=
          static_cast<uint8_t>(RhythmFamily::Count) ||
      !isValidMelodicRhythmOperationId(request.requestedRhythmOperation) ||
      !isValidMelodicContourId(request.requestedContour) ||
      !isValidMelodicMotifOperationId(request.requestedOperation) ||
      request.minDegreeOffset > 0 || request.maxDegreeOffset < 0 ||
      request.minDegreeOffset > request.maxDegreeOffset ||
      request.maxOnsets > kStepsPerBar ||
      (request.rhythmPlan.onsets &
       static_cast<StepMask>(~request.allowedOnsetSteps)) != 0 ||
      (request.rhythmPlan.continuations &
       static_cast<StepMask>(~request.allowedContinuationSteps)) != 0 ||
      !validContinuationTopology(request.rhythmPlan.onsets,
                                 request.rhythmPlan.continuations)) {
    return false;
  }
  uint8_t onsetSteps[kStepsPerBar]{};
  return collectOnsetSteps(request.rhythmPlan.onsets, onsetSteps) <=
         request.maxOnsets;
}

}  // namespace

bool isValidMelodicRhythmOperationId(MelodicRhythmOperationId id,
                                     bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(MelodicRhythmOperationId::Count))
    return false;
  return allowAuto || id != MelodicRhythmOperationId::Auto;
}

bool isValidMelodicContourId(MelodicContourId id, bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(MelodicContourId::Count)) return false;
  return allowAuto || id != MelodicContourId::Auto;
}

bool isValidMelodicMotifOperationId(MelodicMotifOperationId id,
                                    bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(MelodicMotifOperationId::Count))
    return false;
  return allowAuto || id != MelodicMotifOperationId::Auto;
}

MelodicPitchIntentResult realizeMelodicPitchIntent(
    const MelodicPitchIntentRequest& request) {
  MelodicPitchIntentResult result{};
  if (!validRequest(request)) return result;

  StepMask onsets = request.rhythmPlan.onsets;
  StepMask continuations = request.rhythmPlan.continuations;
  uint8_t onsetSteps[kStepsPerBar]{};
  const uint8_t inputOnsetCount = collectOnsetSteps(onsets, onsetSteps);
  const MelodicRhythmOperationId requestedRhythmOperation =
      selectRhythmOperation(request, inputOnsetCount);
  if (!isValidMelodicRhythmOperationId(requestedRhythmOperation, false))
    return result;

  result.plan.rhythmOperation = applyRhythmOperation(
      request, requestedRhythmOperation, onsets, continuations);
  if (!validContinuationTopology(onsets, continuations)) return result;
  if ((onsets & static_cast<StepMask>(~request.allowedOnsetSteps)) != 0)
    return result;
  if ((continuations &
       static_cast<StepMask>(~request.allowedContinuationSteps)) != 0) {
    return result;
  }

  result.plan.onsets = onsets;
  result.plan.continuations = continuations;
  result.plan.onsetCount = collectOnsetSteps(onsets, result.plan.onsetSteps);
  if (result.plan.onsetCount > request.maxOnsets) return result;
  if (result.plan.onsetCount == 0) {
    if (!request.allowEmptyBar && inputOnsetCount != 0) return result;
    result.plan.contour = MelodicContourId::Static;
    result.plan.operation = MelodicMotifOperationId::None;
    result.status = MelodicPitchIntentStatus::ValidButEmpty;
    return result;
  }

  const MelodicContourId contour =
      selectContour(request, result.plan.onsetCount);
  const MelodicMotifOperationId operation =
      selectOperation(request, result.plan.onsetCount);
  if (!isValidMelodicContourId(contour, false) ||
      !isValidMelodicMotifOperationId(operation, false)) {
    return result;
  }

  result.plan.contour = contour;
  result.plan.operation = operation;
  buildContour(contour, result.plan.onsetCount, request.maxLeapDegrees,
               result.plan.degreeOffsets);
  applyOperation(request, operation, result.plan.onsetCount,
                 result.plan.degreeOffsets);
  enforceBounds(request, result.plan.onsetCount, result.plan.degreeOffsets);
  result.status = MelodicPitchIntentStatus::Ok;
  return result;
}

const char* melodicRhythmOperationName(MelodicRhythmOperationId id) {
  switch (id) {
    case MelodicRhythmOperationId::Auto: return "AUTO";
    case MelodicRhythmOperationId::Preserve: return "PRESERVE";
    case MelodicRhythmOperationId::ControlledRest: return "CONTROLLED REST";
    case MelodicRhythmOperationId::ShiftInteriorEarlier: return "SHIFT EARLIER";
    case MelodicRhythmOperationId::ShiftInteriorLater: return "SHIFT LATER";
    case MelodicRhythmOperationId::TerminalEcho: return "TERMINAL ECHO";
    case MelodicRhythmOperationId::Count: break;
  }
  return "INVALID";
}

const char* melodicContourName(MelodicContourId id) {
  switch (id) {
    case MelodicContourId::Auto: return "AUTO";
    case MelodicContourId::Static: return "STATIC";
    case MelodicContourId::StepUp: return "STEP UP";
    case MelodicContourId::StepDown: return "STEP DOWN";
    case MelodicContourId::Arch: return "ARCH";
    case MelodicContourId::InvertedArch: return "INVERTED ARCH";
    case MelodicContourId::LeapReturn: return "LEAP RETURN";
    case MelodicContourId::Neighbor: return "NEIGHBOR";
    case MelodicContourId::RepeatThenUp: return "REPEAT THEN UP";
    case MelodicContourId::RepeatThenDown: return "REPEAT THEN DOWN";
    case MelodicContourId::Count: break;
  }
  return "INVALID";
}

const char* melodicMotifOperationName(MelodicMotifOperationId id) {
  switch (id) {
    case MelodicMotifOperationId::Auto: return "AUTO";
    case MelodicMotifOperationId::None: return "NONE";
    case MelodicMotifOperationId::ChangeTerminal: return "CHANGE TERMINAL";
    case MelodicMotifOperationId::InvertLocal: return "INVERT LOCAL";
    case MelodicMotifOperationId::PivotRepeat: return "PIVOT REPEAT";
    case MelodicMotifOperationId::TerminalReturn: return "TERMINAL RETURN";
    case MelodicMotifOperationId::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
