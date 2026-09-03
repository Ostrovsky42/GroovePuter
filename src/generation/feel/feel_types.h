#ifndef GROOVEPUTER_GENERATION_FEEL_FEEL_TYPES_H
#define GROOVEPUTER_GENERATION_FEEL_FEEL_TYPES_H

#include <cstdint>
#include <type_traits>

#include "../rhythm/rhythm_types.h"

namespace GroovePuterRhythm {

// Stable Scene IDs. Append only: persisted Scenes store the numeric value.
// Straight..PushPullControlled are concrete timing characters. Auto is a
// selection mode, not a fifth timing curve: it delegates the choice to the
// generation profile and is resolved to a concrete profile before any timing
// is interpreted.
enum class FeelProfileId : uint8_t {
  Straight = 0,
  SwingCompatible,
  LaidBack,
  PushPullControlled,
  Auto,
  Count,
};

constexpr uint16_t kFeelTicksPerQuarter = 96;
constexpr uint16_t kFeelTicksPerStep = 24;
constexpr uint16_t kFeelTicksPerBar = 384;
constexpr uint8_t kMaxFeelBars = 8;
constexpr uint16_t kMaxFeelEvents = 128;

enum class FeelInterpretStatus : uint8_t {
  Ok = 0,
  InvalidProfile,
  InvalidPhrase,
  Overflow,
  Count,
};

struct FeelPhraseEvent {
  RhythmRole role = RhythmRole::Kick;
  uint8_t barIndex = 0;
  uint16_t idealTick = 0;     // Relative to bar origin.
  uint16_t durationTicks = 1;
};

struct FeelPhrase {
  uint8_t barCount = 0;
  uint16_t eventCount = 0;
  FeelPhraseEvent events[kMaxFeelEvents]{};
};

struct TimedFeelEvent {
  uint16_t idealOnTick = 0;
  uint16_t targetOnTick = 0;
  uint32_t targetOffTick = 1;
  int8_t offsetTicks = 0;
};

struct TimedFeelPhrase {
  uint8_t barCount = 0;
  uint16_t eventCount = 0;
  TimedFeelEvent events[kMaxFeelEvents]{};
};

const char* feelProfileName(FeelProfileId profile);

// A concrete timing character the FEEL interpreter can execute. Auto is not
// one: an unresolved Auto reaching interpretation is a resolution bug, never a
// silent Straight.
bool isValidFeelProfile(FeelProfileId profile);

// A value the musician may select on the FEEL page and a Scene may persist:
// any concrete profile, or Auto.
bool isSelectableFeelProfile(FeelProfileId profile);

// The single FEEL arbitration rule. Auto delegates to the generation profile's
// prior; any concrete selection wins over it.
inline FeelProfileId resolveFeelProfile(FeelProfileId requested,
                                        FeelProfileId suggested) {
  return requested == FeelProfileId::Auto ? suggested : requested;
}

static_assert(std::is_trivially_copyable<FeelPhrase>::value,
              "FeelPhrase must remain fixed-capacity");
static_assert(std::is_trivially_copyable<TimedFeelPhrase>::value,
              "TimedFeelPhrase must remain fixed-capacity");
static_assert(sizeof(FeelPhrase) <= 1040,
              "FeelPhrase exceeded the Stage 8 bounded RAM target");
static_assert(sizeof(TimedFeelPhrase) <= 1560,
              "TimedFeelPhrase exceeded the Stage 8 bounded RAM target");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_FEEL_FEEL_TYPES_H
