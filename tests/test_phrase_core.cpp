#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "src/phrase/phrase_song_insert.h"

namespace {

Song makeSong(int length) {
  Song song{};
  song.length = length;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
  for (int row = 0; row < length; ++row) {
    song.positions[row].patterns[0] = static_cast<int16_t>(row + 1);
    song.positions[row].patterns[1] = static_cast<int16_t>(row + 33);
    song.positions[row].patterns[2] = static_cast<int16_t>(row + 65);
  }
  return song;
}

void testMemoryContract() {
  static_assert(sizeof(PhraseCore::PhraseMetadata) <= 16,
                "metadata budget regression");
  static_assert(sizeof(PhraseCore::PhraseBank) == 244,
                "Phrase bank budget regression");
  assert(PhraseCore::kSlotCount == 4);
  assert(PhraseCore::kMaxBars == 8);
}

void testResetAndSummaries() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  assert(bank.nextPhraseId == 1);
  assert(bank.version == PhraseCore::kPersistenceVersion);
  for (int slot = 0; slot < PhraseCore::kSlotCount; ++slot) {
    const auto id = static_cast<PhraseCore::SlotId>(slot);
    const PhraseCore::SlotSummary summary = PhraseCore::summarize(bank, id);
    assert(!summary.valid);
    for (int bar = 0; bar < PhraseCore::kMaxBars; ++bar) {
      for (int track = 0; track < PhraseCore::kTrackCount; ++track) {
        assert(bank.slots[slot].patternRefs[bar][track] == -1);
      }
    }
  }
}

void testCaptureFourBarReferenceView() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song song = makeSong(8);

  const PhraseCore::Result result = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      song,
      0,
      2,
      4,
      PhraseCore::Role::Main,
      PhraseCore::Source::Generated);
  assert(result);
  assert(result.phraseId == 1);
  assert(bank.nextPhraseId == 2);

  const PhraseCore::SlotSummary summary =
      PhraseCore::summarize(bank, PhraseCore::SlotId::A);
  assert(summary.valid);
  assert(summary.lengthBars == 4);
  assert(summary.role == PhraseCore::Role::Main);
  assert(summary.source == PhraseCore::Source::Generated);
  assert(summary.storage == PhraseCore::StorageMode::ReferenceView);
  assert(summary.mutableBacking);
  assert(summary.trackMask == PhraseCore::kAllTracks);
  assert(PhraseCore::patternAt(bank.slots[0], 0, SongTrack::SynthA) == 3);
  assert(PhraseCore::patternAt(bank.slots[0], 3, SongTrack::SynthB) == 38);
  assert(PhraseCore::patternAt(bank.slots[0], 3, SongTrack::Drums) == 70);

  song.positions[2].patterns[0] = 200;
  assert(PhraseCore::patternAt(bank.slots[0], 0, SongTrack::SynthA) == 3);
}

void testTrackMaskAndSparseMaterial() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song song = makeSong(4);

  const PhraseCore::Result result = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::B,
      song,
      1,
      0,
      2,
      PhraseCore::Role::Break,
      PhraseCore::Source::InternalPattern,
      PhraseCore::kTrackDrums);
  assert(result);
  const PhraseCore::PhraseSlot& phrase = bank.slots[1];
  assert(PhraseCore::patternAt(phrase, 0, SongTrack::SynthA) == -1);
  assert(PhraseCore::patternAt(phrase, 0, SongTrack::SynthB) == -1);
  assert(PhraseCore::patternAt(phrase, 0, SongTrack::Drums) == 65);
}

void testInvalidCaptureIsAtomic() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song song = makeSong(4);
  const PhraseCore::PhraseBank before = bank;

  const PhraseCore::Result badLength = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      song,
      0,
      0,
      3,
      PhraseCore::Role::Main,
      PhraseCore::Source::Generated);
  assert(!badLength);
  assert(badLength.error == PhraseCore::Error::InvalidLength);
  assert(std::memcmp(&bank, &before, sizeof(bank)) == 0);

  const PhraseCore::Result badRange = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      song,
      0,
      3,
      2,
      PhraseCore::Role::Main,
      PhraseCore::Source::Generated);
  assert(!badRange);
  assert(badRange.error == PhraseCore::Error::RegionOutOfRange);
  assert(std::memcmp(&bank, &before, sizeof(bank)) == 0);

  const PhraseCore::Result badSource = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      song,
      0,
      0,
      1,
      PhraseCore::Role::Main,
      PhraseCore::Source::SmfRegion);
  assert(!badSource);
  assert(badSource.error == PhraseCore::Error::InvalidSource);
  assert(std::memcmp(&bank, &before, sizeof(bank)) == 0);

  const PhraseCore::Result badRole = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      song,
      0,
      0,
      1,
      static_cast<PhraseCore::Role>(99),
      PhraseCore::Source::Generated);
  assert(!badRole);
  assert(badRole.error == PhraseCore::Error::InvalidRole);
  assert(std::memcmp(&bank, &before, sizeof(bank)) == 0);

  Song empty = makeSong(1);
  for (int track = 0; track < SongPosition::kTrackCount; ++track) {
    empty.positions[0].patterns[track] = -1;
  }
  const PhraseCore::Result emptyResult = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      empty,
      0,
      0,
      1,
      PhraseCore::Role::Main,
      PhraseCore::Source::Generated);
  assert(!emptyResult);
  assert(emptyResult.error == PhraseCore::Error::EmptyRegion);
  assert(std::memcmp(&bank, &before, sizeof(bank)) == 0);
}

void testDerivedParentIdentity() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song song = makeSong(4);
  const PhraseCore::Result captured = PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      song,
      0,
      0,
      4,
      PhraseCore::Role::Main,
      PhraseCore::Source::Generated);
  assert(captured);

  const PhraseCore::Result derived = PhraseCore::deriveReferenceView(
      bank,
      PhraseCore::SlotId::B,
      PhraseCore::SlotId::A,
      PhraseCore::Role::Variation);
  assert(derived);
  assert(derived.phraseId == 2);
  assert(bank.slots[1].metadata.parentId == captured.phraseId);
  assert(bank.slots[1].metadata.source == PhraseCore::Source::Derived);
  assert(bank.slots[1].metadata.role == PhraseCore::Role::Variation);
  assert(std::memcmp(bank.slots[0].patternRefs,
                     bank.slots[1].patternRefs,
                     sizeof(bank.slots[0].patternRefs)) == 0);
}

void testWriteToSongIsPrevalidated() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song source = makeSong(4);
  assert(PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::C,
      source,
      0,
      0,
      4,
      PhraseCore::Role::Ending,
      PhraseCore::Source::InternalPattern));

  Song destination{};
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      destination.positions[row].patterns[track] = -1;
    }
  }
  destination.length = 1;

  const PhraseCore::Result write = PhraseCore::writeToSong(
      bank, PhraseCore::SlotId::C, destination, 8, false);
  assert(write);
  assert(destination.length == 12);
  assert(destination.positions[8].patterns[0] == 1);
  assert(destination.positions[11].patterns[2] == 68);

  const Song beforeOccupiedWrite = destination;
  const PhraseCore::Result occupied = PhraseCore::writeToSong(
      bank, PhraseCore::SlotId::C, destination, 8, false);
  assert(!occupied);
  assert(occupied.error == PhraseCore::Error::DestinationOccupied);
  assert(std::memcmp(&destination,
                     &beforeOccupiedWrite,
                     sizeof(destination)) == 0);

  const PhraseCore::Result overwrite = PhraseCore::writeToSong(
      bank, PhraseCore::SlotId::C, destination, 8, true);
  assert(overwrite);
}

void testInsertShiftsWholeSongRows() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song source = makeSong(2);
  assert(PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      source,
      0,
      0,
      2,
      PhraseCore::Role::Main,
      PhraseCore::Source::InternalPattern));

  Song destination = makeSong(4);
  for (int row = 0; row < 4; ++row) {
    destination.positions[row].patterns[3] = static_cast<int16_t>(100 + row);
  }

  const PhraseCore::Result inserted = PhraseCore::insertIntoSong(
      bank, PhraseCore::SlotId::A, destination, 1);
  assert(inserted);
  assert(destination.length == 6);

  assert(destination.positions[0].patterns[0] == 1);
  assert(destination.positions[0].patterns[3] == 100);

  assert(destination.positions[1].patterns[0] == 1);
  assert(destination.positions[1].patterns[1] == 33);
  assert(destination.positions[1].patterns[2] == 65);
  assert(destination.positions[1].patterns[3] == -1);
  assert(destination.positions[2].patterns[0] == 2);
  assert(destination.positions[2].patterns[1] == 34);
  assert(destination.positions[2].patterns[2] == 66);
  assert(destination.positions[2].patterns[3] == -1);

  assert(destination.positions[3].patterns[0] == 2);
  assert(destination.positions[3].patterns[1] == 34);
  assert(destination.positions[3].patterns[2] == 66);
  assert(destination.positions[3].patterns[3] == 101);
  assert(destination.positions[5].patterns[0] == 4);
  assert(destination.positions[5].patterns[3] == 103);
}

void testInsertHandlesEmptyPlaceholderAppendAndSparseTarget() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song source = makeSong(2);
  assert(PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::B,
      source,
      0,
      0,
      2,
      PhraseCore::Role::Variation,
      PhraseCore::Source::InternalPattern));

  Song empty = makeSong(1);
  for (int track = 0; track < SongPosition::kTrackCount; ++track) {
    empty.positions[0].patterns[track] = -1;
  }
  assert(PhraseCore::logicalSongLengthForInsert(empty) == 0);
  assert(PhraseCore::insertIntoSong(
      bank, PhraseCore::SlotId::B, empty, 0));
  assert(empty.length == 2);
  assert(empty.positions[0].patterns[0] == 1);
  assert(empty.positions[1].patterns[2] == 66);

  Song append = makeSong(2);
  assert(PhraseCore::insertIntoSong(
      bank, PhraseCore::SlotId::B, append, 2));
  assert(append.length == 4);
  assert(append.positions[0].patterns[0] == 1);
  assert(append.positions[1].patterns[0] == 2);
  assert(append.positions[2].patterns[0] == 1);
  assert(append.positions[3].patterns[2] == 66);

  Song sparse = makeSong(2);
  assert(PhraseCore::insertIntoSong(
      bank, PhraseCore::SlotId::B, sparse, 5));
  assert(sparse.length == 7);
  assert(sparse.positions[0].patterns[0] == 1);
  assert(sparse.positions[1].patterns[0] == 2);
  for (int row = 2; row < 5; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      assert(sparse.positions[row].patterns[track] == -1);
    }
  }
  assert(sparse.positions[5].patterns[0] == 1);
  assert(sparse.positions[5].patterns[1] == 33);
  assert(sparse.positions[5].patterns[2] == 65);
  assert(sparse.positions[6].patterns[0] == 2);
  assert(sparse.positions[6].patterns[2] == 66);
}

void testInsertCapacityFailureIsAtomic() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song source = makeSong(2);
  assert(PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::D,
      source,
      0,
      0,
      2,
      PhraseCore::Role::Ending,
      PhraseCore::Source::InternalPattern));

  Song destination = makeSong(127);
  const Song before = destination;
  const PhraseCore::Result overflow = PhraseCore::insertIntoSong(
      bank, PhraseCore::SlotId::D, destination, 127);
  assert(!overflow);
  assert(overflow.error == PhraseCore::Error::RegionOutOfRange);
  assert(std::memcmp(&destination, &before, sizeof(destination)) == 0);
}

void testSanitizePersistenceBoundary() {
  PhraseCore::PhraseBank legacy{};
  legacy.version = 0;
  assert(PhraseCore::sanitize(legacy));
  assert(legacy.version == PhraseCore::kPersistenceVersion);
  assert(legacy.nextPhraseId == 1);

  Song song = makeSong(2);
  assert(PhraseCore::captureSongRegion(
      legacy,
      PhraseCore::SlotId::D,
      song,
      0,
      0,
      2,
      PhraseCore::Role::Ending,
      PhraseCore::Source::Generated));
  legacy.slots[3].patternRefs[0][0] =
      static_cast<int16_t>(kMaxGlobalPatterns + 10);
  assert(PhraseCore::sanitize(legacy));
  assert(legacy.slots[3].patternRefs[0][0] == -1);
  assert(PhraseCore::isValid(legacy.slots[3]));
}

}  // namespace

int main() {
  testMemoryContract();
  testResetAndSummaries();
  testCaptureFourBarReferenceView();
  testTrackMaskAndSparseMaterial();
  testInvalidCaptureIsAtomic();
  testDerivedParentIdentity();
  testWriteToSongIsPrevalidated();
  testInsertShiftsWholeSongRows();
  testInsertHandlesEmptyPlaceholderAppendAndSparseTarget();
  testInsertCapacityFailureIsAtomic();
  testSanitizePersistenceBoundary();
  std::cout << "Phrase Core tests passed\n";
  return 0;
}
