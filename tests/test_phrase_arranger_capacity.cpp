#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "src/phrase/phrase_core.h"

namespace {

Song makeSourceSong() {
  Song song{};
  song.length = 8;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
  for (int row = 0; row < 8; ++row) {
    song.positions[row].patterns[0] = static_cast<int16_t>(row + 1);
    song.positions[row].patterns[1] = static_cast<int16_t>(row + 33);
    song.positions[row].patterns[2] = static_cast<int16_t>(row + 65);
  }
  return song;
}

Song makeEmptySong() {
  Song song{};
  song.length = 1;
  song.reverse = false;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
  return song;
}

void captureEightBarSlots(PhraseCore::PhraseBank& bank) {
  const Song source = makeSourceSong();
  const PhraseCore::SlotId slots[] = {
      PhraseCore::SlotId::A,
      PhraseCore::SlotId::B,
      PhraseCore::SlotId::C,
      PhraseCore::SlotId::D,
  };
  const PhraseCore::Role roles[] = {
      PhraseCore::Role::Main,
      PhraseCore::Role::Variation,
      PhraseCore::Role::Break,
      PhraseCore::Role::Ending,
  };
  for (int index = 0; index < 4; ++index) {
    const auto result = PhraseCore::captureSongRegion(
        bank,
        slots[index],
        source,
        0,
        0,
        8,
        roles[index],
        PhraseCore::Source::InternalPattern);
    assert(result);
  }
}

void fillAllSixteenPositions(PhraseCore::PhraseBank& bank) {
  const PhraseCore::SlotId slots[] = {
      PhraseCore::SlotId::A,
      PhraseCore::SlotId::B,
      PhraseCore::SlotId::C,
      PhraseCore::SlotId::D,
  };
  for (uint8_t position = 0;
       position < PhraseCore::kArrangementCapacity;
       ++position) {
    const auto result = PhraseCore::assignArrangementStep(
        bank, position, slots[position % 4]);
    assert(result);
  }
}

void testExactCapacityWriteAndOffsetRejection() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  captureEightBarSlots(bank);
  fillAllSixteenPositions(bank);

  static_assert(Song::kMaxPositions == 128,
                "boundary test assumes the accepted 128-row Song capacity");
  assert(bank.arrangement.length == 16);
  assert(PhraseCore::arrangementTotalBars(bank) == 128);

  Song exact = makeEmptySong();
  const auto exactWrite = PhraseCore::writeArrangementToSong(
      bank, exact, 0, false);
  assert(exactWrite);
  assert(exactWrite.totalBars == 128);
  assert(exact.length == Song::kMaxPositions);
  assert(exact.positions[0].patterns[0] == 1);
  assert(exact.positions[127].patterns[0] == 8);

  Song offset = makeEmptySong();
  const Song before = offset;
  const auto range = PhraseCore::writeArrangementToSong(
      bank, offset, 1, false);
  assert(!range);
  assert(range.error == PhraseCore::Error::RegionOutOfRange);
  assert(std::memcmp(&offset, &before, sizeof(offset)) == 0);
}

}  // namespace

int main() {
  testExactCapacityWriteAndOffsetRejection();
  std::cout << "Phrase Arranger exact-capacity tests passed\n";
  return 0;
}
