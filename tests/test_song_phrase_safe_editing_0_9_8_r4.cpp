#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/state/undo_owner.h"
#include "src/state/song_phrase_undo_receipts.h"

namespace {
void resetState(uint32_t current, uint32_t persisted) {
  GroovePuterUndo::undoOwner().clear();
  GroovePuterState::restoreSceneRevision({current, persisted});
}
}

int main() {
  using GroovePuterUndo::UndoKind;
  using GroovePuterUndo::UndoResult;
  auto& owner = GroovePuterUndo::undoOwner();

  static_assert(sizeof(GroovePuterUndo::SongUndoPayload) <= 1040,
                "R4 Song receipt budget changed");
  static_assert(sizeof(GroovePuterUndo::PhraseBankUndoPayload) <= 256,
                "R4 PhraseBank receipt budget changed");
  static_assert(sizeof(GroovePuterUndo::SongUndoPayload) <=
                    GroovePuterUndo::kUndoPayloadBytes,
                "Song receipt no longer fits canonical owner");
  static_assert(sizeof(GroovePuterUndo::PhraseBankUndoPayload) <=
                    GroovePuterUndo::kUndoPayloadBytes,
                "Phrase receipt no longer fits canonical owner");
  static_assert(sizeof(GroovePuterUndo::UndoOwner) <= 1552,
                "R4 must not grow the resident Undo owner");

  // Song: full-slot before-image restores length, reverse and every lane while
  // selector identity remains outside the payload.
  resetState(100, 100);
  Song song{};
  song.length = 4;
  song.reverse = true;
  song.positions[2].patterns[0] = 17;
  GroovePuterUndo::SongUndoPayload song_before{};
  song_before.songSlot = 1;
  song_before.before = song;
  Song edited = song;
  edited.length = 8;
  edited.reverse = false;
  edited.positions[2].patterns[0] = 33;
  assert(!GroovePuterUndo::songsEqual(song, edited));
  assert(owner.commitPrepared(UndoKind::Song, song_before, [&] { song = edited; }));
  assert(song.length == 8 && song.positions[2].patterns[0] == 33);
  auto revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 101);
  auto result = owner.undoPrepared<GroovePuterUndo::SongUndoPayload>(
      UndoKind::Song,
      [](const GroovePuterUndo::SongUndoPayload& receipt) {
        return GroovePuterUndo::validSongAddress(receipt);
      },
      [&](const GroovePuterUndo::SongUndoPayload& receipt) { song = receipt.before; });
  assert(result == UndoResult::Restored);
  assert(song.length == 4 && song.reverse && song.positions[2].patterns[0] == 17);
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 100 && revision.persistedRevision == 100);

  // Phrase: the whole bank is the bounded identity because CAPTURE/DERIVE also
  // advance nextPhraseId. Undo must restore allocator state with the slot.
  resetState(200, 199);
  PhraseCore::PhraseBank bank{};
  bank.nextPhraseId = 7;
  bank.slots[0].phrase.id = 6;
  GroovePuterUndo::PhraseBankUndoPayload phrase_before{};
  phrase_before.before = bank;
  PhraseCore::PhraseBank phrase_after = bank;
  phrase_after.nextPhraseId = 8;
  phrase_after.slots[0].phrase.id = 7;
  assert(!GroovePuterUndo::phraseBanksEqual(bank, phrase_after));
  assert(owner.commitPrepared(UndoKind::Phrase, phrase_before,
                              [&] { bank = phrase_after; }));
  assert(bank.nextPhraseId == 8 && bank.slots[0].phrase.id == 7);
  result = owner.undoPrepared<GroovePuterUndo::PhraseBankUndoPayload>(
      UndoKind::Phrase,
      [](const GroovePuterUndo::PhraseBankUndoPayload&) { return true; },
      [&](const GroovePuterUndo::PhraseBankUndoPayload& receipt) {
        bank = receipt.before;
      });
  assert(result == UndoResult::Restored);
  assert(bank.nextPhraseId == 7 && bank.slots[0].phrase.id == 6);
  revision = GroovePuterState::sceneRevisionSnapshot();
  assert(revision.currentRevision == 200 && revision.persistedRevision == 199);
  assert(revision.dirty());

  std::printf("R4 SongReceipt=%zu PhraseBankReceipt=%zu UndoOwner=%zu\n",
              sizeof(GroovePuterUndo::SongUndoPayload),
              sizeof(GroovePuterUndo::PhraseBankUndoPayload),
              sizeof(GroovePuterUndo::UndoOwner));
  std::puts("0.9.8 R4 Song/Phrase safe-edit contracts: PASS");
  return 0;
}
