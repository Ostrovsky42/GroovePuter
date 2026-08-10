#include "bass_pitch_behavior.h"

namespace GroovePuterRhythm {
namespace {

constexpr uint32_t kBassContourSalt = 0x15C10001u;
constexpr uint32_t kBassArticulationSalt = 0x15C20002u;

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

BassPitchContourId selectContour(const BassPitchBehaviorRequest& request,
                                 uint8_t onsetCount) {
  if (request.requestedContour != BassPitchContourId::Auto)
    return request.requestedContour;
  if (onsetCount <= 1) return BassPitchContourId::RootAnchor;

  BassPitchContourId candidates[6]{};
  uint8_t count = 0;
  switch (request.family) {
    case RhythmFamily::FourFloor:
      candidates[count++] = BassPitchContourId::RootAnchor;
      candidates[count++] = BassPitchContourId::RootFifth;
      candidates[count++] = BassPitchContourId::RootOctave;
      candidates[count++] = BassPitchContourId::PedalTurn;
      break;
    case RhythmFamily::MachineSyncopation:
    case RhythmFamily::Funk16:
      candidates[count++] = BassPitchContourId::RootFifth;
      candidates[count++] = BassPitchContourId::NeighborReturn;
      candidates[count++] = BassPitchContourId::StepApproach;
      candidates[count++] = BassPitchContourId::LeapReturn;
      candidates[count++] = BassPitchContourId::RootFifthNeighbor;
      break;
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
      candidates[count++] = BassPitchContourId::RootFifth;
      candidates[count++] = BassPitchContourId::LeapReturn;
      candidates[count++] = BassPitchContourId::NeighborReturn;
      candidates[count++] = BassPitchContourId::PedalTurn;
      break;
    case RhythmFamily::HipHopBackbeat:
      candidates[count++] = BassPitchContourId::RootAnchor;
      candidates[count++] = BassPitchContourId::NeighborReturn;
      candidates[count++] = BassPitchContourId::StepApproach;
      candidates[count++] = BassPitchContourId::RootFifth;
      break;
    case RhythmFamily::DubPulse:
    case RhythmFamily::SparsePulse:
      candidates[count++] = BassPitchContourId::RootAnchor;
      candidates[count++] = BassPitchContourId::NeighborReturn;
      candidates[count++] = BassPitchContourId::PedalTurn;
      break;
    case RhythmFamily::Count:
      return BassPitchContourId::RootAnchor;
  }

  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId, GenerationDomain::BassPitch,
      kBassContourSalt ^ static_cast<uint32_t>(request.family));
  return candidates[deterministicValue(seed, request.barOrdinal) % count];
}

BassArticulationStyleId selectArticulation(
    const BassPitchBehaviorRequest& request, uint8_t onsetCount) {
  if (request.requestedArticulation != BassArticulationStyleId::Auto)
    return request.requestedArticulation;
  if (onsetCount <= 1) return BassArticulationStyleId::Plain;

  BassArticulationStyleId candidates[4]{};
  uint8_t count = 0;
  switch (request.family) {
    case RhythmFamily::DubPulse:
    case RhythmFamily::SparsePulse:
      candidates[count++] = BassArticulationStyleId::Plain;
      candidates[count++] = BassArticulationStyleId::AccentPulse;
      break;
    case RhythmFamily::HipHopBackbeat:
      candidates[count++] = BassArticulationStyleId::Plain;
      candidates[count++] = BassArticulationStyleId::AccentPulse;
      candidates[count++] = BassArticulationStyleId::Dynamic;
      break;
    case RhythmFamily::FourFloor:
      candidates[count++] = BassArticulationStyleId::AccentPulse;
      candidates[count++] = BassArticulationStyleId::Dynamic;
      candidates[count++] = BassArticulationStyleId::LegatoApproach;
      break;
    case RhythmFamily::MachineSyncopation:
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
    case RhythmFamily::Funk16:
      candidates[count++] = BassArticulationStyleId::AccentPulse;
      candidates[count++] = BassArticulationStyleId::LegatoApproach;
      candidates[count++] = BassArticulationStyleId::Dynamic;
      break;
    case RhythmFamily::Count:
      return BassArticulationStyleId::Plain;
  }

  const uint32_t seed = deriveGenerationSeed(
      request.generation, request.archetypeId, GenerationDomain::BassPitch,
      kBassArticulationSalt ^ static_cast<uint32_t>(request.family));
  return candidates[deterministicValue(seed, request.barOrdinal) % count];
}

void buildContour(BassPitchContourId contour, uint8_t count,
                  int8_t* values) {
  for (uint8_t index = 0; index < count; ++index) values[index] = 0;
  if (count <= 1) return;

  switch (contour) {
    case BassPitchContourId::RootAnchor:
      break;
    case BassPitchContourId::RootFifth:
      for (uint8_t index = 1; index < count; ++index)
        values[index] = (index & 1u) != 0u ? 4 : 0;
      break;
    case BassPitchContourId::RootOctave:
      for (uint8_t index = 1; index < count; ++index)
        values[index] = (index & 1u) != 0u ? 7 : 0;
      break;
    case BassPitchContourId::NeighborReturn:
      for (uint8_t index = 1; index < count; ++index) {
        const uint8_t phase = static_cast<uint8_t>(index % 4u);
        values[index] = phase == 1u ? 1 : phase == 3u ? -1 : 0;
      }
      break;
    case BassPitchContourId::StepApproach:
      if (count == 2) {
        values[0] = -1;
        values[1] = 0;
      } else {
        const uint8_t start = static_cast<uint8_t>(count - 3u);
        values[start] = -2;
        values[start + 1u] = -1;
        values[start + 2u] = 0;
      }
      break;
    case BassPitchContourId::LeapReturn:
      values[1] = 4;
      if (count >= 3) values[2] = 0;
      for (uint8_t index = 3; index < count; ++index)
        values[index] = (index & 1u) != 0u ? 1 : 0;
      break;
    case BassPitchContourId::RootFifthNeighbor: {
      constexpr int8_t pattern[] = {0, 4, 1, 0};
      for (uint8_t index = 0; index < count; ++index)
        values[index] = pattern[index % 4u];
      break;
    }
    case BassPitchContourId::PedalTurn: {
      constexpr int8_t pattern[] = {0, 0, 1, 0};
      for (uint8_t index = 0; index < count; ++index)
        values[index] = pattern[index % 4u];
      break;
    }
    case BassPitchContourId::Auto:
    case BassPitchContourId::Count:
      break;
  }
}

void enforceBounds(const BassPitchBehaviorRequest& request,
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

bool isLegatoConnected(const BassPitchBehaviorPlan& pitchPlan,
                       uint8_t ordinal) {
  if (ordinal == 0 || ordinal >= pitchPlan.onsetCount) return false;
  const uint8_t previous = pitchPlan.onsetSteps[ordinal - 1u];
  const uint8_t destination = pitchPlan.onsetSteps[ordinal];
  if (destination <= previous) return false;
  if (destination == static_cast<uint8_t>(previous + 1u)) return true;
  for (uint8_t step = static_cast<uint8_t>(previous + 1u);
       step < destination; ++step) {
    if ((pitchPlan.continuations & stepBit(step)) == 0) return false;
  }
  return true;
}

void applyArticulation(BassArticulationStyleId articulation,
                       const BassPitchBehaviorPlan& pitchPlan,
                       StepMask& accentOnsets,
                       StepMask& slideIntoOnsets) {
  accentOnsets = 0;
  slideIntoOnsets = 0;
  if (pitchPlan.onsetCount == 0) return;

  const auto markAccent = [&](uint8_t ordinal) {
    accentOnsets = static_cast<StepMask>(
        accentOnsets | stepBit(pitchPlan.onsetSteps[ordinal]));
  };
  const auto markSlide = [&](uint8_t ordinal) {
    slideIntoOnsets = static_cast<StepMask>(
        slideIntoOnsets | stepBit(pitchPlan.onsetSteps[ordinal]));
  };

  switch (articulation) {
    case BassArticulationStyleId::Plain:
      break;
    case BassArticulationStyleId::AccentPulse:
      for (uint8_t ordinal = 0; ordinal < pitchPlan.onsetCount; ++ordinal) {
        const uint8_t step = pitchPlan.onsetSteps[ordinal];
        if (ordinal == 0 || (step % 4u) == 0u) markAccent(ordinal);
      }
      break;
    case BassArticulationStyleId::LegatoApproach:
      markAccent(0);
      for (uint8_t ordinal = 1; ordinal < pitchPlan.onsetCount; ++ordinal) {
        const uint8_t pitchGap = absoluteDifference(
            pitchPlan.degreeOffsets[ordinal],
            pitchPlan.degreeOffsets[ordinal - 1u]);
        if (isLegatoConnected(pitchPlan, ordinal) &&
            pitchGap > 0u && pitchGap <= 2u) {
          markSlide(ordinal);
        }
      }
      break;
    case BassArticulationStyleId::Dynamic:
      for (uint8_t ordinal = 0; ordinal < pitchPlan.onsetCount; ++ordinal) {
        const uint8_t step = pitchPlan.onsetSteps[ordinal];
        if (pitchPlan.degreeOffsets[ordinal] == 0 &&
            (ordinal == 0 || (step % 4u) == 0u)) {
          markAccent(ordinal);
        }
        if (ordinal == 0) continue;
        const uint8_t pitchGap = absoluteDifference(
            pitchPlan.degreeOffsets[ordinal],
            pitchPlan.degreeOffsets[ordinal - 1u]);
        if (isLegatoConnected(pitchPlan, ordinal) && pitchGap == 1u)
          markSlide(ordinal);
      }
      break;
    case BassArticulationStyleId::Auto:
    case BassArticulationStyleId::Count:
      break;
  }
}

bool validRequest(const BassPitchBehaviorRequest& request) {
  return request.archetypeId != kNoArchetypeId &&
         static_cast<uint8_t>(request.family) <
             static_cast<uint8_t>(RhythmFamily::Count) &&
         isValidBassPitchContourId(request.requestedContour) &&
         isValidBassArticulationStyleId(request.requestedArticulation) &&
         request.minDegreeOffset <= 0 && request.maxDegreeOffset >= 0 &&
         request.minDegreeOffset <= request.maxDegreeOffset &&
         validContinuationTopology(request.rhythmPlan.onsets,
                                   request.rhythmPlan.continuations);
}

}  // namespace

bool isValidBassPitchContourId(BassPitchContourId id, bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(BassPitchContourId::Count)) return false;
  return allowAuto || id != BassPitchContourId::Auto;
}

bool isValidBassArticulationStyleId(BassArticulationStyleId id,
                                    bool allowAuto) {
  const uint8_t value = static_cast<uint8_t>(id);
  if (value >= static_cast<uint8_t>(BassArticulationStyleId::Count))
    return false;
  return allowAuto || id != BassArticulationStyleId::Auto;
}

BassPitchBehaviorResult realizeBassPitchBehavior(
    const BassPitchBehaviorRequest& request) {
  BassPitchBehaviorResult result{};
  if (!validRequest(request)) return result;

  result.plan.onsets = request.rhythmPlan.onsets;
  result.plan.continuations = request.rhythmPlan.continuations;
  result.plan.onsetCount = collectOnsetSteps(
      request.rhythmPlan.onsets, result.plan.onsetSteps);
  if (result.plan.onsetCount == 0) {
    result.plan.contour = BassPitchContourId::RootAnchor;
    result.plan.articulation = BassArticulationStyleId::Plain;
    result.status = BassPitchBehaviorStatus::ValidButEmpty;
    return result;
  }

  const BassPitchContourId contour =
      selectContour(request, result.plan.onsetCount);
  const BassArticulationStyleId articulation =
      selectArticulation(request, result.plan.onsetCount);
  if (!isValidBassPitchContourId(contour, false) ||
      !isValidBassArticulationStyleId(articulation, false)) {
    return result;
  }

  result.plan.contour = contour;
  result.plan.articulation = articulation;
  buildContour(contour, result.plan.onsetCount, result.plan.degreeOffsets);
  enforceBounds(request, result.plan.onsetCount, result.plan.degreeOffsets);
  applyArticulation(articulation, result.plan,
                    result.plan.accentOnsets, result.plan.slideIntoOnsets);
  result.plan.accentOnsets = static_cast<StepMask>(
      result.plan.accentOnsets & result.plan.onsets);
  result.plan.slideIntoOnsets = static_cast<StepMask>(
      result.plan.slideIntoOnsets & result.plan.onsets);
  result.status = BassPitchBehaviorStatus::Ok;
  return result;
}

const char* bassPitchContourName(BassPitchContourId id) {
  switch (id) {
    case BassPitchContourId::Auto: return "AUTO";
    case BassPitchContourId::RootAnchor: return "ROOT ANCHOR";
    case BassPitchContourId::RootFifth: return "ROOT/FIFTH";
    case BassPitchContourId::RootOctave: return "ROOT/OCTAVE";
    case BassPitchContourId::NeighborReturn: return "NEIGHBOR RETURN";
    case BassPitchContourId::StepApproach: return "STEP APPROACH";
    case BassPitchContourId::LeapReturn: return "LEAP RETURN";
    case BassPitchContourId::RootFifthNeighbor: return "ROOT/FIFTH/NEIGHBOR";
    case BassPitchContourId::PedalTurn: return "PEDAL TURN";
    case BassPitchContourId::Count: break;
  }
  return "INVALID";
}

const char* bassArticulationStyleName(BassArticulationStyleId id) {
  switch (id) {
    case BassArticulationStyleId::Auto: return "AUTO";
    case BassArticulationStyleId::Plain: return "PLAIN";
    case BassArticulationStyleId::AccentPulse: return "ACCENT PULSE";
    case BassArticulationStyleId::LegatoApproach: return "LEGATO APPROACH";
    case BassArticulationStyleId::Dynamic: return "DYNAMIC";
    case BassArticulationStyleId::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
