#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#include "runtime_synth_events.h"

// P3-U1 bounded mutation boundary for the session-only per-synth Phrase buffer.
//
// Musical edits must never mutate the live RuntimeSynthEventBuffer incrementally.
// Callers prepare a complete before-image copy, mutate the copy, validate it,
// and only then replace the live buffer in one commit step. The caller remains
// responsible for executing commit() inside the existing audio/control guard.
// This owner deliberately knows nothing about Scene persistence, Undo or note
// lifetime; those are separate established owners/checkpoints.
namespace RuntimePhraseEdit {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

static_assert(std::is_trivially_copyable<Buffer>::value,
              "runtime Phrase buffer must remain a bounded value object");

enum class PrepareResult : uint8_t {
  Ready = 0,
  NoChange,
  Rejected,
};

inline bool validLengthTicks(uint16_t lengthTicks) {
  return lengthTicks == PhraseRuntime::kTicksPerBar ||
         lengthTicks == 2u * PhraseRuntime::kTicksPerBar ||
         lengthTicks == 4u * PhraseRuntime::kTicksPerBar ||
         lengthTicks == 8u * PhraseRuntime::kTicksPerBar;
}

inline bool validate(const Buffer& phrase) {
  if (!validLengthTicks(phrase.lengthTicks)) return false;
  if (phrase.count > PhraseRuntime::kMaxSynthEvents) return false;

  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(phrase.lengthTicks) *
      PhraseRuntime::kSubticksPerTick;

  for (uint16_t i = 0; i < phrase.count; ++i) {
    const PhraseRuntime::RuntimeSynthEvent& event = phrase.events[i];
    if (event.durationSubticks == 0) return false;
    if (event.startTick >= phrase.lengthTicks) return false;

    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) *
        PhraseRuntime::kSubticksPerTick;
    const uint32_t endSubtick =
        startSubtick + static_cast<uint32_t>(event.durationSubticks);
    if (endSubtick > phraseEndSubtick) return false;
  }

  return true;
}

inline bool same(const Buffer& lhs, const Buffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(Buffer)) == 0;
}

template <typename Mutator>
PrepareResult prepare(const Buffer& live, Buffer& prepared, Mutator&& mutator) {
  if (!validate(live)) return PrepareResult::Rejected;

  prepared = live;
  std::forward<Mutator>(mutator)(prepared);

  if (!validate(prepared)) return PrepareResult::Rejected;
  if (same(prepared, live)) return PrepareResult::NoChange;
  return PrepareResult::Ready;
}

// Complete-buffer replacement only. No field-level live mutation is exposed by
// this owner. The caller must hold the existing audio/control mutation guard.
inline bool commit(Buffer& live, const Buffer& prepared) {
  if (!validate(prepared)) return false;
  live = prepared;
  return true;
}

}  // namespace RuntimePhraseEdit
