#ifndef GROOVEPUTER_SRC_STATE_GENERATED_PHRASE_UNDO_PAYLOAD_H
#define GROOVEPUTER_SRC_STATE_GENERATED_PHRASE_UNDO_PAYLOAD_H

#include <cstdint>

#include "../../scenes.h"

// The Undo receipt of one generated phrase or cycle (moved out of generated_phrase_song.h so the
// slot-reuse module can read the slot range it protects without duplicating the layout).
namespace GeneratedPhraseSong {

constexpr uint32_t kGeneratedPhraseUndoTag = 0x44325048u;  // "D2PH"

struct GeneratedPhraseUndoPayload {
  uint32_t tag = kGeneratedPhraseUndoTag;
  Song beforeSong{};
  int16_t pageIndex = -1;
  int16_t songSlot = -1;
  int16_t songStart = -1;
  int16_t firstLocalSlot = -1;
  int16_t bars = 0;
  int16_t previousPatternBars = 1;
};

}  // namespace GeneratedPhraseSong

#endif  // GROOVEPUTER_SRC_STATE_GENERATED_PHRASE_UNDO_PAYLOAD_H
