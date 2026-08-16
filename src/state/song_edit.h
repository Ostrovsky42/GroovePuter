#pragma once

#include <algorithm>

#include "../../scenes.h"

namespace GroovePuterUndo {
namespace SongEdit {

inline int trackIndex(SongTrack track) {
  switch (track) {
    case SongTrack::SynthA: return 0;
    case SongTrack::SynthB: return 1;
    case SongTrack::Drums: return 2;
    case SongTrack::Voice: return 3;
  }
  return -1;
}

inline int patternAt(const Song& song, int row, SongTrack track) {
  const int ti = trackIndex(track);
  if (row < 0 || row >= Song::kMaxPositions || ti < 0 ||
      ti >= SongPosition::kTrackCount) {
    return -1;
  }
  return song.positions[row].patterns[ti];
}

inline void setPattern(Song& song, int row, SongTrack track, int pattern) {
  const int ti = trackIndex(track);
  if (row < 0 || row >= Song::kMaxPositions || ti < 0 ||
      ti >= SongPosition::kTrackCount) {
    return;
  }
  if (pattern < 0) {
    song.positions[row].patterns[ti] = -1;
    return;
  }
  song.positions[row].patterns[ti] =
      static_cast<int16_t>(clampSongPatternIndex(pattern));
  if (row >= song.length) song.length = row + 1;
  if (song.length > Song::kMaxPositions) song.length = Song::kMaxPositions;
}

inline void clearPattern(Song& song, int row, SongTrack track) {
  setPattern(song, row, track, -1);
}

inline int flipBankReference(int pattern) {
  if (pattern < 0) return pattern;
  const int bank = songPatternBank(pattern);
  const int slot = songPatternIndexInBank(pattern);
  const int page = songPatternPage(pattern);
  if (bank < 0 || bank >= kBankCount || slot < 0 ||
      slot >= Bank<SynthPattern>::kPatterns || page < 0 || page >= kMaxPages) {
    return pattern;
  }
  return songPatternFromPageBankIndex(page, bank == 0 ? 1 : 0, slot);
}

inline void insertRow(Song& song, int position) {
  if (position < 0) position = 0;
  if (position >= Song::kMaxPositions) return;
  const int len = song.length < 1 ? 1 :
      (song.length > Song::kMaxPositions ? Song::kMaxPositions : song.length);
  const int lastRow = std::min(len, Song::kMaxPositions - 1);
  for (int row = lastRow; row > position; --row) {
    song.positions[row] = song.positions[row - 1];
  }
  for (int track = 0; track < SongPosition::kTrackCount; ++track) {
    song.positions[position].patterns[track] = -1;
  }
  if (len < Song::kMaxPositions) song.length = len + 1;
}

inline void deleteRow(Song& song, int position) {
  if (position < 0) return;
  const int len = song.length < 1 ? 1 :
      (song.length > Song::kMaxPositions ? Song::kMaxPositions : song.length);
  if (position >= len) return;
  for (int row = position; row < len - 1; ++row) {
    song.positions[row] = song.positions[row + 1];
  }
  const int last = len - 1;
  for (int track = 0; track < SongPosition::kTrackCount; ++track) {
    song.positions[last].patterns[track] = -1;
  }
  if (len > 1) song.length = len - 1;
}

inline void reset(Song& song) {
  song = Song{};
}

}  // namespace SongEdit
}  // namespace GroovePuterUndo
