#include <cassert>
#include <cstdint>
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

struct GuardCounter {
  int calls = 0;

  template <typename F>
  void operator()(F&& operation) {
    ++calls;
    operation();
  }
};

void resetRevision() {
  GroovePuterState::restoreSceneRevision({0, 0});
}

void testCaptureMarksOnce() {
  Scene scene = makeScene();
  GuardCounter guard{};
  resetRevision();

  PhraseWorkspace::CaptureRequest request{};
  request.targetSlot = PhraseCore::SlotId::A;
  request.sourceSongSlot = 0;
  request.startRow = 1;
  request.lengthBars = 2;
  request.role = PhraseCore::Role::Main;
  request.source = PhraseCore::Source::Generated;

  const PhraseCore::Result result =
      PhraseWorkspace::capture(scene, request, guard);
  assert(result);
  assert(guard.calls == 1);
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 1);
  const PhraseCore::SlotSummary summary =
      PhraseWorkspace::summary(scene, PhraseCore::SlotId::A);
  assert(summary.valid);
  assert(summary.sourceSongSlot == 0);
  assert(summary.sourceStartRow == 1);
}

void testFailedCaptureDoesNotDirty() {
  Scene scene = makeScene();
  GuardCounter guard{};
  resetRevision();

  PhraseWorkspace::CaptureRequest request{};
  request.targetSlot = PhraseCore::SlotId::A;
  request.sourceSongSlot = 0;
  request.startRow = 3;
  request.lengthBars = 4;

  const PhraseCore::Result result =
      PhraseWorkspace::capture(scene, request, guard);
  assert(!result);
  assert(result.error == PhraseCore::Error::RegionOutOfRange);
  assert(guard.calls == 1);
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 0);
  assert(!PhraseWorkspace::summary(scene, PhraseCore::SlotId::A).valid);
}

void testDeriveWriteAndClearEachMarkOnce() {
  Scene scene = makeScene();
  GuardCounter guard{};
  resetRevision();

  PhraseWorkspace::CaptureRequest capture{};
  capture.targetSlot = PhraseCore::SlotId::A;
  capture.sourceSongSlot = 0;
  capture.startRow = 0;
  capture.lengthBars = 2;
  capture.source = PhraseCore::Source::Generated;
  assert(PhraseWorkspace::capture(scene, capture, guard));

  PhraseWorkspace::DeriveRequest derive{};
  derive.targetSlot = PhraseCore::SlotId::B;
  derive.parentSlot = PhraseCore::SlotId::A;
  derive.role = PhraseCore::Role::Variation;
  assert(PhraseWorkspace::derive(scene, derive, guard));
  assert(PhraseWorkspace::summary(scene, PhraseCore::SlotId::B).parentId == 1);

  PhraseWorkspace::WriteRequest write{};
  write.sourceSlot = PhraseCore::SlotId::B;
  write.destinationSongSlot = 1;
  write.startRow = 0;
  write.overwrite = false;
  assert(PhraseWorkspace::writeToSong(scene, write, guard));
  assert(scene.songs[1].length == 2);
  assert(scene.songs[1].positions[1].patterns[2] == 5);

  assert(PhraseWorkspace::clear(scene, PhraseCore::SlotId::B, guard));
  assert(!PhraseWorkspace::summary(scene, PhraseCore::SlotId::B).valid);

  assert(guard.calls == 4);
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 4);
}

void testEmptyClearAndOccupiedWriteStayClean() {
  Scene scene = makeScene();
  GuardCounter guard{};
  resetRevision();

  const PhraseCore::Result emptyClear =
      PhraseWorkspace::clear(scene, PhraseCore::SlotId::D, guard);
  assert(!emptyClear);
  assert(emptyClear.error == PhraseCore::Error::InvalidPhrase);
  assert(guard.calls == 0);
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 0);

  PhraseWorkspace::CaptureRequest capture{};
  capture.targetSlot = PhraseCore::SlotId::A;
  capture.sourceSongSlot = 0;
  capture.startRow = 0;
  capture.lengthBars = 1;
  assert(PhraseWorkspace::capture(scene, capture, guard));
  const uint32_t revisionAfterCapture =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;

  clearSongForTest(scene.songs[1], 1);
  scene.songs[1].positions[0].patterns[0] = 99;
  PhraseWorkspace::WriteRequest write{};
  write.sourceSlot = PhraseCore::SlotId::A;
  write.destinationSongSlot = 1;
  write.startRow = 0;
  write.overwrite = false;
  const PhraseCore::Result occupied =
      PhraseWorkspace::writeToSong(scene, write, guard);
  assert(!occupied);
  assert(occupied.error == PhraseCore::Error::DestinationOccupied);
  assert(scene.songs[1].positions[0].patterns[0] == 99);
  assert(GroovePuterState::sceneRevisionSnapshot().currentRevision ==
         revisionAfterCapture);
}

void testPreviewQueryDoesNotDirty() {
  Scene scene = makeScene();
  GuardCounter guard{};
  resetRevision();

  scene.synthABanks[0].patterns[0].steps[0].note = 48;
  PhraseWorkspace::CaptureRequest capture{};
  capture.targetSlot = PhraseCore::SlotId::A;
  capture.sourceSongSlot = 0;
  capture.startRow = 0;
  capture.lengthBars = 1;
  assert(PhraseWorkspace::capture(scene, capture, guard));
  GroovePuterState::markSceneSaveSucceeded();

  PhraseCore::BarPreview preview{};
  assert(PhraseWorkspace::barPreview(
      scene, 0, PhraseCore::SlotId::A, 0, preview));
  assert((preview.synthAMask & 1u) != 0);
  assert(!GroovePuterState::sceneDirty());
}

}  // namespace

int main() {
  testCaptureMarksOnce();
  testFailedCaptureDoesNotDirty();
  testDeriveWriteAndClearEachMarkOnce();
  testEmptyClearAndOccupiedWriteStayClean();
  testPreviewQueryDoesNotDirty();
  std::cout << "Phrase workspace tests passed\n";
  return 0;
}
