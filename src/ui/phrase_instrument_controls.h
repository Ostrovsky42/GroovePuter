#pragma once
#ifndef GROOVEPUTER_SRC_UI_PHRASE_INSTRUMENT_CONTROLS_H
#define GROOVEPUTER_SRC_UI_PHRASE_INSTRUMENT_CONTROLS_H

#include <cstdint>
#include <utility>

#include "phrase_notes_cursor.h"

// Small, stateless adapter for musical controls whose values live in the
// runtime/domain. It owns no Phrase material and no playback/source truth.
//
// LENGTH delegates to MiniAcid::setPhraseLength() through a caller-provided
// command. BAR changes only the UI edit cursor. GRID remains in the existing
// PhraseNotesCursor presentation-continuity state.
namespace PhraseInstrumentControls {

inline uint8_t lengthBars(uint16_t lengthTicks) {
  if (!RuntimePhraseEdit::validLengthTicks(lengthTicks)) return 1;
  const uint16_t bars =
      static_cast<uint16_t>(lengthTicks / PhraseRuntime::kTicksPerBar);
  switch (bars) {
    case 1: return 1;
    case 2: return 2;
    case 4: return 4;
    case 8: return 8;
    default: return 1;
  }
}

inline uint8_t nextLengthBars(uint8_t current, int direction) {
  if (direction == 0) return current;
  static constexpr uint8_t kBars[] = {1, 2, 4, 8};
  int index = 0;
  for (int i = 0; i < 4; ++i) {
    if (kBars[i] == current) {
      index = i;
      break;
    }
  }
  index += direction > 0 ? 1 : -1;
  if (index < 0) index = 3;
  if (index > 3) index = 0;
  return kBars[index];
}

// Prepare a requested Phrase extent without treating the old extent as a
// lifetime barrier. Expansion first grants the new temporal territory, then
// validates the unchanged events against that requested extent. Shrink keeps
// the existing RuntimePhraseEdit policy: reject any contraction that would cut
// or discard material. This helper is UI transaction preparation only; it owns
// no live Phrase buffer and performs no runtime commit.
inline bool prepareLengthTarget(
    const PhraseRuntime::RuntimeSynthEventBuffer& before,
    uint8_t bars,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
  const uint16_t targetTicks = RuntimePhraseEdit::lengthTicksForBars(bars);
  if (targetTicks == 0 || targetTicks == before.lengthTicks) return false;

  candidate = before;
  if (targetTicks > before.lengthTicks) {
    candidate.lengthTicks = targetTicks;
    return RuntimePhraseEdit::validate(candidate);
  }

  return RuntimePhraseEdit::setLengthBars(candidate, bars) ==
         RuntimePhraseEdit::LengthEditResult::Changed;
}

enum class LengthChangeResult : uint8_t {
  Changed = 0,
  Unchanged,
  InvalidDirection,
  InvalidLength,
  WouldTruncateEvent,
  CommitFailed,
};

struct LengthChangeOutcome {
  LengthChangeResult result = LengthChangeResult::Unchanged;
  uint8_t targetBars = 1;
};

inline bool shrinkWouldTruncateEvent(
    const PhraseRuntime::RuntimeSynthEventBuffer& current,
    uint16_t targetTicks) {
  if (targetTicks >= current.lengthTicks) return false;
  const uint32_t targetEndSubtick =
      static_cast<uint32_t>(targetTicks) * PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < current.count; ++i) {
    const auto& event = current.events[i];
    if (event.startTick >= targetTicks) return true;
    const uint32_t eventEndSubtick =
        static_cast<uint32_t>(event.startTick) *
            PhraseRuntime::kSubticksPerTick +
        static_cast<uint32_t>(event.durationSubticks);
    if (eventEndSubtick > targetEndSubtick) return true;
  }
  return false;
}

// Detailed UI preflight. RuntimePhraseEdit intentionally exposes only the
// mutation-level Rejected result. The surface still needs to distinguish the
// musically meaningful safe-shrink refusal from a stale/invalid commit, so we
// classify whether existing notes cross the requested boundary before calling
// the owner command. No live buffer is modified during this classification.
template <typename SetLengthFn>
inline LengthChangeOutcome applyLengthChangeDetailed(
    const PhraseRuntime::RuntimeSynthEventBuffer& current,
    int direction,
    SetLengthFn&& setLength) {
  LengthChangeOutcome outcome{};
  if (!RuntimePhraseEdit::validLengthTicks(current.lengthTicks)) {
    outcome.result = LengthChangeResult::InvalidLength;
    return outcome;
  }
  outcome.targetBars = lengthBars(current.lengthTicks);
  if (direction != -1 && direction != 1) {
    outcome.result = LengthChangeResult::InvalidDirection;
    return outcome;
  }

  outcome.targetBars = nextLengthBars(outcome.targetBars, direction);
  const uint16_t targetTicks =
      RuntimePhraseEdit::lengthTicksForBars(outcome.targetBars);
  if (targetTicks == 0) {
    outcome.result = LengthChangeResult::InvalidLength;
    return outcome;
  }
  if (targetTicks == current.lengthTicks) {
    outcome.result = LengthChangeResult::Unchanged;
    return outcome;
  }

  if (targetTicks > current.lengthTicks) {
    PhraseRuntime::RuntimeSynthEventBuffer candidate{};
    if (!prepareLengthTarget(current, outcome.targetBars, candidate)) {
      outcome.result = LengthChangeResult::InvalidLength;
      return outcome;
    }
  } else {
    if (!RuntimePhraseEdit::validate(current)) {
      outcome.result = LengthChangeResult::InvalidLength;
      return outcome;
    }
    if (shrinkWouldTruncateEvent(current, targetTicks)) {
      outcome.result = LengthChangeResult::WouldTruncateEvent;
      return outcome;
    }
  }

  const bool committed =
      std::forward<SetLengthFn>(setLength)(outcome.targetBars);
  outcome.result = committed
      ? LengthChangeResult::Changed
      : LengthChangeResult::CommitFailed;
  return outcome;
}

template <typename SetLengthFn>
inline bool applyLengthChange(uint16_t currentLengthTicks,
                              int direction,
                              SetLengthFn&& setLength) {
  if (direction != -1 && direction != 1) return false;
  const uint8_t target =
      nextLengthBars(lengthBars(currentLengthTicks), direction);
  return std::forward<SetLengthFn>(setLength)(target);
}

inline PhraseNotesCursor::State jumpBar(PhraseNotesCursor::State state,
                                        int direction,
                                        uint16_t lengthTicks) {
  state = PhraseNotesCursor::clamp(state, lengthTicks);
  if ((direction != -1 && direction != 1) ||
      !RuntimePhraseEdit::validLengthTicks(lengthTicks)) {
    return state;
  }

  const uint8_t totalBars = lengthBars(lengthTicks);
  int targetBar = static_cast<int>(PhraseNotesCursor::focusBar(state)) + direction;
  if (targetBar < 0) targetBar = 0;
  if (targetBar >= static_cast<int>(totalBars)) {
    targetBar = static_cast<int>(totalBars) - 1;
  }

  const uint16_t quantum = PhraseNotesCursor::quantumTicks(state.grid);
  if (quantum == 0) return state;

  const uint16_t withinBar = static_cast<uint16_t>(
      PhraseNotesCursor::tick(state) % PhraseRuntime::kTicksPerBar);
  const uint32_t targetTick =
      static_cast<uint32_t>(targetBar) * PhraseRuntime::kTicksPerBar + withinBar;
  state.cell = static_cast<uint8_t>(targetTick / quantum);
  return PhraseNotesCursor::clamp(state, lengthTicks);
}

}  // namespace PhraseInstrumentControls

#endif  // GROOVEPUTER_SRC_UI_PHRASE_INSTRUMENT_CONTROLS_H
