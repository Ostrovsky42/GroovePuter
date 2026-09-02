#pragma once

#include <cstdint>

namespace GroovePuterState {

// Product/session request owner for the next Generated Phrase operation.
// This is intentionally separate from PhraseCore capture length and from
// Scene::feel.patternBars (FEEL CYCLE). Musical admissibility remains owned by
// the frozen phrase-length policy; this state stores only the user's exact
// requested extent and never coerces either an invalid raw value or a typed
// musical rejection into another length.
inline bool isRequestedPhraseBarsValue(uint8_t bars) {
  switch (bars) {
    case 1:
    case 2:
    case 4:
    case 8:
      return true;
    default:
      return false;
  }
}

namespace phrase_generation_request_detail {
inline uint8_t& requestedBarsStorage() {
  static uint8_t bars = 4;
  return bars;
}
}  // namespace phrase_generation_request_detail

inline uint8_t requestedPhraseBars() {
  return phrase_generation_request_detail::requestedBarsStorage();
}

inline bool setRequestedPhraseBars(uint8_t bars) {
  if (!isRequestedPhraseBarsValue(bars)) return false;
  uint8_t& current = phrase_generation_request_detail::requestedBarsStorage();
  if (current == bars) return false;
  current = bars;
  return true;
}

inline uint8_t cycleRequestedPhraseBars(int direction = 1) {
  static constexpr uint8_t kBars[] = {1, 2, 4, 8};
  int index = 0;
  const uint8_t current = requestedPhraseBars();
  for (int i = 0; i < 4; ++i) {
    if (kBars[i] == current) {
      index = i;
      break;
    }
  }
  index += direction;
  while (index < 0) index += 4;
  while (index >= 4) index -= 4;
  (void)setRequestedPhraseBars(kBars[index]);
  return requestedPhraseBars();
}

}  // namespace GroovePuterState
