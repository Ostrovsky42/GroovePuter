#include <cassert>
#include <cstdio>

#include "src/state/song_edit.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

namespace {
using namespace GroovePuterUndo;
using namespace GroovePuterUndo::SongEdit;

Song makeSong() {
  Song song{};
  song.length = 1;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
  song.reverse = false;
  return song;
}

void testSetClearAndNoopEquality() {
  Song song = makeSong();
  const Song original = song;
  assert(sameSong(song, original));

  setPattern(song, 3, SongTrack::SynthA, 7);
  assert(patternAt(song, 3, SongTrack::SynthA) == 7);
  assert(song.length == 4);
  assert(!sameSong(song, original));

  clearPattern(song, 3, SongTrack::SynthA);
  assert(patternAt(song, 3, SongTrack::SynthA) == -1);
  // Match SceneManager::clearSongPattern: clearing never trims Song length.
  assert(song.length == 4);
}

void testInsertDeletePreserveRows() {
  Song song = makeSong();
  setPattern(song, 0, SongTrack::SynthA, 1);
  setPattern(song, 1, SongTrack::SynthA, 2);
  setPattern(song, 2, SongTrack::SynthA, 3);
  assert(song.length == 3);

  insertRow(song, 1);
  assert(song.length == 4);
  assert(patternAt(song, 0, SongTrack::SynthA) == 1);
  assert(patternAt(song, 1, SongTrack::SynthA) == -1);
  assert(patternAt(song, 2, SongTrack::SynthA) == 2);
  assert(patternAt(song, 3, SongTrack::SynthA) == 3);

  deleteRow(song, 1);
  assert(song.length == 3);
  assert(patternAt(song, 0, SongTrack::SynthA) == 1);
  assert(patternAt(song, 1, SongTrack::SynthA) == 2);
  assert(patternAt(song, 2, SongTrack::SynthA) == 3);
}

void testBankFlipPreservesPageAndSlot() {
  const int original = songPatternFromPageBankIndex(2, 0, 5);
  const int flipped = flipBankReference(original);
  assert(songPatternPage(flipped) == 2);
  assert(songPatternBank(flipped) == 1);
  assert(songPatternIndexInBank(flipped) == 5);
  assert(flipBankReference(flipped) == original);
  assert(flipBankReference(-1) == -1);
}

void testResetAndReceiptBudgets() {
  Song song = makeSong();
  setPattern(song, 7, SongTrack::Drums, 4);
  song.reverse = true;
  reset(song);
  Song canonical{};
  assert(sameSong(song, canonical));

  static_assert(sizeof(SongUndoPayload) <= 1040,
                "Song receipt exceeds R4 budget");
  static_assert(sizeof(PhraseUndoPayload) <= 248,
                "Phrase receipt exceeds R4 budget");
  static_assert(sizeof(SongUndoPayload) <= kUndoPayloadBytes,
                "Song receipt no longer fits canonical UndoOwner");
  static_assert(sizeof(PhraseUndoPayload) <= kUndoPayloadBytes,
                "Phrase receipt no longer fits canonical UndoOwner");
}

void testPhraseBankEquality() {
  PhraseCore::PhraseBank a{};
  PhraseCore::PhraseBank b = a;
  assert(samePhraseBank(a, b));
  ++b.nextPhraseId;
  assert(!samePhraseBank(a, b));
}

}  // namespace

int main() {
  testSetClearAndNoopEquality();
  testInsertDeletePreserveRows();
  testBankFlipPreservesPageAndSlot();
  testResetAndReceiptBudgets();
  testPhraseBankEquality();
  std::puts("0.9.8 R4 prepared Song/Phrase semantics: PASS");
  return 0;
}
