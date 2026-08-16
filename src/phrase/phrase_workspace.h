#pragma once

#include <cstdint>
#include <utility>

#include "phrase_song_insert.h"
#include "src/state/undo_owner.h"
#include "src/state/song_phrase_undo_receipts.h"

namespace PhraseWorkspace {

struct CaptureRequest {
  PhraseCore::SlotId targetSlot = PhraseCore::SlotId::A;
  uint8_t sourceSongSlot = 0;
  uint8_t startRow = 0;
  uint8_t lengthBars = 1;
  PhraseCore::Role role = PhraseCore::Role::Main;
  PhraseCore::Source source = PhraseCore::Source::InternalPattern;
  uint8_t trackMask = PhraseCore::kAllTracks;
};

struct DeriveRequest {
  PhraseCore::SlotId targetSlot = PhraseCore::SlotId::B;
  PhraseCore::SlotId parentSlot = PhraseCore::SlotId::A;
  PhraseCore::Role role = PhraseCore::Role::Variation;
};

struct WriteRequest {
  PhraseCore::SlotId sourceSlot = PhraseCore::SlotId::A;
  uint8_t destinationSongSlot = 0;
  uint8_t startRow = 0;
  // false = INSERT at startRow and shift existing Song rows down.
  // true = REPLACE only the Phrase lanes at startRow without shifting rows.
  bool overwrite = false;
};

inline const char* errorName(PhraseCore::Error error) {
  switch (error) {
    case PhraseCore::Error::None: return "OK";
    case PhraseCore::Error::InvalidSlot: return "SLOT";
    case PhraseCore::Error::InvalidLength: return "LENGTH";
    case PhraseCore::Error::InvalidTrackMask: return "TRACKS";
    case PhraseCore::Error::InvalidSongSlot: return "SONG";
    case PhraseCore::Error::InvalidSource: return "SOURCE";
    case PhraseCore::Error::InvalidRole: return "ROLE";
    case PhraseCore::Error::RegionOutOfRange: return "RANGE";
    case PhraseCore::Error::EmptyRegion: return "EMPTY";
    case PhraseCore::Error::InvalidParent: return "PARENT";
    case PhraseCore::Error::DestinationOccupied: return "OCCUPIED";
    case PhraseCore::Error::InvalidPhrase: return "EMPTY";
  }
  return "ERROR";
}

inline PhraseCore::SlotSummary summary(const Scene& scene,
                                       PhraseCore::SlotId slot) {
  return PhraseCore::summarize(scene.phraseBank, slot);
}

inline bool barPreview(const Scene& scene,
                       int currentPageIndex,
                       PhraseCore::SlotId slot,
                       uint8_t bar,
                       PhraseCore::BarPreview& output) {
  const PhraseCore::PhraseSlot* phrase =
      PhraseCore::slotAt(scene.phraseBank, slot);
  return phrase && PhraseCore::buildBarPreview(
                       *phrase, bar, scene, currentPageIndex, output);
}

// R4 preserves the existing synchronous AudioGuard contract but moves Scene
// revision ownership to the canonical UndoOwner. PREPARE runs only on fixed
// local copies; COMMIT only assigns the already-prepared bounded value.
template <typename Commit, typename Prepare>
PhraseCore::Result commitPhraseBank(Scene& scene,
                                    PhraseCore::SlotId resultSlot,
                                    Commit&& commit,
                                    Prepare&& prepare) {
  GroovePuterUndo::PhraseBankUndoPayload before{};
  before.before = scene.phraseBank;
  PhraseCore::PhraseBank after = before.before;

  PhraseCore::Result result = std::forward<Prepare>(prepare)(after);
  if (!result) return result;
  if (GroovePuterUndo::phraseBanksEqual(before.before, after)) return result;

  auto&& guardedCommit = commit;
  const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
      UndoKind::Phrase, before, [&]() {
        guardedCommit([&]() { scene.phraseBank = after; });
      });
  if (!committed) {
    result = PhraseCore::Result{};
    result.slot = resultSlot;
    result.error = PhraseCore::Error::InvalidPhrase;
  }
  return result;
}

template <typename Commit>
PhraseCore::Result capture(Scene& scene,
                           const CaptureRequest& request,
                           Commit&& commit) {
  PhraseCore::Result invalid{};
  invalid.slot = request.targetSlot;
  if (request.sourceSongSlot > 1) {
    invalid.error = PhraseCore::Error::InvalidSongSlot;
    return invalid;
  }

  const Song sourceSong = scene.songs[request.sourceSongSlot];
  return commitPhraseBank(
      scene, request.targetSlot, std::forward<Commit>(commit),
      [&](PhraseCore::PhraseBank& bank) {
        return PhraseCore::captureSongRegion(
            bank,
            request.targetSlot,
            sourceSong,
            request.sourceSongSlot,
            request.startRow,
            request.lengthBars,
            request.role,
            request.source,
            request.trackMask);
      });
}

template <typename Commit>
PhraseCore::Result derive(Scene& scene,
                          const DeriveRequest& request,
                          Commit&& commit) {
  return commitPhraseBank(
      scene, request.targetSlot, std::forward<Commit>(commit),
      [&](PhraseCore::PhraseBank& bank) {
        return PhraseCore::deriveReferenceView(
            bank, request.targetSlot, request.parentSlot, request.role);
      });
}

template <typename Commit>
PhraseCore::Result clear(Scene& scene,
                         PhraseCore::SlotId slot,
                         Commit&& commit) {
  PhraseCore::Result invalid{};
  invalid.slot = slot;
  if (!PhraseCore::summarize(scene.phraseBank, slot).valid) {
    invalid.error = PhraseCore::Error::InvalidPhrase;
    return invalid;
  }

  return commitPhraseBank(
      scene, slot, std::forward<Commit>(commit),
      [&](PhraseCore::PhraseBank& bank) { return PhraseCore::clear(bank, slot); });
}

template <typename Commit>
PhraseCore::Result writeToSong(Scene& scene,
                               const WriteRequest& request,
                               Commit&& commit) {
  PhraseCore::Result invalid{};
  invalid.slot = request.sourceSlot;
  if (request.destinationSongSlot > 1) {
    invalid.error = PhraseCore::Error::InvalidSongSlot;
    return invalid;
  }

  GroovePuterUndo::SongUndoPayload before{};
  before.songSlot = request.destinationSongSlot;
  before.before = scene.songs[request.destinationSongSlot];
  Song after = before.before;

  PhraseCore::Result result{};
  if (request.overwrite) {
    result = PhraseCore::writeToSong(
        scene.phraseBank, request.sourceSlot, after, request.startRow, true);
  } else {
    result = PhraseCore::insertIntoSong(
        scene.phraseBank, request.sourceSlot, after, request.startRow);
  }
  if (!result) return result;
  if (GroovePuterUndo::songsEqual(before.before, after)) return result;

  auto&& guardedCommit = commit;
  const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
      UndoKind::Song, before, [&]() {
        guardedCommit([&]() { scene.songs[request.destinationSongSlot] = after; });
      });
  if (!committed) {
    result = PhraseCore::Result{};
    result.slot = request.sourceSlot;
    result.error = PhraseCore::Error::InvalidPhrase;
  }
  return result;
}

}  // namespace PhraseWorkspace
