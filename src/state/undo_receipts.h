#pragma once

#ifndef GROOVEPUTER_STATE_UNDO_RECEIPTS_H
#define GROOVEPUTER_STATE_UNDO_RECEIPTS_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../scenes.h"
#include "../phrase/runtime_synth_events.h"
#include "undo_owner.h"

namespace GroovePuterUndo {

struct SynthPatternUndoPayload {
  uint8_t pageIndex{0};
  uint8_t synthIndex{0};
  uint8_t bankIndex{0};
  uint8_t patternIndex{0};
  SynthPattern before{};
};

static_assert(sizeof(SynthPatternUndoPayload) == 116,
              "R2 Synth Pattern Undo receipt size changed");
static_assert(std::is_trivially_copyable<SynthPatternUndoPayload>::value,
              "Synth Pattern Undo receipt must remain a fixed value");
static_assert(sizeof(SynthPatternUndoPayload) <= kUndoPayloadBytes,
              "Synth Pattern Undo receipt exceeds canonical owner capacity");

inline bool validSynthPatternAddress(const SynthPatternUndoPayload& receipt) {
  return receipt.pageIndex < kMaxPages &&
         receipt.synthIndex < 2 &&
         receipt.bankIndex < kBankCount &&
         receipt.patternIndex < Bank<SynthPattern>::kPatterns;
}

inline bool captureCurrentSynthPatternUndo(SceneManager& manager,
                                           int synthIndex,
                                           SynthPatternUndoPayload& receipt) {
  if (synthIndex < 0 || synthIndex >= 2) return false;
  const int page = manager.currentPageIndex();
  const int bank = manager.getCurrentBankIndex(synthIndex + 1);
  const int pattern = manager.getCurrentSynthPatternIndex(synthIndex);
  if (page < 0 || page >= kMaxPages ||
      bank < 0 || bank >= kBankCount ||
      pattern < 0 || pattern >= Bank<SynthPattern>::kPatterns) {
    return false;
  }

  receipt.pageIndex = static_cast<uint8_t>(page);
  receipt.synthIndex = static_cast<uint8_t>(synthIndex);
  receipt.bankIndex = static_cast<uint8_t>(bank);
  receipt.patternIndex = static_cast<uint8_t>(pattern);
  receipt.before = manager.getCurrentSynthPattern(synthIndex);
  return true;
}

inline bool synthPatternUndoTargetAvailable(
    const SceneManager& manager,
    const SynthPatternUndoPayload& receipt) {
  return validSynthPatternAddress(receipt) &&
         manager.currentPageIndex() == receipt.pageIndex;
}

inline void restoreSynthPatternUndo(SceneManager& manager,
                                    const SynthPatternUndoPayload& receipt) {
  Scene& scene = manager.currentScene();
  if (receipt.synthIndex == 0) {
    scene.synthABanks[receipt.bankIndex].patterns[receipt.patternIndex] =
        receipt.before;
  } else {
    scene.synthBBanks[receipt.bankIndex].patterns[receipt.patternIndex] =
        receipt.before;
  }
}

inline bool isCanonicalClearedSynthPattern(const SynthPattern& pattern) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& value = pattern.steps[step];
    if (value.note != -1 || value.slide || value.accent || value.ghost ||
        value.velocity != 100 || value.timing != 0 || value.fx != 0 ||
        value.fxParam != 0 || value.probability != 100) {
      return false;
    }
  }
  return true;
}

// R4 owns committed Song arrangement edits only. Transport position/mode/loop
// and playback-slot switching are runtime/TIME. This receipt does not describe
// Phrase->Song generation; that domain has its own Generation receipt.
struct SongUndoPayload {
  uint8_t pageIndex{0};
  uint8_t songSlot{0};
  Song before{};
};

static_assert(std::is_trivially_copyable<SongUndoPayload>::value,
              "Song Undo receipt must remain a fixed value");
static_assert(sizeof(SongUndoPayload) <= 1040,
              "R4 Song Undo receipt exceeded the measured ~1 KiB budget");
static_assert(sizeof(SongUndoPayload) <= kUndoPayloadBytes,
              "Song Undo receipt exceeds canonical owner capacity");

inline bool sameSong(const Song& lhs, const Song& rhs) {
  if (lhs.length != rhs.length || lhs.reverse != rhs.reverse) return false;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      if (lhs.positions[row].patterns[track] != rhs.positions[row].patterns[track]) {
        return false;
      }
    }
  }
  return true;
}

inline bool captureCurrentSongUndo(SceneManager& manager,
                                   SongUndoPayload& receipt) {
  const int page = manager.currentPageIndex();
  const int slot = manager.activeSongSlot();
  if (page < 0 || page >= kMaxPages || slot < 0 || slot > 1) return false;
  receipt = SongUndoPayload{};
  receipt.pageIndex = static_cast<uint8_t>(page);
  receipt.songSlot = static_cast<uint8_t>(slot);
  receipt.before = manager.currentScene().songs[slot];
  return true;
}

inline bool songUndoTargetAvailable(const SceneManager& manager,
                                    const SongUndoPayload& receipt) {
  return receipt.pageIndex < kMaxPages && receipt.songSlot < 2 &&
         manager.currentPageIndex() == receipt.pageIndex;
}

inline void restoreSongUndo(SceneManager& manager,
                            const SongUndoPayload& receipt) {
  manager.currentScene().songs[receipt.songSlot] = receipt.before;
}

// Phrase capture/derive/clear can change nextPhraseId as well as one slot. The
// whole fixed 244-byte bank is still a bounded domain receipt, not a Scene
// snapshot, and provides exact rollback for every current Phrase command.
struct PhraseUndoPayload {
  uint8_t pageIndex{0};
  PhraseCore::PhraseBank before{};
};

static_assert(std::is_trivially_copyable<PhraseUndoPayload>::value,
              "Phrase Undo receipt must remain a fixed value");
static_assert(sizeof(PhraseUndoPayload) <= 248,
              "R4 Phrase Undo receipt exceeded the fixed PhraseBank budget");
static_assert(sizeof(PhraseUndoPayload) <= kUndoPayloadBytes,
              "Phrase Undo receipt exceeds canonical owner capacity");

inline bool samePhraseBank(const PhraseCore::PhraseBank& lhs,
                           const PhraseCore::PhraseBank& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

inline bool phraseUndoTargetAvailable(const SceneManager& manager,
                                      const PhraseUndoPayload& receipt) {
  return receipt.pageIndex < kMaxPages &&
         manager.currentPageIndex() == receipt.pageIndex;
}

inline void restorePhraseUndo(SceneManager& manager,
                              const PhraseUndoPayload& receipt) {
  manager.currentScene().phraseBank = receipt.before;
}

// P3 Runtime Phrase is session-only and is not represented by the Scene codec.
// It therefore gets a distinct receipt in the same canonical Undo slot instead
// of aliasing the persisted PhraseCore bank kind. Source is kept as its bounded
// enum byte so the receipt layer stays independent from MiniAcid headers.
struct RuntimePhraseUndoPayload {
  uint8_t voiceIndex{0};
  uint8_t source{0};
  PhraseRuntime::RuntimeSynthEventBuffer before{};
};

static_assert(std::is_trivially_copyable<RuntimePhraseUndoPayload>::value,
              "runtime Phrase Undo receipt must remain a fixed value");
static_assert(sizeof(RuntimePhraseUndoPayload) <= kUndoPayloadBytes,
              "runtime Phrase Undo receipt exceeds canonical owner capacity");

inline bool validRuntimePhraseUndoPayload(
    const RuntimePhraseUndoPayload& receipt) {
  return receipt.voiceIndex < 2 && receipt.source <= 1;
}

inline bool sameRuntimePhraseBuffer(
    const PhraseRuntime::RuntimeSynthEventBuffer& lhs,
    const PhraseRuntime::RuntimeSynthEventBuffer& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

struct DrumPatternUndoPayload {
  int16_t pageIndex{-1};
  int16_t bankIndex{-1};
  int16_t patternIndex{-1};
  DrumPatternSet before{};
};

static_assert(std::is_trivially_copyable<DrumPatternUndoPayload>::value,
              "Drum Pattern Undo receipt must remain fixed and trivially copyable");
static_assert(sizeof(DrumPatternUndoPayload) <= kUndoPayloadBytes,
              "Drum Pattern Undo receipt exceeds canonical owner capacity");

inline bool captureCurrentDrumPatternUndo(SceneManager& manager,
                                          DrumPatternUndoPayload& out) {
  const int page = manager.currentPageIndex();
  const int bank = manager.getCurrentBankIndex(0);
  const int pattern = manager.getCurrentDrumPatternIndex();
  if (page < 0 || page >= kMaxPages || bank < 0 || bank >= kBankCount ||
      pattern < 0 || pattern >= Bank<DrumPatternSet>::kPatterns) return false;
  out.pageIndex = static_cast<int16_t>(page);
  out.bankIndex = static_cast<int16_t>(bank);
  out.patternIndex = static_cast<int16_t>(pattern);
  out.before = manager.currentScene().drumBanks[bank].patterns[pattern];
  return true;
}

inline bool drumPatternUndoTargetAvailable(
    const SceneManager& manager,
    const DrumPatternUndoPayload& receipt) {
  return receipt.pageIndex >= 0 && receipt.pageIndex < kMaxPages &&
         receipt.pageIndex == manager.currentPageIndex() &&
         receipt.bankIndex >= 0 && receipt.bankIndex < kBankCount &&
         receipt.patternIndex >= 0 &&
         receipt.patternIndex < Bank<DrumPatternSet>::kPatterns;
}

inline bool sameDrumPattern(const DrumPatternSet& lhs,
                            const DrumPatternSet& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(DrumPatternSet)) == 0;
}

inline void restoreDrumPatternUndo(SceneManager& manager,
                                   const DrumPatternUndoPayload& receipt) {
  manager.currentScene().drumBanks[receipt.bankIndex]
      .patterns[receipt.patternIndex] = receipt.before;
}

inline void exchangeSynthPatternUndo(SceneManager& manager,
                                     SynthPatternUndoPayload& receipt) {
  Scene& scene = manager.currentScene();
  Bank<SynthPattern>& bank = receipt.synthIndex == 0
      ? scene.synthABanks[receipt.bankIndex]
      : scene.synthBBanks[receipt.bankIndex];
  exchangeFixedValue(bank.patterns[receipt.patternIndex], receipt.before);
}

inline void exchangeSongUndo(SceneManager& manager, SongUndoPayload& receipt) {
  exchangeFixedValue(manager.currentScene().songs[receipt.songSlot], receipt.before);
}

inline void exchangePhraseUndo(SceneManager& manager, PhraseUndoPayload& receipt) {
  exchangeFixedValue(manager.currentScene().phraseBank, receipt.before);
}

inline void exchangeDrumPatternUndo(SceneManager& manager,
                                    DrumPatternUndoPayload& receipt) {
  exchangeFixedValue(
      manager.currentScene().drumBanks[receipt.bankIndex]
          .patterns[receipt.patternIndex],
      receipt.before);
}

}  // namespace GroovePuterUndo

#endif  // GROOVEPUTER_STATE_UNDO_RECEIPTS_H
