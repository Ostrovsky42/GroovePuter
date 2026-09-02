#include "feel_interpreter.h"

#include <cstdint>
#include <limits>

namespace GroovePuterRhythm {
namespace {

int roleCoefficient(FeelProfileId profile,
                    RhythmRole role,
                    uint16_t idealTick) {
  const uint8_t step = static_cast<uint8_t>(idealTick / kFeelTicksPerStep);
  switch (profile) {
    case FeelProfileId::Straight:
      return 0;
    case FeelProfileId::SwingCompatible:
      switch (role) {
        // Constant inter-role nudges remain compatible with whatever offbeat
        // law swingPct/swingMask applies later; this profile does not encode a
        // second odd-step swing curve.
        case RhythmRole::ClosedHat: return 20;
        case RhythmRole::OpenHat: return 25;
        case RhythmRole::Percussion: return -20;
        default:
          return 0;
      }
    case FeelProfileId::LaidBack:
      switch (role) {
        case RhythmRole::Kick: return 0;
        case RhythmRole::Backbeat: return 100;
        case RhythmRole::ClosedHat: return 45;
        case RhythmRole::OpenHat: return 55;
        case RhythmRole::Percussion: return 65;
        case RhythmRole::BassRhythm: return 35;
        case RhythmRole::ChordRhythm: return 70;
        case RhythmRole::MelodicRhythm: return 50;
        default: return 0;
      }
    case FeelProfileId::PushPullControlled:
      switch (role) {
        case RhythmRole::Kick: return step == 0 ? 0 : -55;
        case RhythmRole::Backbeat: return 55;
        case RhythmRole::ClosedHat:
        case RhythmRole::OpenHat: return (step & 1u) == 0u ? -35 : 35;
        case RhythmRole::Percussion: return (step & 1u) == 0u ? 30 : -30;
        case RhythmRole::BassRhythm: return -30;
        case RhythmRole::ChordRhythm: return 35;
        case RhythmRole::MelodicRhythm: return (step & 1u) == 0u ? -25 : 25;
        default: return 0;
      }
    default:
      return 0;
  }
}

int boundedOffset(const FeelInterpretRequest& request,
                  const FeelPhraseEvent& event,
                  uint16_t eventIndex) {
  if (request.profile == FeelProfileId::Straight || request.amount == 0) {
    return 0;
  }

  // A Feel profile may consume at most one quarter of the active grid cell.
  const int gridBound = static_cast<int>(request.gridIntervalTicks / 4u);
  const int profileBound = gridBound < 6 ? gridBound : 6;
  const int scaledBound = (profileBound * request.amount + 50) / 100;
  if (scaledBound == 0) return 0;

  const int coefficient = roleCoefficient(
      request.profile, event.role, event.idealTick);
  int offset = (scaledBound * coefficient) / 100;

  // One deterministic tick of expression avoids a mechanical repeated shape
  // while retaining the profile's direction and fixed bound.
  if (offset != 0 && scaledBound >= 3) {
    const uint32_t seed = deriveGenerationSeed(
        request.generation, kNoArchetypeId,
        GenerationDomain::FeelExpression,
        static_cast<uint32_t>(request.profile));
    const uint32_t coordinate =
        (static_cast<uint32_t>(event.barIndex) << 24u) |
        (static_cast<uint32_t>(event.idealTick) << 8u) |
        static_cast<uint32_t>(eventIndex & 0xFFu);
    const int expression =
        static_cast<int>(deterministicValue(seed, coordinate) % 3u) - 1;
    offset += expression;
  }
  if (offset < -scaledBound) offset = -scaledBound;
  if (offset > scaledBound) offset = scaledBound;
  return offset;
}

bool validPhrase(const FeelPhrase& phrase) {
  if (phrase.barCount == 0 || phrase.barCount > kMaxFeelBars ||
      phrase.eventCount > kMaxFeelEvents) {
    return false;
  }
  uint32_t previousIdeal = 0;
  for (uint16_t i = 0; i < phrase.eventCount; ++i) {
    const FeelPhraseEvent& event = phrase.events[i];
    if (event.barIndex >= phrase.barCount ||
        event.idealTick >= kFeelTicksPerBar ||
        event.durationTicks == 0 ||
        static_cast<uint8_t>(event.role) >= kRhythmRoleCount) {
      return false;
    }
    const uint32_t ideal =
        static_cast<uint32_t>(event.barIndex) * kFeelTicksPerBar +
        event.idealTick;
    if (i != 0 && ideal < previousIdeal) return false;
    previousIdeal = ideal;
  }
  return true;
}

}  // namespace

const char* feelProfileName(FeelProfileId profile) {
  switch (profile) {
    case FeelProfileId::Straight: return "STRAIGHT";
    case FeelProfileId::SwingCompatible: return "SWING COMPAT";
    case FeelProfileId::LaidBack: return "LAID BACK";
    case FeelProfileId::PushPullControlled: return "PUSH/PULL";
    default: return "STRAIGHT";
  }
}

bool isValidFeelProfile(FeelProfileId profile) {
  return static_cast<uint8_t>(profile) <
         static_cast<uint8_t>(FeelProfileId::Count);
}

FeelInterpretStatus interpretFeelPhrase(const FeelInterpretRequest& request,
                                        TimedFeelPhrase& destination) {
  if (!isValidFeelProfile(request.profile)) {
    return FeelInterpretStatus::InvalidProfile;
  }
  if (request.phrase == nullptr || request.amount > 100 ||
      request.gridIntervalTicks == 0 ||
      request.gridIntervalTicks > kFeelTicksPerBar) {
    return FeelInterpretStatus::InvalidPhrase;
  }
  if (request.phrase->eventCount > kMaxFeelEvents) {
    return FeelInterpretStatus::Overflow;
  }
  if (!validPhrase(*request.phrase)) {
    return FeelInterpretStatus::InvalidPhrase;
  }

  TimedFeelPhrase next{};
  next.barCount = request.phrase->barCount;
  next.eventCount = request.phrase->eventCount;
  uint32_t previousIdeal = 0;
  uint32_t previousGroupMax = 0;
  uint32_t currentGroupMax = 0;
  for (uint16_t i = 0; i < request.phrase->eventCount; ++i) {
    const FeelPhraseEvent& source = request.phrase->events[i];
    TimedFeelEvent& target = next.events[i];
    const uint32_t barOrigin =
        static_cast<uint32_t>(source.barIndex) * kFeelTicksPerBar;
    const uint32_t barLastTick = barOrigin + kFeelTicksPerBar - 1u;
    target.idealOnTick = static_cast<uint16_t>(barOrigin + source.idealTick);

    if (i != 0 && target.idealOnTick != previousIdeal) {
      previousGroupMax = currentGroupMax;
      currentGroupMax = 0;
    }

    int64_t interpreted = static_cast<int64_t>(target.idealOnTick) +
                          boundedOffset(request, source, i);
    if (interpreted < static_cast<int64_t>(barOrigin)) interpreted = barOrigin;
    if (interpreted > static_cast<int64_t>(barLastTick)) {
      interpreted = barLastTick;
    }
    uint32_t absoluteTarget = static_cast<uint32_t>(interpreted);
    // Events sharing one ideal coordinate are a simultaneous group. Across
    // distinct coordinates, however, Feel must not reverse musical order.
    if (i != 0 && target.idealOnTick != previousIdeal &&
        absoluteTarget < previousGroupMax) {
      absoluteTarget = previousGroupMax;
    }
    target.targetOnTick = static_cast<uint16_t>(absoluteTarget);
    target.offsetTicks = static_cast<int8_t>(
        static_cast<int32_t>(absoluteTarget) -
        static_cast<int32_t>(target.idealOnTick));
    target.targetOffTick = target.targetOnTick + source.durationTicks;
    if (target.targetOffTick <= target.targetOnTick) {
      target.targetOffTick = std::numeric_limits<uint32_t>::max();
    }
    if (absoluteTarget > currentGroupMax) currentGroupMax = absoluteTarget;
    previousIdeal = target.idealOnTick;
  }

  destination = next;
  return FeelInterpretStatus::Ok;
}

}  // namespace GroovePuterRhythm
