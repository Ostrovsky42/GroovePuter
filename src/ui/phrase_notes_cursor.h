#pragma once

#include <cstdint>

#include "../phrase/runtime_phrase_edit.h"

namespace PhraseNotesCursor {

struct State {
  uint8_t cell = 0;
  RuntimePhraseEdit::Grid grid = RuntimePhraseEdit::Grid::Sixteenth;
};

inline uint16_t quantumTicks(RuntimePhraseEdit::Grid grid) {
  return RuntimePhraseEdit::gridTicks(grid);
}

inline uint16_t tick(const State& state) {
  return static_cast<uint16_t>(
      static_cast<uint16_t>(state.cell) * quantumTicks(state.grid));
}

inline uint8_t maxCell(uint16_t lengthTicks, RuntimePhraseEdit::Grid grid) {
  if (!RuntimePhraseEdit::validLengthTicks(lengthTicks)) return 0;
  const uint16_t quantum = quantumTicks(grid);
  if (quantum == 0 || lengthTicks < quantum) return 0;
  const uint16_t cells = static_cast<uint16_t>(lengthTicks / quantum);
  return static_cast<uint8_t>(cells > 0 ? cells - 1u : 0u);
}

inline State clamp(State state, uint16_t lengthTicks) {
  const uint8_t last = maxCell(lengthTicks, state.grid);
  if (state.cell > last) state.cell = last;
  return state;
}

inline State move(State state, int delta, uint16_t lengthTicks) {
  state = clamp(state, lengthTicks);
  if (delta != -1 && delta != 1) return state;

  int next = static_cast<int>(state.cell) + delta;
  if (next < 0) next = 0;
  const int last = static_cast<int>(maxCell(lengthTicks, state.grid));
  if (next > last) next = last;
  state.cell = static_cast<uint8_t>(next);
  return state;
}

inline int gridRank(RuntimePhraseEdit::Grid grid) {
  switch (grid) {
    case RuntimePhraseEdit::Grid::Eighth: return 0;
    case RuntimePhraseEdit::Grid::Sixteenth: return 1;
    case RuntimePhraseEdit::Grid::ThirtySecond: return 2;
  }
  return 1;
}

inline RuntimePhraseEdit::Grid gridForRank(int rank) {
  if (rank <= 0) return RuntimePhraseEdit::Grid::Eighth;
  if (rank >= 2) return RuntimePhraseEdit::Grid::ThirtySecond;
  return RuntimePhraseEdit::Grid::Sixteenth;
}

inline State changeGrid(State state, int direction, uint16_t lengthTicks) {
  state = clamp(state, lengthTicks);
  if (direction != -1 && direction != 1) return state;

  const uint16_t oldTick = tick(state);
  const int nextRank = gridRank(state.grid) + direction;
  state.grid = gridForRank(nextRank);

  const uint16_t quantum = quantumTicks(state.grid);
  const uint16_t snappedTick = quantum == 0
      ? 0
      : static_cast<uint16_t>((oldTick / quantum) * quantum);
  state.cell = quantum == 0
      ? 0
      : static_cast<uint8_t>(snappedTick / quantum);
  return clamp(state, lengthTicks);
}

inline uint8_t focusBar(const State& state) {
  return static_cast<uint8_t>(tick(state) / PhraseRuntime::kTicksPerBar);
}

inline const char* gridLabel(RuntimePhraseEdit::Grid grid) {
  switch (grid) {
    case RuntimePhraseEdit::Grid::Eighth: return "1/8";
    case RuntimePhraseEdit::Grid::Sixteenth: return "1/16";
    case RuntimePhraseEdit::Grid::ThirtySecond: return "1/32";
  }
  return "1/16";
}

}  // namespace PhraseNotesCursor
