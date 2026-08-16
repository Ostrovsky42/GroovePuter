#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../scenes.h"

namespace GroovePuterUndo {

// R4: one complete Song before-image. The target slot is edit identity; active
// Song selection is runtime/editor context and is intentionally not restored.
struct SongUndoPayload {
  uint8_t songSlot{0};
  Song before{};
};

static_assert(std::is_trivially_copyable<SongUndoPayload>::value,
              "Song Undo receipt must remain a fixed value");
static_assert(sizeof(SongUndoPayload) <= 1040,
              "R4 Song Undo receipt exceeded the measured R2 payload budget");

inline bool validSongAddress(const SongUndoPayload& receipt) {
  return receipt.songSlot < 2;
}

inline bool captureSongUndo(const SceneManager& manager,
                            int songSlot,
                            SongUndoPayload& receipt) {
  if (songSlot < 0 || songSlot >= 2) return false;
  receipt.songSlot = static_cast<uint8_t>(songSlot);
  receipt.before = manager.currentScene().songs[songSlot];
  return true;
}

inline bool songUndoTargetAvailable(const SceneManager&,
                                    const SongUndoPayload& receipt) {
  return validSongAddress(receipt);
}

inline void restoreSongUndo(SceneManager& manager,
                            const SongUndoPayload& receipt) {
  if (!validSongAddress(receipt)) return;
  manager.currentScene().songs[receipt.songSlot] = receipt.before;
}

inline bool songsEqual(const Song& lhs, const Song& rhs) {
  static_assert(std::is_trivially_copyable<Song>::value,
                "Song comparison assumes a fixed value type");
  return std::memcmp(&lhs, &rhs, sizeof(Song)) == 0;
}

// CAPTURE and DERIVE allocate Phrase IDs. Restoring only the edited PhraseSlot
// would leave nextPhraseId advanced, so R4 intentionally snapshots the whole
// bounded PhraseBank. Selector state remains outside the receipt.
struct PhraseBankUndoPayload {
  PhraseCore::PhraseBank before{};
};

static_assert(std::is_trivially_copyable<PhraseBankUndoPayload>::value,
              "PhraseBank Undo receipt must remain a fixed value");
static_assert(sizeof(PhraseBankUndoPayload) <= 256,
              "R4 PhraseBank Undo receipt exceeded the measured R2 budget");

inline PhraseBankUndoPayload capturePhraseBankUndo(const SceneManager& manager) {
  PhraseBankUndoPayload receipt{};
  receipt.before = manager.currentScene().phraseBank;
  return receipt;
}

inline void restorePhraseBankUndo(SceneManager& manager,
                                  const PhraseBankUndoPayload& receipt) {
  manager.currentScene().phraseBank = receipt.before;
}

inline bool phraseBanksEqual(const PhraseCore::PhraseBank& lhs,
                             const PhraseCore::PhraseBank& rhs) {
  static_assert(std::is_trivially_copyable<PhraseCore::PhraseBank>::value,
                "PhraseBank comparison assumes a fixed value type");
  return std::memcmp(&lhs, &rhs, sizeof(PhraseCore::PhraseBank)) == 0;
}

}  // namespace GroovePuterUndo
