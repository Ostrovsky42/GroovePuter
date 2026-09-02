#pragma once

#include <cstdint>

#include "phrase_song_insert.h"

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
  const PhraseCore::PhraseSlot* phrase = PhraseCore::slotAt(scene.phraseBank, slot);
  return phrase && PhraseCore::buildBarPreview(
      *phrase, bar, scene, currentPageIndex, output);
}

// R4 PREPARE helpers mutate only caller-owned detached values. They own no
// retained history, revision publication, audio exclusion, filesystem or
// activation boundary.
inline PhraseCore::Result capturePrepared(
    const Scene& sourceScene,
    const CaptureRequest& request,
    PhraseCore::PhraseBank& preparedBank) {
  PhraseCore::Result result{};
  result.slot = request.targetSlot;
  if (request.sourceSongSlot > 1) {
    result.error = PhraseCore::Error::InvalidSongSlot;
    return result;
  }
  return PhraseCore::captureSongRegion(
      preparedBank,
      request.targetSlot,
      sourceScene.songs[request.sourceSongSlot],
      request.sourceSongSlot,
      request.startRow,
      request.lengthBars,
      request.role,
      request.source,
      request.trackMask);
}

inline PhraseCore::Result derivePrepared(
    const DeriveRequest& request,
    PhraseCore::PhraseBank& preparedBank) {
  return PhraseCore::deriveReferenceView(
      preparedBank, request.targetSlot, request.parentSlot, request.role);
}

inline PhraseCore::Result clearPrepared(
    PhraseCore::SlotId slot,
    PhraseCore::PhraseBank& preparedBank) {
  if (!PhraseCore::summarize(preparedBank, slot).valid) {
    PhraseCore::Result result{};
    result.slot = slot;
    result.error = PhraseCore::Error::InvalidPhrase;
    return result;
  }
  return PhraseCore::clear(preparedBank, slot);
}

inline PhraseCore::Result writeToSongPrepared(
    const PhraseCore::PhraseBank& phraseBank,
    const WriteRequest& request,
    Song& preparedSong) {
  PhraseCore::Result result{};
  result.slot = request.sourceSlot;
  if (request.destinationSongSlot > 1) {
    result.error = PhraseCore::Error::InvalidSongSlot;
    return result;
  }
  if (request.overwrite) {
    return PhraseCore::writeToSong(
        phraseBank, request.sourceSlot, preparedSong, request.startRow, true);
  }
  return PhraseCore::insertIntoSong(
      phraseBank, request.sourceSlot, preparedSong, request.startRow);
}

}  // namespace PhraseWorkspace
