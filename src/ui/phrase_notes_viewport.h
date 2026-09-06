#pragma once
#include <cstdint>
#include "../phrase/runtime_synth_events.h"

namespace PhraseNotesViewport {
struct Window {
  uint8_t totalBars = 0;
  uint8_t focusBar = 0;
  uint8_t startBar = 0;
  uint8_t barCount = 0;
};

inline uint8_t barsForLength(uint16_t lengthTicks) {
  const uint16_t bar = PhraseRuntime::kTicksPerBar;
  if (lengthTicks == bar) return 1;
  if (lengthTicks == static_cast<uint16_t>(2u * bar)) return 2;
  if (lengthTicks == static_cast<uint16_t>(4u * bar)) return 4;
  if (lengthTicks == static_cast<uint16_t>(8u * bar)) return 8;
  return 0;
}

inline Window resolve(uint16_t lengthTicks, uint8_t requestedFocusBar) {
  Window out{};
  out.totalBars = barsForLength(lengthTicks);
  if (out.totalBars == 0) return out;
  out.focusBar = requestedFocusBar < out.totalBars
      ? requestedFocusBar
      : static_cast<uint8_t>(out.totalBars - 1u);
  if (out.totalBars <= 2u) {
    out.startBar = 0;
    out.barCount = out.totalBars;
    return out;
  }
  out.barCount = 2;
  out.startBar = out.focusBar == 0
      ? 0
      : static_cast<uint8_t>(out.focusBar - 1u);
  const uint8_t latestStart =
      static_cast<uint8_t>(out.totalBars - out.barCount);
  if (out.startBar > latestStart) out.startBar = latestStart;
  return out;
}

inline uint8_t moveFocus(uint8_t currentFocusBar,
                         int delta,
                         uint16_t lengthTicks) {
  const Window current = resolve(lengthTicks, currentFocusBar);
  if (current.totalBars == 0) return 0;
  int next = static_cast<int>(current.focusBar) + delta;
  if (next < 0) next = 0;
  const int last = static_cast<int>(current.totalBars) - 1;
  if (next > last) next = last;
  return static_cast<uint8_t>(next);
}
}  // namespace PhraseNotesViewport
