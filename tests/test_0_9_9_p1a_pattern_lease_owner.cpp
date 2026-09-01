#include "src/phrase/pattern_lease_owner.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

constexpr int kPage = 0;

void initializeScene(Scene& scene) {
  PhraseCore::reset(scene.phraseBank);
}

int globalForLocal(int localSlot, int page = kPage) {
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  return songPatternFromPageBankIndex(page, bank, index);
}

void writeCandidateMaterial(Scene& scene, int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  assert(address.valid());
  assert(address.page == kPage);
  scene.synthABanks[address.bank].patterns[address.slot].steps[0].note = 36;
  scene.synthBBanks[address.bank].patterns[address.slot].steps[0].note = 48;
  scene.drumBanks[address.bank].patterns[address.slot]
      .voices[0].steps[0].hit = 1;
}

bool candidateMaterialPresent(const Scene& scene, int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  assert(address.valid());
  return !PhraseGenerator::localSlotIsEmpty(
      scene,
      address.bank * Bank<SynthPattern>::kPatterns + address.slot);
}

void fillLocalSlot(Scene& scene, int localSlot) {
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  scene.synthABanks[bank].patterns[index].steps[0].note = 60;
}

void referenceInPhrase(Scene& scene,
                       int globalPattern,
                       uint8_t trackMask) {
  PhraseCore::PhraseSlot& phrase = scene.phraseBank.slots[0];
  PhraseCore::clearSlotValue(phrase);
  phrase.metadata.phraseId = 1;
  phrase.metadata.lengthBars = 1;
  phrase.metadata.role = PhraseCore::Role::Main;
  phrase.metadata.source = PhraseCore::Source::InternalPattern;
  phrase.metadata.storage = PhraseCore::StorageMode::ReferenceView;
  phrase.metadata.flags =
      PhraseCore::kFlagValid | PhraseCore::kFlagReferenceView |
      PhraseCore::kFlagMutableBacking;
  phrase.metadata.sourceSongSlot = 0;
  phrase.metadata.sourceStartRow = 0;
  phrase.metadata.trackMask = trackMask;
  for (int track = 0; track < PhraseCore::kTrackCount; ++track) {
    if ((trackMask & PhraseCore::maskForTrackIndex(track)) != 0) {
      phrase.patternRefs[0][track] = static_cast<int16_t>(globalPattern);
    }
  }
}

void assertUnique(const PatternLease& lease) {
  for (int left = 0; left < lease.count; ++left) {
    assert(lease.globalPattern[left] >= 0);
    for (int right = left + 1; right < lease.count; ++right) {
      assert(lease.globalPattern[left] != lease.globalPattern[right]);
    }
  }
}

void testAcquireSupportedLengths() {
  for (uint8_t bars : {uint8_t{1}, uint8_t{2}, uint8_t{4}}) {
    Scene scene{};
    initializeScene(scene);
    PatternLeaseOwner owner{};
    PatternLease lease{};
    const AcquireResult acquired = owner.acquire(scene, kPage, bars, lease);
    assert(acquired);
    assert(!acquired.reusedExistingLease());
    assert(lease.isActive());
    assert(lease.count == bars);
    assert(owner.activeLeaseCount() == 1);
    assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  }
}

void testUniqueAddresses() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 4, lease));
  assertUnique(lease);
}

void testSongReferenceNeverLeased() {
  Scene scene{};
  initializeScene(scene);
  const int protectedPattern = globalForLocal(0);
  scene.songs[0].positions[0]
      .patterns[static_cast<int>(SongTrack::SynthA)] = protectedPattern;

  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, lease, 0));
  assert(lease.globalPattern[0] != protectedPattern);
}

void testPhraseReferenceNeverLeased() {
  Scene scene{};
  initializeScene(scene);
  const int protectedPattern = globalForLocal(0);
  referenceInPhrase(scene, protectedPattern, PhraseCore::kTrackSynthA);

  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, lease, 0));
  assert(lease.globalPattern[0] != protectedPattern);
}

void testSimultaneousLeaseCollisionPrevented() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease first{};
  PatternLease second{};
  PatternLease third{};
  assert(owner.acquire(scene, kPage, 4, first, 0));
  assert(owner.acquire(scene, kPage, 4, second, 0));
  assert(owner.activeLeaseCount() == 2);

  for (int left = 0; left < first.count; ++left) {
    for (int right = 0; right < second.count; ++right) {
      assert(first.globalPattern[left] != second.globalPattern[right]);
    }
  }

  const AcquireResult noOwnerSlot = owner.acquire(scene, kPage, 1, third, 0);
  assert(!noOwnerSlot);
  assert(noOwnerSlot.status == LeaseStatus::OwnerFull);
  assert(!third.isActive());
}

void testExhaustionFailsWithoutMutation() {
  Scene scene{};
  initializeScene(scene);
  for (int local = 3; local < kPatternsPerPage; ++local) {
    fillLocalSlot(scene, local);
  }

  PatternLeaseOwner owner{};
  PatternLease lease{};
  const PatternLease before = lease;
  const AcquireResult result = owner.acquire(scene, kPage, 4, lease, 0);
  assert(!result);
  assert(result.status == LeaseStatus::Exhausted);
  assert(std::memcmp(&lease, &before, sizeof(lease)) == 0);
  assert(owner.activeLeaseCount() == 0);
  for (int local = 3; local < kPatternsPerPage; ++local) {
    assert(scene.synthABanks[local / Bank<SynthPattern>::kPatterns]
               .patterns[local % Bank<SynthPattern>::kPatterns]
               .steps[0].note == 60);
  }
}

void testDiscardClearsAndReturnsAddress() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, lease, 0));
  const int firstGlobal = lease.globalPattern[0];
  writeCandidateMaterial(scene, firstGlobal);
  assert(candidateMaterialPresent(scene, firstGlobal));

  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  assert(!lease.isActive());
  assert(owner.activeLeaseCount() == 0);
  assert(!candidateMaterialPresent(scene, firstGlobal));

  PatternLease again{};
  assert(owner.acquire(scene, kPage, 1, again, 0));
  assert(again.globalPattern[0] == firstGlobal);
}

void testActiveLeaseReuseDoesNotGrowPool() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 4, lease, 0));
  int16_t original[PhrasePatternLease::kMaxLeasePatterns]{};
  for (int i = 0; i < lease.count; ++i) {
    original[i] = lease.globalPattern[i];
    writeCandidateMaterial(scene, lease.globalPattern[i]);
  }

  const AcquireResult reroll = owner.acquire(scene, kPage, 4, lease, 12);
  assert(reroll);
  assert(reroll.reusedExistingLease());
  assert(owner.activeLeaseCount() == 1);
  for (int i = 0; i < lease.count; ++i) {
    assert(lease.globalPattern[i] == original[i]);
  }
}

void testTransferMakesBackingPermanent() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, lease, 0));
  const int committedPattern = lease.globalPattern[0];
  writeCandidateMaterial(scene, committedPattern);
  referenceInPhrase(scene, committedPattern, PhraseCore::kAllTracks);

  assert(owner.transferCommittedOwnership(scene, kPage, lease) ==
         LeaseStatus::Ok);
  assert(!lease.isActive());
  assert(owner.activeLeaseCount() == 0);
  assert(candidateMaterialPresent(scene, committedPattern));

  PatternLease next{};
  assert(owner.acquire(scene, kPage, 1, next, 0));
  assert(next.globalPattern[0] != committedPattern);
}

void testTransferRejectsIncompleteOwnership() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, lease, 0));
  const int candidate = lease.globalPattern[0];
  writeCandidateMaterial(scene, candidate);
  referenceInPhrase(scene, candidate, PhraseCore::kTrackSynthA);

  assert(owner.transferCommittedOwnership(scene, kPage, lease) ==
         LeaseStatus::IncompletePersistentOwnership);
  assert(lease.isActive());
  assert(owner.activeLeaseCount() == 1);
}

void testDiscardRejectsReferencedBacking() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, lease, 0));
  const int candidate = lease.globalPattern[0];
  writeCandidateMaterial(scene, candidate);
  scene.songs[0].positions[0]
      .patterns[static_cast<int>(SongTrack::Drums)] = candidate;

  assert(owner.discard(scene, kPage, lease) ==
         LeaseStatus::PersistentReference);
  assert(lease.isActive());
  assert(candidateMaterialPresent(scene, candidate));
}

void testInvalidAndDoubleReleaseSafety() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease inactive{};
  assert(owner.discard(scene, kPage, inactive) == LeaseStatus::InvalidLease);

  PatternLease lease{};
  assert(owner.acquire(scene, kPage, 1, lease, 0));
  PatternLease staleCopy = lease;
  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  assert(owner.discard(scene, kPage, staleCopy) == LeaseStatus::InvalidLease);
  assert(owner.activeLeaseCount() == 0);
}

void testUnsupportedEightBars() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  const AcquireResult result = owner.acquire(scene, kPage, 8, lease, 0);
  assert(!result);
  assert(result.status == LeaseStatus::UnsupportedLength);
  assert(owner.activeLeaseCount() == 0);
}

void testLeaseOwnerDoesNotAllocateHeap() {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  const std::size_t before = gHeapAllocations;

  const AcquireResult first = owner.acquire(scene, kPage, 4, lease, 0);
  assert(first);
  for (int i = 0; i < lease.count; ++i) {
    writeCandidateMaterial(scene, lease.globalPattern[i]);
  }
  const AcquireResult reroll = owner.acquire(scene, kPage, 4, lease, 0);
  assert(reroll && reroll.reusedExistingLease());
  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);

  assert(gHeapAllocations == before);
}

}  // namespace

static_assert(sizeof(PhrasePatternLease::PatternLease) == 14,
              "P1a lease value budget changed");
static_assert(sizeof(PhrasePatternLease::PatternLeaseOwner) == 28,
              "P1a lease owner budget changed");

int main() {
  testAcquireSupportedLengths();
  testUniqueAddresses();
  testSongReferenceNeverLeased();
  testPhraseReferenceNeverLeased();
  testSimultaneousLeaseCollisionPrevented();
  testExhaustionFailsWithoutMutation();
  testDiscardClearsAndReturnsAddress();
  testActiveLeaseReuseDoesNotGrowPool();
  testTransferMakesBackingPermanent();
  testTransferRejectsIncompleteOwnership();
  testDiscardRejectsReferencedBacking();
  testInvalidAndDoubleReleaseSafety();
  testUnsupportedEightBars();
  testLeaseOwnerDoesNotAllocateHeap();
  std::puts("0.9.9-P1a pattern lease owner: PASS");
  return 0;
}
