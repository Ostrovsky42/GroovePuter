#include "src/phrase/pattern_lease_owner.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
std::size_t gHeapAllocations = 0;
}

void* operator new(std::size_t size) {
  ++gHeapAllocations;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
  ++gHeapAllocations;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
  std::free(memory);
}

void operator delete[](void* memory) noexcept {
  std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
  std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
  std::free(memory);
}

namespace {

using PhrasePatternLease::AcquireResult;
using PhrasePatternLease::LeaseStatus;
using PhrasePatternLease::PatternLease;
using PhrasePatternLease::PatternLeaseOwner;
using PhrasePatternLease::PreparedPersistentTransfer;

constexpr int kPage = 0;
constexpr uint8_t kA = SongPatternMaterializer::kSynthAMask;
constexpr uint8_t kB = SongPatternMaterializer::kSynthBMask;
constexpr uint8_t kD = SongPatternMaterializer::kDrumsMask;
constexpr uint8_t kAll = SongPatternMaterializer::kEditableTrackMask;

void initializeScene(Scene& scene) {
  PhraseCore::reset(scene.phraseBank);
}

int globalForLocal(int localSlot, int page = kPage) {
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  return songPatternFromPageBankIndex(page, bank, index);
}

int localForGlobal(int globalPattern) {
  return SongPatternMaterializer::localSlotFromGlobalPattern(globalPattern);
}

void writeTrackMaterial(Scene& scene, SongTrack track, int globalPattern) {
  const int localSlot = localForGlobal(globalPattern);
  assert(localSlot >= 0);
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  switch (track) {
    case SongTrack::SynthA:
      scene.synthABanks[bank].patterns[index].steps[0].note = 36;
      break;
    case SongTrack::SynthB:
      scene.synthBBanks[bank].patterns[index].steps[0].note = 48;
      break;
    case SongTrack::Drums:
      scene.drumBanks[bank].patterns[index].voices[0].steps[0].hit = 1;
      break;
    case SongTrack::Voice:
      assert(false);
      break;
  }
}

bool trackMaterialPresent(const Scene& scene,
                          SongTrack track,
                          int globalPattern) {
  const int localSlot = localForGlobal(globalPattern);
  assert(localSlot >= 0);
  return !SongPatternMaterializer::slotContentIsEmpty(scene, track, localSlot);
}

void referenceInSong(Scene& scene, SongTrack track, int globalPattern) {
  const int trackIndex = SongPatternMaterializer::editableTrackIndex(track);
  assert(trackIndex >= 0);
  scene.songs[0].positions[0].patterns[trackIndex] =
      static_cast<int16_t>(globalPattern);
}

void testAllTrackAcquireCompatibility() {
  Scene oldScene{};
  Scene explicitScene{};
  initializeScene(oldScene);
  initializeScene(explicitScene);

  PatternLeaseOwner oldOwner{};
  PatternLeaseOwner explicitOwner{};
  PatternLease oldLease{};
  PatternLease explicitLease{};

  assert(oldOwner.acquire(oldScene, kPage, 4, oldLease, 3));
  assert(explicitOwner.acquire(
      explicitScene, kPage, 4, kAll, explicitLease, 3));
  assert(oldLease.trackMask == kAll);
  assert(explicitLease.trackMask == kAll);
  assert(oldLease.count == explicitLease.count);
  for (int i = 0; i < oldLease.count; ++i) {
    assert(oldLease.globalPattern[i] == explicitLease.globalPattern[i]);
  }
}

void testSingleTrackAcquire() {
  struct Case {
    uint8_t mask;
  };
  for (const Case testCase : {Case{kA}, Case{kB}, Case{kD}}) {
    Scene scene{};
    initializeScene(scene);
    PatternLeaseOwner owner{};
    PatternLease lease{};
    const AcquireResult result = owner.acquire(
        scene, kPage, 1, testCase.mask, lease, 0);
    assert(result);
    assert(lease.trackMask == testCase.mask);
    assert(lease.globalPattern[0] == globalForLocal(0));
    assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  }
}

void testMixedRequestedUnrequestedOccupancy() {
  Scene scene{};
  initializeScene(scene);
  const int global0 = globalForLocal(0);
  writeTrackMaterial(scene, SongTrack::SynthB, global0);
  writeTrackMaterial(scene, SongTrack::Drums, global0);

  PatternLeaseOwner owner{};
  PatternLease aOnly{};
  assert(owner.acquire(scene, kPage, 1, kA, aOnly, 0));
  assert(aOnly.globalPattern[0] == global0);
  assert(owner.discard(scene, kPage, aOnly) == LeaseStatus::Ok);
  assert(trackMaterialPresent(scene, SongTrack::SynthB, global0));
  assert(trackMaterialPresent(scene, SongTrack::Drums, global0));

  PatternLease allTracks{};
  assert(owner.acquire(scene, kPage, 1, kAll, allTracks, 0));
  assert(allTracks.globalPattern[0] != global0);
}

void testRequestedReferencedTrackRejected() {
  Scene scene{};
  initializeScene(scene);
  const int protectedPattern = globalForLocal(0);
  referenceInSong(scene, SongTrack::SynthA, protectedPattern);

  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, kA, lease, 0));
  assert(lease.globalPattern[0] != protectedPattern);
}

void testUnrequestedReferencedTrackPreserved() {
  Scene scene{};
  initializeScene(scene);
  const int global0 = globalForLocal(0);
  writeTrackMaterial(scene, SongTrack::SynthB, global0);
  referenceInSong(scene, SongTrack::SynthB, global0);

  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, kA, lease, 0));
  assert(lease.globalPattern[0] == global0);
  writeTrackMaterial(scene, SongTrack::SynthA, global0);

  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  assert(!trackMaterialPresent(scene, SongTrack::SynthA, global0));
  assert(trackMaterialPresent(scene, SongTrack::SynthB, global0));
  assert(scene.songs[0].positions[0]
             .patterns[static_cast<int>(SongTrack::SynthB)] == global0);
}

void testDiscardClearsOnlyOwnedMask() {
  Scene scene{};
  initializeScene(scene);
  const int global0 = globalForLocal(0);
  writeTrackMaterial(scene, SongTrack::SynthB, global0);
  writeTrackMaterial(scene, SongTrack::Drums, global0);

  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, kA, lease, 0));
  writeTrackMaterial(scene, SongTrack::SynthA, global0);

  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  assert(!trackMaterialPresent(scene, SongTrack::SynthA, global0));
  assert(trackMaterialPresent(scene, SongTrack::SynthB, global0));
  assert(trackMaterialPresent(scene, SongTrack::Drums, global0));
}

void testRerollReusePreservesAddressAndMask() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 2, kA, lease, 2));
  const int16_t first = lease.globalPattern[0];
  const int16_t second = lease.globalPattern[1];
  for (int i = 0; i < lease.count; ++i) {
    writeTrackMaterial(scene, SongTrack::SynthA, lease.globalPattern[i]);
  }

  const AcquireResult reroll = owner.acquire(scene, kPage, 2, kA, lease, 12);
  assert(reroll && reroll.reusedExistingLease());
  assert(lease.globalPattern[0] == first);
  assert(lease.globalPattern[1] == second);
  assert(lease.trackMask == kA);

  const AcquireResult shapeMismatch = owner.acquire(
      scene, kPage, 2, kB, lease, 12);
  assert(!shapeMismatch);
  assert(shapeMismatch.status == LeaseStatus::ShapeMismatch);
  assert(lease.isActive());
  assert(lease.trackMask == kA);
}

void testSimultaneousMaskedLeaseCollisions() {
  {
    Scene scene{};
    initializeScene(scene);
    PatternLeaseOwner owner{};
    PatternLease synthA{};
    PatternLease synthB{};
    assert(owner.acquire(scene, kPage, 1, kA, synthA, 0));
    assert(owner.acquire(scene, kPage, 1, kB, synthB, 0));
    assert(synthA.globalPattern[0] == synthB.globalPattern[0]);
    assert(owner.isLeased(synthA.globalPattern[0], kA));
    assert(owner.isLeased(synthA.globalPattern[0], kB));
    assert(owner.activeLeaseCount() == 2);
  }

  {
    Scene scene{};
    initializeScene(scene);
    PatternLeaseOwner owner{};
    PatternLease synthA{};
    PatternLease overlap{};
    assert(owner.acquire(scene, kPage, 1, kA, synthA, 0));
    assert(owner.acquire(
        scene, kPage, 1, static_cast<uint8_t>(kA | kB), overlap, 0));
    assert(synthA.globalPattern[0] == globalForLocal(0));
    assert(overlap.globalPattern[0] != synthA.globalPattern[0]);
  }
}

void testPrepareTransferValidation() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, kA, lease, 0));

  PreparedPersistentTransfer prepared{};
  assert(owner.preparePersistentTransfer(scene, 1, lease, prepared) ==
         LeaseStatus::PageMismatch);
  assert(!prepared.isPrepared());
  assert(lease.isActive());

  referenceInSong(scene, SongTrack::SynthA, lease.globalPattern[0]);
  assert(owner.preparePersistentTransfer(scene, kPage, lease, prepared) ==
         LeaseStatus::PersistentReference);
  assert(!prepared.isPrepared());
  assert(lease.isActive());
}

void testFailedPersistentCommitLeavesLeaseActive() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, kA, lease, 0));
  const int candidate = lease.globalPattern[0];
  writeTrackMaterial(scene, SongTrack::SynthA, candidate);

  PreparedPersistentTransfer prepared{};
  assert(owner.preparePersistentTransfer(scene, kPage, lease, prepared) ==
         LeaseStatus::Ok);

  // Simulated canonical commit failure: no persistent mutation and no complete.
  assert(lease.isActive());
  assert(owner.activeLeaseCount() == 1);
  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  assert(!trackMaterialPresent(scene, SongTrack::SynthA, candidate));
}

void testCompleteAfterCommitCannotFailOrClearAcceptedBytes() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, kA, lease, 0));
  const int accepted = lease.globalPattern[0];
  writeTrackMaterial(scene, SongTrack::SynthA, accepted);

  PreparedPersistentTransfer prepared{};
  assert(owner.preparePersistentTransfer(scene, kPage, lease, prepared) ==
         LeaseStatus::Ok);

  // Canonical Song mutation succeeds.
  referenceInSong(scene, SongTrack::SynthA, accepted);

  assert(owner.completePersistentTransfer(lease, prepared) == LeaseStatus::Ok);
  assert(!lease.isActive());
  assert(owner.activeLeaseCount() == 0);
  assert(trackMaterialPresent(scene, SongTrack::SynthA, accepted));
  assert(scene.songs[0].positions[0]
             .patterns[static_cast<int>(SongTrack::SynthA)] == accepted);

  PatternLease next{};
  assert(owner.acquire(scene, kPage, 1, kA, next, 0));
  assert(next.globalPattern[0] != accepted);
}

void testInvalidTrackMaskRejectedWithoutMutation() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  const AcquireResult emptyMask = owner.acquire(scene, kPage, 1, 0, lease, 0);
  assert(!emptyMask);
  assert(emptyMask.status == LeaseStatus::InvalidTrackMask);
  assert(!lease.isActive());
  assert(owner.activeLeaseCount() == 0);

  const AcquireResult unknownMask = owner.acquire(
      scene, kPage, 1, static_cast<uint8_t>(1u << 7), lease, 0);
  assert(!unknownMask);
  assert(unknownMask.status == LeaseStatus::InvalidTrackMask);
  assert(!lease.isActive());
  assert(owner.activeLeaseCount() == 0);
}

void testNoHeapAllocation() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  PreparedPersistentTransfer prepared{};
  const std::size_t before = gHeapAllocations;

  assert(owner.acquire(scene, kPage, 4, kD, lease, 0));
  for (int i = 0; i < lease.count; ++i) {
    writeTrackMaterial(scene, SongTrack::Drums, lease.globalPattern[i]);
  }
  const AcquireResult reroll = owner.acquire(scene, kPage, 4, kD, lease, 9);
  assert(reroll && reroll.reusedExistingLease());
  assert(owner.preparePersistentTransfer(scene, kPage, lease, prepared) ==
         LeaseStatus::Ok);
  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);

  assert(gHeapAllocations == before);
}

}  // namespace

static_assert(sizeof(PhrasePatternLease::PatternLease) == 14,
              "P1a2 PatternLease budget changed");
static_assert(sizeof(PhrasePatternLease::PreparedPersistentTransfer) == 14,
              "P1a2 prepared transfer budget changed");
static_assert(sizeof(PhrasePatternLease::PatternLeaseOwner) == 28,
              "P1a2 PatternLeaseOwner budget changed");

int main() {
  testAllTrackAcquireCompatibility();
  testSingleTrackAcquire();
  testMixedRequestedUnrequestedOccupancy();
  testRequestedReferencedTrackRejected();
  testUnrequestedReferencedTrackPreserved();
  testDiscardClearsOnlyOwnedMask();
  testRerollReusePreservesAddressAndMask();
  testSimultaneousMaskedLeaseCollisions();
  testPrepareTransferValidation();
  testFailedPersistentCommitLeavesLeaseActive();
  testCompleteAfterCommitCannotFailOrClearAcceptedBytes();
  testInvalidTrackMaskRejectedWithoutMutation();
  testNoHeapAllocation();
  std::puts("0.9.9-P1a2 pattern lease generalization: PASS");
  return 0;
}
