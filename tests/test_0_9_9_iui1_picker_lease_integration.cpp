#include "src/audio/pattern_paging.h"
#include "src/phrase/pattern_lease_owner.h"
#include "src/state/undo_receipts.h"

#include "platform_sdl/arduino_compat.h"

#include <cassert>
#include <cstdio>
#include <filesystem>

SerialMock Serial;
SDMock SD;

Scene& sceneTransactionScratch() {
  static Scene scratch{};
  return scratch;
}

namespace {

using GroovePuterUndo::SongUndoPayload;
using GroovePuterUndo::UndoKind;
using GroovePuterUndo::UndoOwner;
using GroovePuterUndo::UndoResult;
using PhrasePatternLease::LeaseStatus;
using PhrasePatternLease::PatternLease;
using PhrasePatternLease::PatternLeaseOwner;
using PhrasePatternLease::PersistentClass;
using PhrasePatternLease::PreparedPersistentTransfer;

constexpr int kPage = 0;
constexpr int kRow = 3;
constexpr SongTrack kTrack = SongTrack::SynthA;

int trackIndex() {
  return SongPatternMaterializer::editableTrackIndex(kTrack);
}

int songReference(const Scene& scene) {
  return scene.songs[0].positions[kRow].patterns[trackIndex()];
}

void setSongReference(Scene& scene, int globalPattern) {
  scene.songs[0].positions[kRow].patterns[trackIndex()] =
      static_cast<int16_t>(globalPattern);
}

int localSlot(int globalPattern) {
  return SongPatternMaterializer::localSlotFromGlobalPattern(globalPattern);
}

void writeCandidate(Scene& scene, int globalPattern, int note) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  assert(address.valid());
  scene.synthABanks[address.bank].patterns[address.slot].steps[0].note =
      static_cast<int8_t>(note);
}

int candidateNote(const Scene& scene, int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  assert(address.valid());
  return scene.synthABanks[address.bank].patterns[address.slot].steps[0].note;
}

bool candidatePresent(const Scene& scene, int globalPattern) {
  return !SongPatternMaterializer::slotContentIsEmpty(
      scene, kTrack, localSlot(globalPattern));
}

SongUndoPayload captureSong(const Scene& scene) {
  SongUndoPayload payload{};
  payload.pageIndex = kPage;
  payload.songSlot = 0;
  payload.before = scene.songs[0];
  return payload;
}

bool canonicalSongCommit(Scene& scene,
                         UndoOwner& undo,
                         int globalPattern) {
  const SongUndoPayload before = captureSong(scene);
  Song after = before.before;
  after.positions[kRow].patterns[trackIndex()] =
      static_cast<int16_t>(globalPattern);
  if (GroovePuterUndo::sameSong(before.before, after)) return false;
  return undo.commitPrepared(UndoKind::Song, before, [&] {
    scene.songs[before.songSlot] = after;
  });
}

UndoResult toggleSong(Scene& scene, UndoOwner& undo) {
  return undo.togglePrepared<SongUndoPayload>(
      UndoKind::Song,
      [](const SongUndoPayload& payload) {
        return payload.pageIndex == kPage && payload.songSlot == 0;
      },
      [&](SongUndoPayload& payload) {
        GroovePuterUndo::exchangeFixedValue(
            scene.songs[payload.songSlot], payload.before);
      });
}

void resetUndo(UndoOwner& undo) {
  undo.clear();
  GroovePuterState::restoreSceneRevision({100, 100});
}

void testExistingCancelAndAcceptUndoRedo() {
  Scene scene{};
  PatternLeaseOwner leases{};
  UndoOwner undo{};
  resetUndo(undo);
  const int original = songPatternFromPageBankIndex(kPage, 0, 0);
  const int existing = songPatternFromPageBankIndex(kPage, 0, 1);
  setSongReference(scene, original);
  writeCandidate(scene, original, 36);
  writeCandidate(scene, existing, 48);
  const Scene beforeBrowse = scene;

  // EXISTING preview/cancel changes runtime audition only; this persistent
  // model remains byte-identical and owns no lease or receipt.
  assert(leases.activeLeaseCount() == 0);
  assert(!undo.hasUndo());
  assert(std::memcmp(&scene, &beforeBrowse, sizeof(scene)) == 0);

  assert(canonicalSongCommit(scene, undo, existing));
  assert(songReference(scene) == existing);
  assert(leases.activeLeaseCount() == 0);
  assert(undo.hasUndo() && undo.kind() == UndoKind::Song);
  assert(candidatePresent(scene, original));
  assert(candidatePresent(scene, existing));

  assert(toggleSong(scene, undo) == UndoResult::Restored);
  assert(songReference(scene) == original);
  assert(candidatePresent(scene, existing));
  assert(leases.activeLeaseCount() == 0);
  assert(undo.nextIsRedo());

  assert(toggleSong(scene, undo) == UndoResult::Restored);
  assert(songReference(scene) == existing);
  assert(candidatePresent(scene, existing));
  assert(leases.activeLeaseCount() == 0);
  assert(!undo.nextIsRedo());
}

void testGenerateAcquireRerollCancelAndReuse() {
  Scene scene{};
  PatternLeaseOwner owner{};
  UndoOwner undo{};
  resetUndo(undo);
  const int original = songPatternFromPageBankIndex(kPage, 1, 0);
  setSongReference(scene, original);
  const uint8_t mask = SongPatternMaterializer::maskForTrack(kTrack);
  int stableAddress = -1;

  for (int cycle = 0; cycle < 4; ++cycle) {
    PatternLease lease{};
    const auto acquired = owner.acquire(scene, kPage, 1, mask, lease, 0);
    assert(acquired && !acquired.reusedExistingLease());
    assert(lease.count == 1 && lease.trackMask == mask);
    assert(owner.activeLeaseCount() == 1);
    if (stableAddress < 0) stableAddress = lease.globalPattern[0];
    assert(lease.globalPattern[0] == stableAddress);

    writeCandidate(scene, stableAddress, 40 + cycle);
    const auto rerolled = owner.acquire(scene, kPage, 1, mask, lease, 7);
    assert(rerolled && rerolled.reusedExistingLease());
    assert(owner.activeLeaseCount() == 1);
    assert(lease.globalPattern[0] == stableAddress);
    writeCandidate(scene, stableAddress, 60 + cycle);
    assert(candidateNote(scene, stableAddress) == 60 + cycle);
    assert(songReference(scene) == original);
    assert(!undo.hasUndo());

    assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
    assert(owner.activeLeaseCount() == 0);
    assert(!lease.isActive());
    assert(!candidatePresent(scene, stableAddress));
    assert(songReference(scene) == original);
    assert(!undo.hasUndo());
    assert(owner.discard(scene, kPage, lease) == LeaseStatus::InvalidLease);
  }
}

void testGenerateAcceptUndoRedoAndFailureRecovery() {
  Scene scene{};
  PatternLeaseOwner owner{};
  UndoOwner undo{};
  resetUndo(undo);
  const int original = songPatternFromPageBankIndex(kPage, 1, 0);
  setSongReference(scene, original);
  PatternLease lease{};
  const uint8_t mask = SongPatternMaterializer::maskForTrack(kTrack);
  assert(owner.acquire(scene, kPage, 1, mask, lease, 0));
  const int generated = lease.globalPattern[0];
  writeCandidate(scene, generated, 72);

  PreparedPersistentTransfer wrongPage{};
  assert(owner.preparePersistentTransfer(
             scene, 1, lease, PersistentClass::SongGenerated, wrongPage) ==
         LeaseStatus::PageMismatch);
  assert(lease.isActive() && owner.activeLeaseCount() == 1);

  PreparedPersistentTransfer prepared{};
  assert(owner.preparePersistentTransfer(
             scene, kPage, lease, PersistentClass::SongGenerated, prepared) ==
         LeaseStatus::Ok);
  assert(SongPatternMaterializer::slotIsSongGenerated(
      scene, kTrack, localSlot(generated)));

  // A failed canonical commit does not complete or discard the transfer.
  UndoOwner rejectingUndo{};
  resetUndo(rejectingUndo);
  const SongUndoPayload before = captureSong(scene);
  bool applied = false;
  assert(!rejectingUndo.commitPrepared(
      UndoKind::None, before, [&] { applied = true; }));
  assert(!applied);
  assert(lease.isActive() && owner.activeLeaseCount() == 1);
  assert(songReference(scene) == original);

  assert(canonicalSongCommit(scene, undo, generated));
  assert(songReference(scene) == generated);
  assert(owner.completePersistentTransfer(lease, prepared) == LeaseStatus::Ok);
  assert(!lease.isActive() && owner.activeLeaseCount() == 0);
  assert(candidateNote(scene, generated) == 72);
  assert(SongPatternMaterializer::slotIsSongGenerated(
      scene, kTrack, localSlot(generated)));
  assert(owner.completePersistentTransfer(lease, prepared) ==
         LeaseStatus::InvalidLease);

  assert(toggleSong(scene, undo) == UndoResult::Restored);
  assert(songReference(scene) == original);
  assert(candidateNote(scene, generated) == 72);
  assert(owner.activeLeaseCount() == 0);
  assert(toggleSong(scene, undo) == UndoResult::Restored);
  assert(songReference(scene) == generated);
  assert(candidateNote(scene, generated) == 72);
  assert(owner.activeLeaseCount() == 0);
}

void testFailureAndPagePinPaths() {
  Scene scene{};
  PatternLeaseOwner localOwner{};
  PatternLease first{};
  PatternLease second{};
  PatternLease third{};
  const uint8_t mask = SongPatternMaterializer::maskForTrack(kTrack);
  assert(localOwner.acquire(scene, kPage, 1, mask, first, 0));
  assert(localOwner.acquire(scene, kPage, 1, mask, second, 1));
  const auto full = localOwner.acquire(scene, kPage, 1, mask, third, 2);
  assert(!full && full.status == LeaseStatus::OwnerFull);
  assert(localOwner.activeLeaseCount() == 2);
  assert(localOwner.discard(scene, kPage, first) == LeaseStatus::Ok);
  assert(localOwner.discard(scene, kPage, second) == LeaseStatus::Ok);

  auto& sharedOwner = PhrasePatternLease::patternLeaseOwner();
  PatternLease pinned{};
  assert(sharedOwner.activeLeaseCount() == 0);
  assert(sharedOwner.acquire(scene, kPage, 1, mask, pinned, 0));
  writeCandidate(scene, pinned.globalPattern[0], 55);
  assert(!PatternPagingService::savePage(kPage, scene));
  assert(!PatternPagingService::loadPage(kPage, scene));
  assert(!PatternPagingService::restoreBackup(kPage));
  assert(pinned.isActive() && sharedOwner.activeLeaseCount() == 1);

  // Picker close/onExit owns the same fail-closed discard path.
  assert(sharedOwner.discard(scene, kPage, pinned) == LeaseStatus::Ok);
  assert(sharedOwner.activeLeaseCount() == 0);
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
      "grooveputer-iui1-pattern-paging";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root);
  SD.setRoot(root);

  testExistingCancelAndAcceptUndoRedo();
  testGenerateAcquireRerollCancelAndReuse();
  testGenerateAcceptUndoRedoAndFailureRecovery();
  testFailureAndPagePinPaths();

  std::filesystem::remove_all(root, error);
  std::puts("0.9.9-IUI1 Pattern Picker/PatternLease integration: PASS");
  return 0;
}
