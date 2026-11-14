#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_MACRO_ENGINE_H
#define GROOVEPUTER_PERFORMANCE_MACRO_ENGINE_H

#include <cstdint>
#include <type_traits>

#include "src/phrase/runtime_phrase_edit.h"
#include "src/ui/project_key.h"

namespace GroovePuterPerformance {
namespace Macros {

constexpr int8_t kValueMax = 100;
constexpr int8_t kValueMin = -100;   // negative: the opposite of each macro
constexpr uint16_t kBeatTicks = PhraseRuntime::kTicksPerBar / 4u;

enum class Control : uint8_t {
  Tension = 0,
  Energy,
  Space,
  Mutation,
};

struct State {
  int8_t tension{0};
  int8_t energy{0};
  int8_t space{0};
  int8_t mutation{0};

  bool set(Control control, int value) {
    if (value > kValueMax || value < kValueMin) return false;
    switch (control) {
      case Control::Tension: tension = value; return true;
      case Control::Energy: energy = value; return true;
      case Control::Space: space = value; return true;
      case Control::Mutation: mutation = value; return true;
    }
    return false;
  }

  bool valid() const {
    return inRange(tension) && inRange(energy) && inRange(space) && inRange(mutation);
  }

 private:
  static bool inRange(int8_t value) { return value >= kValueMin && value <= kValueMax; }

 public:
};

constexpr uint32_t packState(const State& state) {
  return static_cast<uint32_t>(static_cast<uint8_t>(state.tension)) |
         (static_cast<uint32_t>(static_cast<uint8_t>(state.energy)) << 8u) |
         (static_cast<uint32_t>(static_cast<uint8_t>(state.space)) << 16u) |
         (static_cast<uint32_t>(static_cast<uint8_t>(state.mutation)) << 24u);
}

constexpr State unpackState(uint32_t packed) {
  State state{};
  state.tension = static_cast<int8_t>(static_cast<uint8_t>(packed & 0xFFu));
  state.energy = static_cast<int8_t>(static_cast<uint8_t>((packed >> 8u) & 0xFFu));
  state.space = static_cast<int8_t>(static_cast<uint8_t>((packed >> 16u) & 0xFFu));
  state.mutation = static_cast<int8_t>(static_cast<uint8_t>((packed >> 24u) & 0xFFu));
  return state;
}

// Mutation only follows the project key when the caller can establish it.
// Unknown or ambiguous harmony leaves pitches unchanged.
struct HarmonyContext {
  bool known{false};
  uint8_t root{0};
  ProjectKey::ScaleTypeValue scale{GroovePuterRhythm::kDefaultScaleTypeValue};
};

enum class ApplyResult : uint8_t {
  Ready = 0,
  InvalidState,
  InvalidHarmony,
  InvalidSource,
  InvalidCandidate,
  AliasedBuffers,
};

namespace detail {

inline bool validHarmony(const HarmonyContext& harmony) {
  return !harmony.known ||
         (harmony.root < 12u && harmony.scale < GroovePuterRhythm::kScaleTypeCount);
}

inline uint32_t mixSeed(uint32_t value) {
  value ^= value >> 16u;
  value *= 0x7FEB352Du;
  value ^= value >> 15u;
  value *= 0x846CA68Bu;
  value ^= value >> 16u;
  return value;
}

inline bool isWeakOnset(uint16_t tick) {
  return (tick % kBeatTicks) != 0;
}

inline bool validEvent(const PhraseRuntime::RuntimeSynthEvent& event,
                       uint16_t lengthTicks) {
  if (!RuntimePhraseEdit::validLengthTicks(lengthTicks) ||
      !RuntimePhraseEdit::validEventValues(event) ||
      event.durationSubticks == 0 || event.startTick >= lengthTicks) {
    return false;
  }
  // Runtime Pattern lifetimes can intentionally cross a bar line (for example
  // a tied step 15). The playback event path accepts that; only the control
  // side batch transform additionally applies RuntimePhraseEdit::validate().
  return true;
}

inline uint8_t mutatedNote(
    const PhraseRuntime::RuntimeSynthEvent& event, uint16_t eventIndex,
    const State& state, const HarmonyContext& harmony, uint32_t seed) {
  if (state.mutation == 0 || !harmony.known ||
      (event.flags & PhraseRuntime::kEventGhost) == 0 ||
      !isWeakOnset(event.startTick) ||
      !ProjectKey::inScale(event.note, harmony.root, harmony.scale)) {
    return event.note;
  }

  const uint32_t identity =
      seed ^ (static_cast<uint32_t>(event.startTick) * 0x9E3779B9u) ^
      (static_cast<uint32_t>(eventIndex) * 0x85EBCA6Bu);
  const uint32_t choice = mixSeed(identity);
  const uint8_t strength = static_cast<uint8_t>(state.mutation < 0 ? -state.mutation : state.mutation);
  if ((choice % 100u) >= strength) return event.note;

  // Negative MUTATION steps the other way: the same notes, the opposite neighbour.
  int direction = (choice & 0x100u) != 0 ? 1 : -1;
  if (state.mutation < 0) direction = -direction;
  const int neighbor = ProjectKey::step(
      event.note, direction, harmony.root, harmony.scale);
  return neighbor >= 0 && neighbor <= 127
      ? static_cast<uint8_t>(neighbor)
      : event.note;
}

inline void applyMutation(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                          const State& state,
                          const HarmonyContext& harmony,
                          uint32_t seed) {
  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    event.note = mutatedNote(event, i, state, harmony, seed);
  }
}

// ENERGY on velocity: 80% of the room left below 127 at 100%. Never lowers a note
// and never exceeds 127, so a loud note still rises (the old x1.5 gain clipped at 85).
inline uint8_t energyVelocity(uint8_t velocity, int energy) {
  if (energy < 0) {
    // Negative ENERGY: up to half the velocity away, never below 1.
    const uint32_t out = (static_cast<uint32_t>(velocity) * (200u + static_cast<uint32_t>(energy)) + 100u) / 200u;
    return static_cast<uint8_t>(out < 1u ? 1u : out);
  }
  if (energy == 0 || velocity >= 127u) return velocity;
  const uint32_t room = 127u - velocity;
  const uint32_t gain = (room * static_cast<uint32_t>(energy) * 8u + 500u) / 1000u;
  const uint32_t out = velocity + gain;
  return static_cast<uint8_t>(out > 127u ? 127u : out);
}

// Velocity and gate articulation only: no pitch, onset, FX or event insertion.
inline void tensionEvent(PhraseRuntime::RuntimeSynthEvent& event,
                         const PhraseRuntime::RuntimeSynthEvent* next,
                         int tension) {
  if (tension == 0 || !isWeakOnset(event.startTick)) return;
  if (tension < 0) {
    // Negative TENSION takes the weak-onset accent back, up to 12 velocity points.
    const uint16_t drop = static_cast<uint16_t>((-tension * 12 + 50) / 100);
    const int lowered = static_cast<int>(event.velocity) - static_cast<int>(drop);
    event.velocity = static_cast<uint8_t>(lowered < 1 ? 1 : lowered);
    return;
  }
  const int interval = next == nullptr ? 0 :
      static_cast<int>(next->note) - event.note;
  const bool approach = interval != 0 && interval >= -3 && interval <= 3;
  const uint16_t accent = (tension * (approach ? 60u : 30u) + 50u) / 100u;
  const uint16_t velocity = event.velocity + accent;
  event.velocity = static_cast<uint8_t>(velocity > 127u ? 127u : velocity);
}

inline void applyTension(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                         int tension) {
  if (tension == 0) return;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    const PhraseRuntime::RuntimeSynthEvent* next = nullptr;
    for (uint16_t j = static_cast<uint16_t>(i + 1u); j < phrase.count; ++j) {
      if (phrase.events[j].startTick > phrase.events[i].startTick &&
          (next == nullptr || phrase.events[j].startTick < next->startTick)) {
        next = &phrase.events[j];
      }
    }
    tensionEvent(phrase.events[i], next, tension);
  }
}

// Chance of a ghost note under ENERGY: positive lifts toward 100, negative scales down.
inline uint8_t energyProbability(uint8_t probability, int energy) {
  if (energy < 0) {
    const uint32_t scaled = (static_cast<uint32_t>(probability) * (100u + static_cast<uint32_t>(energy)) + 50u) / 100u;
    return static_cast<uint8_t>(scaled);
  }
  const uint16_t missing = static_cast<uint16_t>(100u - probability);
  const uint16_t boost = static_cast<uint16_t>((missing * static_cast<uint16_t>(energy) + 50u) / 100u);
  const uint16_t lifted = static_cast<uint16_t>(probability) + boost;
  return static_cast<uint8_t>(lifted > 100u ? 100u : lifted);
}

inline void applyEnergy(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                        int energy) {
  if (energy == 0) return;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    event.velocity = detail::energyVelocity(event.velocity, energy);
    if ((event.flags & PhraseRuntime::kEventGhost) == 0) continue;
    event.probability = detail::energyProbability(event.probability, energy);
  }
}

inline void spaceEvent(PhraseRuntime::RuntimeSynthEvent& event, int space,
                       uint32_t maxDuration) {
  if (space == 0) return;
  if (space < 0) {
    // Negative SPACE: legato. Gates grow up to 100% longer, never past the bar.
    const uint32_t grown = (static_cast<uint32_t>(event.durationSubticks) * (100u + static_cast<uint32_t>(-space))) / 100u;
    const uint32_t capped = grown > maxDuration ? maxDuration : grown;
    if (capped > event.durationSubticks) {
      event.durationSubticks = static_cast<uint16_t>(capped > 65535u ? 65535u : capped);
    }
    return;
  }
  const uint16_t durationPercent = 100u - (space * 90u) / 100u;
  const uint32_t duration =
      (static_cast<uint32_t>(event.durationSubticks) * durationPercent) / 100u;
  event.durationSubticks = static_cast<uint16_t>(duration == 0 ? 1 : duration);
  // Preserve beat anchors; optional ornaments can disappear entirely.
  const bool ghost = (event.flags & PhraseRuntime::kEventGhost) != 0;
  // A velocity threshold here is not monotonic: ENERGY can lift a note across it and
  // the chance would rise again. Ghosts and weak onsets follow the flag and position.
  const bool beatNotBar = event.startTick % 96u == 0u && event.startTick % 384u != 0u;
  uint16_t reduction = 0;
  if (ghost) {
    reduction = space;
  } else if (beatNotBar) {
    reduction = (space * 35u) / 100u;         // beats 2-4 can drop; the bar start stays
  } else if (isWeakOnset(event.startTick)) {
    reduction = (space * 85u) / 100u;
  }
  if (reduction != 0) {
    event.probability = static_cast<uint8_t>(
        (static_cast<uint16_t>(event.probability) * (100u - reduction)) / 100u);
  }
}

// Longest gate that still ends inside the bar: 16 subticks per tick.
inline uint32_t maxSpaceDuration(uint16_t lengthTicks, uint16_t startTick) {
  return lengthTicks > startTick ? static_cast<uint32_t>(lengthTicks - startTick) * 16u : 0u;
}

inline void applySpace(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                       int space) {
  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    spaceEvent(event, space, maxSpaceDuration(phrase.lengthTicks, event.startTick));
  }
}

}  // namespace detail

// Transform one onset into a bounded transient playback event. The caller
// supplies the nearest strictly later source onset (when one exists) so the
// real-time path does not scan or allocate a Phrase-sized buffer. eventIndex
// and nextEventIndex preserve the batch transform's deterministic identities.
inline ApplyResult transformEvent(
    const PhraseRuntime::RuntimeSynthEvent& source,
    const PhraseRuntime::RuntimeSynthEvent* nextEvent,
    uint16_t eventIndex, uint16_t nextEventIndex, uint16_t lengthTicks,
    const State& state, const HarmonyContext& harmony, uint32_t seed,
    PhraseRuntime::RuntimeSynthEvent& destination) {
  if (!state.valid()) return ApplyResult::InvalidState;
  if (!detail::validHarmony(harmony)) return ApplyResult::InvalidHarmony;
  if (!detail::validEvent(source, lengthTicks) ||
      (nextEvent != nullptr && !detail::validEvent(*nextEvent, lengthTicks))) {
    return ApplyResult::InvalidSource;
  }

  PhraseRuntime::RuntimeSynthEvent candidate = source;
  candidate.note = detail::mutatedNote(source, eventIndex, state, harmony, seed);

  PhraseRuntime::RuntimeSynthEvent nextCandidate{};
  if (nextEvent != nullptr && nextEvent->startTick > source.startTick) {
    nextCandidate = *nextEvent;
    nextCandidate.note = detail::mutatedNote(
        *nextEvent, nextEventIndex, state, harmony, seed);
  }
  detail::tensionEvent(candidate,
      nextEvent != nullptr && nextEvent->startTick > source.startTick
          ? &nextCandidate : nullptr, state.tension);

  candidate.velocity = detail::energyVelocity(candidate.velocity, state.energy);
  if ((candidate.flags & PhraseRuntime::kEventGhost) != 0) {
    candidate.probability = detail::energyProbability(candidate.probability, state.energy);
  }

  detail::spaceEvent(candidate, state.space,
                     detail::maxSpaceDuration(lengthTicks, candidate.startTick));

  if (!detail::validEvent(candidate, lengthTicks)) {
    return ApplyResult::InvalidCandidate;
  }
  destination = candidate;
  return ApplyResult::Ready;
}

// Produce a transient playback view into an unpublished destination buffer on
// the control side. Source and destination must be distinct. Do not call this
// batch transform from the audio callback: TENSION scans later onsets. Scene
// and storage are intentionally absent. Callers publish the completed buffer
// through the existing playback owner at a safe boundary.
inline ApplyResult apply(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const State& state,
    const HarmonyContext& harmony,
    uint32_t seed,
    PhraseRuntime::RuntimeSynthEventBuffer& destination) {
  if (!state.valid()) return ApplyResult::InvalidState;
  if (!detail::validHarmony(harmony)) return ApplyResult::InvalidHarmony;
  if (!RuntimePhraseEdit::validate(source)) return ApplyResult::InvalidSource;
  if (&source == &destination) return ApplyResult::AliasedBuffers;

  destination = source;
  detail::applyMutation(destination, state, harmony, seed);
  detail::applyTension(destination, state.tension);
  detail::applyEnergy(destination, state.energy);
  detail::applySpace(destination, state.space);
  if (!RuntimePhraseEdit::validate(destination)) {
    destination = source;
    return ApplyResult::InvalidCandidate;
  }

  return ApplyResult::Ready;
}

static_assert(std::is_trivially_copyable<State>::value,
              "performance macro state must remain a fixed value object");
static_assert(sizeof(State) == 4,
              "performance macro state must remain four bytes");
static_assert(std::is_trivially_copyable<HarmonyContext>::value,
              "macro harmony context must remain a fixed value object");

}  // namespace Macros
}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_MACRO_ENGINE_H
