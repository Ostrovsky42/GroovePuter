// Per-cell source override follows its cell through row operations and Undo.
#include <cassert>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/phrase/phrase_song_insert.h"
#include "src/state/song_edit.h"
#include "src/state/undo_receipts.h"

SerialMock Serial;
SDMock SD;

using GroovePuterUndo::SongEdit::deleteRow;
using GroovePuterUndo::SongEdit::insertRow;
using GroovePuterUndo::SongEdit::setPattern;

static Song makeSong() {
  Song song{};
  song.length = 4;
  for (int r = 0; r < 4; ++r) {
    setPattern(song, r, SongTrack::SynthA, r);
    setPattern(song, r, SongTrack::SynthB, r);
  }
  song.setStepsOverride(0, 2, true);
  song.setStepsOverride(1, 3, true);
  return song;
}

int main() {
  // SongEdit insert / delete move the bit with its cell.
  {
    Song song = makeSong();
    insertRow(song, 1);
    assert(song.length == 5);
    assert(!song.stepsOverrideAt(0, 2) && song.stepsOverrideAt(0, 3));
    assert(!song.stepsOverrideAt(1, 3) && song.stepsOverrideAt(1, 4));
    assert(!song.stepsOverrideAt(0, 1) && !song.stepsOverrideAt(1, 1));
    deleteRow(song, 1);
    assert(song.stepsOverrideAt(0, 2) && song.stepsOverrideAt(1, 3));
    assert(!song.stepsOverrideAt(0, 3) && !song.stepsOverrideAt(1, 4));
    deleteRow(song, 2);  // delete the overridden A cell's row: its bit goes with it
    assert(!song.stepsOverrideAt(0, 2) && song.stepsOverrideAt(1, 2));
  }
  // SceneManager insert / delete.
  {
    SceneManager scenes;
    scenes.loadDefaultScene();
    Song& song = scenes.currentScene().songs[scenes.activeSongSlot()];
    song = makeSong();
    scenes.insertSongRow(0);
    assert(song.stepsOverrideAt(0, 3) && song.stepsOverrideAt(1, 4) && !song.stepsOverrideAt(0, 2));
    scenes.deleteSongRow(0);
    assert(song.stepsOverrideAt(0, 2) && song.stepsOverrideAt(1, 3));
  }
  // Writing a different slot clears the cell's bit; the same slot keeps it.
  {
    Song song = makeSong();
    setPattern(song, 2, SongTrack::SynthA, 2);
    assert(song.stepsOverrideAt(0, 2));
    setPattern(song, 2, SongTrack::SynthA, 5);
    assert(!song.stepsOverrideAt(0, 2));
    song.setStepsOverride(1, 3, true);
    GroovePuterUndo::SongEdit::clearPattern(song, 3, SongTrack::SynthB);
    assert(!song.stepsOverrideAt(1, 3));
  }
  // Whole-Song paste carries every bit with its cells.
  {
    const Song copied = makeSong();
    Song target{};
    target = copied;
    assert(target.stepsOverrideAt(0, 2) && target.stepsOverrideAt(1, 3));
  }
  // Undo restores the bit; a bit-only change is a real Song change.
  {
    SceneManager scenes;
    scenes.loadDefaultScene();
    Song& song = scenes.currentScene().songs[scenes.activeSongSlot()];
    song = makeSong();
    GroovePuterUndo::SongUndoPayload receipt{};
    assert(GroovePuterUndo::captureCurrentSongUndo(scenes, receipt));
    Song after = song;
    after.setStepsOverride(0, 2, false);
    assert(!GroovePuterUndo::sameSong(receipt.before, after));
    song = after;
    assert(!song.stepsOverrideAt(0, 2));
    GroovePuterUndo::restoreSongUndo(scenes, receipt);
    assert(song.stepsOverrideAt(0, 2) && song.stepsOverrideAt(1, 3));
    assert(GroovePuterUndo::sameSong(song, receipt.before));
  }
  std::puts("test_song_cell_source_row_ops PASS");
  return 0;
}
