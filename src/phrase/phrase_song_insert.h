#pragma once

#include "phrase_core.h"

namespace PhraseCore {

inline bool songPositionIsEmpty(const SongPosition& position) {
  for (int track = 0; track < SongPosition::kTrackCount; ++track) {
    if (position.patterns[track] >= 0) return false;
  }
  return true;
}

inline void clearSongPosition(SongPosition& position) {
  for (int track = 0; track < SongPosition::kTrackCount; ++track) {
    position.patterns[track] = -1;
  }
}

// Song keeps a one-row empty placeholder for an otherwise empty arrangement.
// Treat that placeholder as logical length zero for insertion so the first
// inserted Phrase starts at row 0 instead of leaving a synthetic blank row.
inline int logicalSongLengthForInsert(const Song& song) {
  int length = song.length;
  if (length < 1) length = 1;
  if (length > Song::kMaxPositions) length = Song::kMaxPositions;
  if (length == 1 && songPositionIsEmpty(song.positions[0])) return 0;
  return length;
}

// W has one predictable destination contract:
// - inside the logical Song: insert before startRow and shift complete rows;
// - at/after the logical end: materialize exactly at startRow, leaving an
//   explicitly empty gap when the user selected a sparse destination.
// The complete SongPosition is shifted so non-Phrase lanes (for example Voice)
// are preserved. All range/capacity checks happen before the first mutation.
inline Result insertIntoSong(const PhraseBank& bank,
                             SlotId sourceSlot,
                             Song& destination,
                             uint8_t startRow) {
  Result result{};
  result.slot = sourceSlot;

  const PhraseSlot* phrase = slotAt(bank, sourceSlot);
  if (!phrase || !isValid(*phrase)) {
    result.error = Error::InvalidPhrase;
    return result;
  }

  const int phraseBars = phrase->metadata.lengthBars;
  const int logicalLength = logicalSongLengthForInsert(destination);
  const int insertRow = static_cast<int>(startRow);
  const bool shiftsRows = insertRow < logicalLength;
  const int finalLength = shiftsRows
      ? logicalLength + phraseBars
      : insertRow + phraseBars;
  if (insertRow < 0 || insertRow >= Song::kMaxPositions ||
      finalLength > Song::kMaxPositions) {
    result.error = Error::RegionOutOfRange;
    return result;
  }

  if (shiftsRows) {
    for (int row = logicalLength - 1; row >= insertRow; --row) {
      destination.positions[row + phraseBars] = destination.positions[row];
    }
  } else {
    for (int row = logicalLength; row < insertRow; ++row) {
      clearSongPosition(destination.positions[row]);
    }
  }

  for (int bar = 0; bar < phraseBars; ++bar) {
    SongPosition& position = destination.positions[insertRow + bar];
    clearSongPosition(position);
    for (int track = 0; track < kTrackCount; ++track) {
      if ((phrase->metadata.trackMask & maskForTrackIndex(track)) == 0) {
        continue;
      }
      position.patterns[track] = phrase->patternRefs[bar][track];
    }
  }

  destination.length = finalLength > 0 ? finalLength : 1;
  result.phraseId = phrase->metadata.phraseId;
  return result;
}

}  // namespace PhraseCore
