#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "src/phrase/phrase_core.h"
#include "src/phrase/phrase_persistence.h"

namespace {

Song makeSong(int length, int base) {
  Song song{};
  song.length = length;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
  for (int row = 0; row < length; ++row) {
    song.positions[row].patterns[0] = static_cast<int16_t>(base + row);
    song.positions[row].patterns[1] = static_cast<int16_t>(base + 32 + row);
    song.positions[row].patterns[2] = static_cast<int16_t>(base + 64 + row);
  }
  return song;
}

Song emptySong() {
  Song song{};
  song.length = 1;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
  return song;
}

void captureFourRoles(PhraseCore::PhraseBank& bank) {
  Song source = makeSong(16, 1);
  assert(PhraseCore::captureSongRegion(
      bank, PhraseCore::SlotId::A, source, 0, 0, 4,
      PhraseCore::Role::Main, PhraseCore::Source::InternalPattern));
  assert(PhraseCore::captureSongRegion(
      bank, PhraseCore::SlotId::B, source, 0, 4, 2,
      PhraseCore::Role::Variation, PhraseCore::Source::InternalPattern));
  assert(PhraseCore::captureSongRegion(
      bank, PhraseCore::SlotId::C, source, 0, 6, 1,
      PhraseCore::Role::Break, PhraseCore::Source::InternalPattern));
  assert(PhraseCore::captureSongRegion(
      bank, PhraseCore::SlotId::D, source, 0, 7, 2,
      PhraseCore::Role::Ending, PhraseCore::Source::InternalPattern));
}

void testAssignReplaceRemoveAndTotal() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  captureFourRoles(bank);

  const PhraseCore::SlotId chain[] = {
      PhraseCore::SlotId::A, PhraseCore::SlotId::A,
      PhraseCore::SlotId::B, PhraseCore::SlotId::A,
      PhraseCore::SlotId::C, PhraseCore::SlotId::A,
      PhraseCore::SlotId::B, PhraseCore::SlotId::D,
  };
  for (uint8_t position = 0; position < sizeof(chain) / sizeof(chain[0]);
       ++position) {
    const auto result = PhraseCore::assignArrangementStep(
        bank, position, chain[position]);
    assert(result);
    assert(result.changed);
  }
  assert(bank.arrangement.length == 8);
  assert(PhraseCore::arrangementTotalBars(bank) == 23);

  auto same = PhraseCore::assignArrangementStep(
      bank, 2, PhraseCore::SlotId::B);
  assert(same);
  assert(!same.changed);

  auto replace = PhraseCore::assignArrangementStep(
      bank, 2, PhraseCore::SlotId::C);
  assert(replace);
  assert(replace.changed);
  assert(PhraseCore::arrangementTotalBars(bank) == 22);

  auto remove = PhraseCore::removeArrangementStep(bank, 2);
  assert(remove);
  assert(remove.changed);
  assert(bank.arrangement.length == 7);
  assert(bank.arrangement.slots[2] ==
         static_cast<uint8_t>(PhraseCore::SlotId::A));
  assert(bank.arrangement.slots[7] == PhraseCore::kNoSlot);
}

void testInvalidAssignmentsAreAtomic() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  Song source = makeSong(4, 1);
  assert(PhraseCore::captureSongRegion(
      bank, PhraseCore::SlotId::A, source, 0, 0, 4,
      PhraseCore::Role::Main, PhraseCore::Source::InternalPattern));

  const PhraseCore::PhraseBank before = bank;
  auto gap = PhraseCore::assignArrangementStep(
      bank, 1, PhraseCore::SlotId::A);
  assert(!gap);
  assert(gap.error == PhraseCore::Error::InvalidArrangementPosition);
  assert(std::memcmp(&bank, &before, sizeof(bank)) == 0);

  auto empty = PhraseCore::assignArrangementStep(
      bank, 0, PhraseCore::SlotId::B);
  assert(!empty);
  assert(empty.error == PhraseCore::Error::InvalidPhrase);
  assert(std::memcmp(&bank, &before, sizeof(bank)) == 0);
}

void testAtomicArrangementWrite() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  captureFourRoles(bank);
  assert(PhraseCore::assignArrangementStep(
      bank, 0, PhraseCore::SlotId::A));
  assert(PhraseCore::assignArrangementStep(
      bank, 1, PhraseCore::SlotId::B));
  assert(PhraseCore::assignArrangementStep(
      bank, 2, PhraseCore::SlotId::C));
  assert(PhraseCore::assignArrangementStep(
      bank, 3, PhraseCore::SlotId::D));

  Song destination = emptySong();
  auto write = PhraseCore::writeArrangementToSong(
      bank, destination, 10, false);
  assert(write);
  assert(write.totalBars == 9);
  assert(destination.length == 19);
  assert(destination.positions[10].patterns[0] == 1);
  assert(destination.positions[13].patterns[2] == 68);
  assert(destination.positions[14].patterns[0] == 5);
  assert(destination.positions[16].patterns[0] == 7);
  assert(destination.positions[17].patterns[0] == 8);
  assert(destination.positions[18].patterns[2] == 72);

  const Song beforeOccupied = destination;
  auto occupied = PhraseCore::writeArrangementToSong(
      bank, destination, 10, false);
  assert(!occupied);
  assert(occupied.error == PhraseCore::Error::DestinationOccupied);
  assert(std::memcmp(&destination, &beforeOccupied, sizeof(destination)) == 0);

  auto overwrite = PhraseCore::writeArrangementToSong(
      bank, destination, 10, true);
  assert(overwrite);
  assert(overwrite.totalBars == 9);

  const Song beforeRange = destination;
  auto range = PhraseCore::writeArrangementToSong(
      bank, destination, 124, true);
  assert(!range);
  assert(range.error == PhraseCore::Error::RegionOutOfRange);
  assert(std::memcmp(&destination, &beforeRange, sizeof(destination)) == 0);
}

void testClearPhraseCompactsArrangement() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  captureFourRoles(bank);
  assert(PhraseCore::assignArrangementStep(
      bank, 0, PhraseCore::SlotId::A));
  assert(PhraseCore::assignArrangementStep(
      bank, 1, PhraseCore::SlotId::B));
  assert(PhraseCore::assignArrangementStep(
      bank, 2, PhraseCore::SlotId::A));
  assert(PhraseCore::assignArrangementStep(
      bank, 3, PhraseCore::SlotId::D));

  assert(PhraseCore::clear(bank, PhraseCore::SlotId::A));
  assert(bank.arrangement.length == 2);
  assert(bank.arrangement.slots[0] ==
         static_cast<uint8_t>(PhraseCore::SlotId::B));
  assert(bank.arrangement.slots[1] ==
         static_cast<uint8_t>(PhraseCore::SlotId::D));
  assert(bank.arrangement.slots[2] == PhraseCore::kNoSlot);
}

void testArrangementPersistenceAndLegacyDecode() {
  PhraseCore::PhraseBank source{};
  PhraseCore::reset(source);
  captureFourRoles(source);
  assert(PhraseCore::assignArrangementStep(
      source, 0, PhraseCore::SlotId::A));
  assert(PhraseCore::assignArrangementStep(
      source, 1, PhraseCore::SlotId::C));
  assert(PhraseCore::assignArrangementStep(
      source, 2, PhraseCore::SlotId::D));

  int32_t values[PhraseCore::kPersistValueCount]{};
  for (int index = 0; index < PhraseCore::kPersistValueCount; ++index) {
    values[index] = PhraseCore::persistentValueAt(source, index);
  }

  PhraseCore::PhraseBank decoded{};
  PhraseCore::beginPersistentDecode(decoded);
  for (int index = 0; index < PhraseCore::kPersistValueCount; ++index) {
    assert(PhraseCore::applyPersistentValue(decoded, index, values[index]));
  }
  assert(!PhraseCore::sanitize(decoded));
  assert(std::memcmp(&source, &decoded, sizeof(source)) == 0);

  PhraseCore::PhraseBank legacy{};
  PhraseCore::beginPersistentDecode(legacy);
  for (int index = 0; index < PhraseCore::kPersistLegacyValueCount; ++index) {
    assert(PhraseCore::applyPersistentValue(legacy, index, values[index]));
  }
  assert(!PhraseCore::sanitize(legacy));
  assert(legacy.arrangement.length == 0);
  for (uint8_t slot : legacy.arrangement.slots) {
    assert(slot == PhraseCore::kNoSlot);
  }
}

void testSanitizeDropsDanglingEntries() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  captureFourRoles(bank);
  bank.arrangement.length = 4;
  bank.arrangement.slots[0] = static_cast<uint8_t>(PhraseCore::SlotId::A);
  bank.arrangement.slots[1] = 99;
  bank.arrangement.slots[2] = static_cast<uint8_t>(PhraseCore::SlotId::B);
  bank.arrangement.slots[3] = PhraseCore::kNoSlot;

  assert(PhraseCore::sanitize(bank));
  assert(bank.arrangement.length == 2);
  assert(bank.arrangement.slots[0] ==
         static_cast<uint8_t>(PhraseCore::SlotId::A));
  assert(bank.arrangement.slots[1] ==
         static_cast<uint8_t>(PhraseCore::SlotId::B));
  assert(bank.arrangement.slots[2] == PhraseCore::kNoSlot);
}

}  // namespace

int main() {
  testAssignReplaceRemoveAndTotal();
  testInvalidAssignmentsAreAtomic();
  testAtomicArrangementWrite();
  testClearPhraseCompactsArrangement();
  testArrangementPersistenceAndLegacyDecode();
  testSanitizeDropsDanglingEntries();
  std::cout << "Phrase Arranger tests passed\n";
  return 0;
}
