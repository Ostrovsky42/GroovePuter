#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_MACRO_ENGINE_H
#define GROOVEPUTER_PERFORMANCE_MACRO_ENGINE_H

#include <cstdint>
#include <type_traits>

#include "src/phrase/runtime_phrase_edit.h"
#include "src/ui/project_key.h"

namespace GroovePuterPerformance {
namespace Macros {

constexpr uint8_t kValueMax = 100;
constexpr uint16_t kBeatTicks = PhraseRuntime::kTicksPerBar / 4u;

enum class Control : uint8_t {
  Tension = 0,
  Energy,
  Space,
  Mutation,
};

struct State {
  uint8_t tension{0};
  uint8_t energy{0};
  uint8_t space{0};
  uint8_t mutation{0};

  bool set(Control control, uint8_t value) {
    if (value > kValueMax) return false;
    switch (control) {
      case Control::Tension: tension = value; return true;
      case Control::Energy: energy = value; return true;
      case Control::Space: space = value; return true;
      case Control::Mutation: mutation = value; return true;
    }
    return false;
  }

  bool valid() const {
    return tension <= kValueMax && energy <= kValueMax &&
           space <= kValueMax && mutation <= kValueMax;
  }
};

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

inline void applyMutation(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                          const State& state,
                          const HarmonyContext& harmony,
                          uint32_t seed) {
  if (state.mutation == 0 || !harmony.known) return;

  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    if (!isWeakOnset(event.startTick) ||
        !ProjectKey::inScale(event.note, harmony.root, harmony.scale)) {
      continue;
    }

    const uint32_t identity =
        seed ^ (static_cast<uint32_t>(event.startTick) * 0x9E3779B9u) ^
        (static_cast<uint32_t>(i) * 0x85EBCA6Bu);
    const uint32_t choice = mixSeed(identity);
    if ((choice % 100u) >= state.mutation) continue;

    const int direction = (choice & 0x100u) != 0 ? 1 : -1;
    const int neighbor = ProjectKey::step(
        event.note, direction, harmony.root, harmony.scale);
    if (neighbor >= 0 && neighbor <= 127) {
      event.note = static_cast<uint8_t>(neighbor);
    }
  }
}

inline void applyTension(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                         uint8_t tension) {
  if (tension == 0) return;
  const uint8_t accent = static_cast<uint8_t>((tension * 12u + 50u) / 100u);
  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    if (!isWeakOnset(event.startTick)) continue;

    const PhraseRuntime::RuntimeSynthEvent* next = nullptr;
    for (uint16_t j = static_cast<uint16_t>(i + 1u); j < phrase.count; ++j) {
      if (phrase.events[j].startTick > event.startTick &&
          (next == nullptr || phrase.events[j].startTick < next->startTick)) {
        next = &phrase.events[j];
      }
    }
    if (next == nullptr) continue;

    const int interval = static_cast<int>(next->note) - event.note;
    const int absoluteInterval = interval < 0 ? -interval : interval;
    if (absoluteInterval == 0 || absoluteInterval > 2) continue;
    const uint16_t velocity = static_cast<uint16_t>(event.velocity) + accent;
    event.velocity = static_cast<uint8_t>(velocity > 127u ? 127u : velocity);
  }
}

inline void applyEnergy(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                        uint8_t energy) {
  if (energy == 0) return;
  const uint16_t gainPercent = static_cast<uint16_t>(100u + energy / 2u);
  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    const uint16_t velocity =
        (static_cast<uint16_t>(event.velocity) * gainPercent + 50u) / 100u;
    event.velocity = static_cast<uint8_t>(velocity > 127u ? 127u : velocity);

    if ((event.flags & PhraseRuntime::kEventGhost) == 0) continue;
    const uint16_t missing = static_cast<uint16_t>(100u - event.probability);
    const uint16_t boost =
        (missing * energy + 50u) / 100u;
    const uint16_t probability =
        static_cast<uint16_t>(event.probability) + boost;
    event.probability = static_cast<uint8_t>(probability > 100u ? 100u : probability);
  }
}

inline void applySpace(PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                       uint8_t space) {
  if (space == 0) return;
  const uint16_t durationPercent = static_cast<uint16_t>(100u - space / 2u);
  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    uint32_t duration =
        (static_cast<uint32_t>(event.durationSubticks) * durationPercent) / 100u;
    if (duration == 0) duration = 1;
    event.durationSubticks = static_cast<uint16_t>(duration);

    if ((event.flags & PhraseRuntime::kEventGhost) == 0) continue;
    event.probability = static_cast<uint8_t>(
        (static_cast<uint16_t>(event.probability) * (100u - space)) / 100u);
  }
}

}  // namespace detail

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
