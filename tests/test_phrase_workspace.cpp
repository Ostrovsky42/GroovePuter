#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "src/phrase/phrase_workspace.h"

namespace {

void clearSongForTest(Song& song, int length) {
  song.length = length;
  song.reverse = false;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
}

Scene makeScene() {
  Scene scene{};
  PhraseCore::reset(scene.phraseBank);
  clearSongForTest(scene.songs[0], 4);
  clearSongForTest(scene.songs[1], 1);
  for (int row = 0; row < 4; ++row) {
    scene.songs[0].positions[row].patterns[0] = row;
    scene.songs[0].positions[row].patterns[1] = row + 8;
    scene.songs[0].positions[row].patterns[2] = row + 4;
  }
  return scene;
}

PhraseCore::PhraseBank capturePhraseA(const Scene& scene, uint8_t lengthBars) {
  PhraseCore::PhraseBank prepared = scene.phraseBank;
  PhraseWorkspace::CaptureRequest request{};
  request.targetSlot = PhraseCore::SlotId::A;
  request.sourceSongSlot = 0;
  request.startRow = 0;
  request.lengthBars = lengthBars;
  request.source = PhraseCore::Source::Generated;
  assert(PhraseWorkspace::capturePrepared(scene, request, prepared));
  return prepared;
}

void testCapturePreparedIsDetached() {
  const Scene scene = makeScene();
  const PhraseCore::PhraseBank sourceBefore = scene.phraseBank;
  PhraseCore::PhraseBank prepared = scene.phraseBank;

  PhraseWorkspace::CaptureRequest request{};
  request.targetSlot = PhraseCore::SlotId::A;
  request.sourceSongSlot = 0;
  request.startRow = 0;
  request.lengthBars = 4;
  request.role = PhraseCore::Role::Main;
  request.source = PhraseCore::Source::Generated;

  const PhraseCore::Result result =
      PhraseWorkspace::capturePrepared(scene, request, prepared);
  assert(result);
  assert(std::memcmp(&scene.phraseBank, &sourceBefore, sizeof(sourceBefore)) == 0);
  assert(PhraseCore::summarize(prepared, PhraseCore::SlotId::A).valid);
}

void testFailedCapturePreservesPreparedValue() {
  const Scene scene = makeScene();
  PhraseCore::PhraseBank prepared = scene.phraseBank;
  const PhraseCore::PhraseBank before = prepared;

  PhraseWorkspace::CaptureRequest request{};
  request.targetSlot = PhraseCore::SlotId::A;
  request.sourceSongSlot = 0;
  request.startRow = 3;
  request.lengthBars = 4;

  const PhraseCore::Result result =
      PhraseWorkspace::capturePrepared(scene, request, prepared);
  assert(!result);
  assert(result.error == PhraseCore::Error::RegionOutOfRange);
  assert(std::memcmp(&prepared, &before, sizeof(before)) == 0);
}

void testDeriveWriteAndClearPreparedValues() {
  const Scene scene = makeScene();
  PhraseCore::PhraseBank preparedBank = capturePhraseA(scene, 2);

  PhraseWorkspace::DeriveRequest derive{};
  derive.targetSlot = PhraseCore::SlotId::B;
  derive.parentSlot = PhraseCore::SlotId::A;
  derive.role = PhraseCore::Role::Variation;
  assert(PhraseWorkspace::derivePrepared(derive, preparedBank));
  assert(PhraseCore::summarize(preparedBank, PhraseCore::SlotId::B).parentId == 1);

  const Song residentBefore = scene.songs[1];
  Song preparedSong = residentBefore;
  PhraseWorkspace::WriteRequest write{};
  write.sourceSlot = PhraseCore::SlotId::B;
  write.destinationSongSlot = 1;
  write.startRow = 0;
  write.overwrite = false;
  assert(PhraseWorkspace::writeToSongPrepared(preparedBank, write, preparedSong));
  assert(preparedSong.length == 2);
  assert(preparedSong.positions[1].patterns[2] == 5);
  assert(std::memcmp(&scene.songs[1], &residentBefore, sizeof(residentBefore)) == 0);

  assert(PhraseWorkspace::clearPrepared(PhraseCore::SlotId::B, preparedBank));
  assert(!PhraseCore::summarize(preparedBank, PhraseCore::SlotId::B).valid);
}

void testNormalWriteInsertsOccupiedRow() {
  const Scene scene = makeScene();
  const PhraseCore::PhraseBank preparedBank = capturePhraseA(scene, 1);

  Song preparedSong = scene.songs[1];
  clearSongForTest(preparedSong, 2);
  preparedSong.positions[0].patterns[0] = 99;
  preparedSong.positions[0].patterns[3] = 77;
  preparedSong.positions[1].patterns[0] = 100;
  preparedSong.positions[1].patterns[3] = 78;

  PhraseWorkspace::WriteRequest insert{};
  insert.sourceSlot = PhraseCore::SlotId::A;
  insert.destinationSongSlot = 1;
  insert.startRow = 0;
  insert.overwrite = false;
  assert(PhraseWorkspace::writeToSongPrepared(preparedBank, insert, preparedSong));
  assert(preparedSong.length == 3);
  assert(preparedSong.positions[0].patterns[0] == 0);
  assert(preparedSong.positions[0].patterns[1] == 8);
  assert(preparedSong.positions[0].patterns[2] == 4);
  assert(preparedSong.positions[0].patterns[3] == -1);
  assert(preparedSong.positions[1].patterns[0] == 99);
  assert(preparedSong.positions[1].patterns[3] == 77);
  assert(preparedSong.positions[2].patterns[0] == 100);
  assert(preparedSong.positions[2].patterns[3] == 78);
}

void testReplaceDoesNotShiftRows() {
  const Scene scene = makeScene();
  const PhraseCore::PhraseBank preparedBank = capturePhraseA(scene, 1);

  Song preparedSong = scene.songs[1];
  clearSongForTest(preparedSong, 2);
  preparedSong.positions[0].patterns[0] = 99;
  preparedSong.positions[0].patterns[3] = 77;
  preparedSong.positions[1].patterns[0] = 100;

  PhraseWorkspace::WriteRequest replace{};
  replace.sourceSlot = PhraseCore::SlotId::A;
  replace.destinationSongSlot = 1;
  replace.startRow = 0;
  replace.overwrite = true;
  assert(PhraseWorkspace::writeToSongPrepared(preparedBank, replace, preparedSong));
  assert(preparedSong.length == 2);
  assert(preparedSong.positions[0].patterns[0] == 0);
  assert(preparedSong.positions[0].patterns[3] == 77);
  assert(preparedSong.positions[1].patterns[0] == 100);
}

void testFailedPreparedEditsPreserveInputs() {
  const Scene scene = makeScene();
  PhraseCore::PhraseBank preparedBank = scene.phraseBank;
  const PhraseCore::PhraseBank emptyBefore = preparedBank;
  const PhraseCore::Result emptyClear =
      PhraseWorkspace::clearPrepared(PhraseCore::SlotId::D, preparedBank);
  assert(!emptyClear);
  assert(emptyClear.error == PhraseCore::Error::InvalidPhrase);
  assert(std::memcmp(&preparedBank, &emptyBefore, sizeof(emptyBefore)) == 0);

  preparedBank = capturePhraseA(scene, 1);
  Song preparedSong = scene.songs[1];
  clearSongForTest(preparedSong, Song::kMaxPositions);
  const Song songBefore = preparedSong;

  PhraseWorkspace::WriteRequest write{};
  write.sourceSlot = PhraseCore::SlotId::A;
  write.destinationSongSlot = 1;
  write.startRow = Song::kMaxPositions - 1;
  write.overwrite = false;
  const PhraseCore::Result range =
      PhraseWorkspace::writeToSongPrepared(preparedBank, write, preparedSong);
  assert(!range);
  assert(range.error == PhraseCore::Error::RegionOutOfRange);
  assert(std::memcmp(&preparedSong, &songBefore, sizeof(songBefore)) == 0);
}

void testPreviewQueryUsesCommittedValueWithoutMutation() {
  Scene scene = makeScene();
  scene.synthABanks[0].patterns[0].steps[0].note = 48;
  scene.phraseBank = capturePhraseA(scene, 1);
  const Scene before = scene;

  PhraseCore::BarPreview preview{};
  assert(PhraseWorkspace::barPreview(
      scene, 0, PhraseCore::SlotId::A, 0, preview));
  assert((preview.synthAMask & 1u) != 0);
  assert(std::memcmp(&scene, &before, sizeof(Scene)) == 0);
}

}  // namespace

int main() {
  testCapturePreparedIsDetached();
  testFailedCapturePreservesPreparedValue();
  testDeriveWriteAndClearPreparedValues();
  testNormalWriteInsertsOccupiedRow();
  testReplaceDoesNotShiftRows();
  testFailedPreparedEditsPreserveInputs();
  testPreviewQueryUsesCommittedValueWithoutMutation();
  std::cout << "Phrase workspace PREPARE tests passed\n";
  return 0;
}
