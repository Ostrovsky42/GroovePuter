#pragma once

#include <cstdint>

#include "../phrase/runtime_synth_events.h"

// Where the playback position strip stands inside a bar drawn `width` pixels
// wide from `left`. One mapping for the song-voice Melody overview and the
// Melody roll, so both show the same step at the same place.
namespace MelodyPlayhead {

inline int x(uint16_t phraseTick, int left, int width) {
  const uint16_t inBar =
      static_cast<uint16_t>(phraseTick % PhraseRuntime::kTicksPerBar);
  return left + static_cast<int>(static_cast<uint32_t>(inBar) *
                                 static_cast<uint32_t>(width - 1) /
                                 PhraseRuntime::kTicksPerBar);
}

// A note sounds from its start up to (not including) its audible end.
inline bool isSounding(uint32_t playTick, uint32_t start, uint32_t audibleEnd) {
  return start <= playTick && playTick < audibleEnd;
}

}  // namespace MelodyPlayhead
