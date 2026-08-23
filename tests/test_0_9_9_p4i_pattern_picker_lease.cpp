#include "src/phrase/pattern_lease_owner.h"

#include <cassert>
#include <cstdio>

namespace {

using PhrasePatternLease::LeaseStatus;
using PhrasePatternLease::PatternLease;
using PhrasePatternLease::PatternLeaseOwner;
using PhrasePatternLease::PersistentClass;
using PhrasePatternLease::PreparedPersistentTransfer;

constexpr int kPage = 0;

int localForGlobal(int globalPattern) {
  return SongPatternMaterializer::localSlotFromGlobalPattern(globalPattern);
}

void initializeScene(Scene& scene) {
  PhraseCore::reset(scene.phraseBank);
}

void writeRequestedMaterial(Scene& scene, SongTrack track, int globalPattern) {
  const int local = localForGlobal(globalPattern);
  assert(local >= 0);
  const int bank = local / Bank<SynthPattern>::kPatterns;
  const int slot = local % Bank<SynthPattern>::kPatterns;
  switch (track) {
    case SongTrack::SynthA:
      scene.synthABanks[bank].patterns[slot].steps[0].note = 36;
      break;
    case SongTrack::SynthB:
      scene.synthBBanks[bank].patterns[slot].steps[0].note = 60;
      break;
    case SongTrack::Drums:
      scene.drumBanks[bank].patterns[slot].voices[0].steps[0].hit = 1;
      break;
    case SongTrack::Voice:
      assert(false);
      break;
  }
}

bool materialPresent(const Scene& scene, SongTrack track, int globalPattern) {
  const int local = localForGlobal(globalPattern);
  assert(local >= 0);
  return !SongPatternMaterializer::slotContentIsEmpty(scene, track, local);
}

void setUnrequestedSentinels(Scene& scene, SongTrack requested, int local) {
  const int bank = local / Bank<SynthPattern>::kPatterns;
  const int slot = local % Bank<SynthPattern>::kPatterns;
  if (requested != SongTrack::SynthA) {
    scene.synthABanks[bank].patterns[slot].steps[4].note = 41;
  }
  if (requested != SongTrack::SynthB) {
    scene.synthBBanks[bank].patterns[slot].steps[5].note = 73;
  }
  if (requested != SongTrack::Drums) {
    scene.drumBanks[bank].patterns[slot].voices[2].steps[6].hit = 1;
  }
}

void assertUnrequestedSentinels(const Scene& scene,
                                SongTrack requested,
                                int local) {
  const int bank = local / Bank<SynthPattern>::kPatterns;
  const int slot = local % Bank<SynthPattern>::kPatterns;
  if (requested != SongTrack::SynthA) {
    assert(scene.synthABanks[bank].patterns[slot].steps[4].note == 41);
  }
  if (requested != SongTrack::SynthB) {
    assert(scene.synthBBanks[bank].patterns[slot].steps[5].note == 73);
  }
  if (requested != SongTrack::Drums) {
    assert(scene.drumBanks[bank].patterns[slot].voices[2].steps[6].hit == 1);
  }
}

void referenceInSong(Scene& scene, SongTrack track, int globalPattern) {
  const int index = SongPatternMaterializer::editableTrackIndex(track);
  assert(index >= 0);
  scene.songs[0].positions[0].patterns[index] =
      static_cast<int16_t>(globalPattern);
}

void clearSongReference(Scene& scene, SongTrack track) {
  const int index = SongPatternMaterializer::editableTrackIndex(track);
  assert(index >= 0);
  scene.songs[0].positions[0].patterns[index] = -1;
}

void testFailedCommitKeepsLeaseAndDiscardScope(SongTrack track) {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  const uint8_t mask = SongPatternMaterializer::maskForTrack(track);

  // Unrequested aligned tracks may be occupied; the one-track lease still owns
  // only the selected Song lane.
  setUnrequestedSentinels(scene, track, 0);
  assert(owner.acquire(scene, kPage, 1, mask, lease, 0));
  const int candidate = lease.globalPattern[0];
  assert(localForGlobal(candidate) == 0);
  writeRequestedMaterial(scene, track, candidate);

  const auto reroll = owner.acquire(scene, kPage, 1, mask, lease, 9);
  assert(reroll && reroll.reusedExistingLease());
  assert(lease.globalPattern[0] == candidate);

  PreparedPersistentTransfer prepared{};
  assert(owner.preparePersistentTransfer(
             scene, kPage, lease, PersistentClass::SongGenerated, prepared) ==
         LeaseStatus::Ok);
  assert(SongPatternMaterializer::slotIsSongGenerated(
      scene, track, localForGlobal(candidate)));

  // Simulated canonical Song commit failure: do not write a Song reference and
  // do not call complete. The lease must remain the rollback owner.
  assert(lease.isActive());
  assert(owner.activeLeaseCount() == 1);
  assert(owner.discard(scene, kPage, lease) == LeaseStatus::Ok);
  assert(!materialPresent(scene, track, candidate));
  assertUnrequestedSentinels(scene, track, 0);
}

void testAcceptedCandidateSurvivesReferenceUndo(SongTrack track) {
  Scene scene{};
  initializeScene(scene);
  PatternLeaseOwner owner{};
  PatternLease lease{};
  const uint8_t mask = SongPatternMaterializer::maskForTrack(track);

  setUnrequestedSentinels(scene, track, 0);
  assert(owner.acquire(scene, kPage, 1, mask, lease, 0));
  const int accepted = lease.globalPattern[0];
  writeRequestedMaterial(scene, track, accepted);

  PreparedPersistentTransfer prepared{};
  assert(owner.preparePersistentTransfer(
             scene, kPage, lease, PersistentClass::SongGenerated, prepared) ==
         LeaseStatus::Ok);

  // Canonical Song mutation succeeds between prepare and complete.
  referenceInSong(scene, track, accepted);
  assert(owner.completePersistentTransfer(lease, prepared) == LeaseStatus::Ok);
  assert(!lease.isActive());
  assert(materialPresent(scene, track, accepted));
  assert(SongPatternMaterializer::slotIsSongGenerated(
      scene, track, localForGlobal(accepted)));
  assertUnrequestedSentinels(scene, track, 0);

  // Canonical Song Undo removes only the reference. Accepted backing remains in
  // the existing Song-generated/reclaimable storage class.
  clearSongReference(scene, track);
  assert(SongPatternMaterializer::globalPatternReferenceCount(
             scene, track, accepted) == 0);
  assert(materialPresent(scene, track, accepted));
  assert(SongPatternMaterializer::slotIsSongGenerated(
      scene, track, localForGlobal(accepted)));

  SongPatternMaterializer::Request request{};
  request.row = 0;
  request.pageIndex = kPage;
  bool reusedGenerated = false;
  const int reusable = SongPatternMaterializer::findReusableLocalSlot(
      scene, request, track, 0, reusedGenerated);
  assert(reusable == localForGlobal(accepted));
  assert(reusedGenerated);
}

}  // namespace

int main() {
  for (SongTrack track :
       {SongTrack::SynthA, SongTrack::SynthB, SongTrack::Drums}) {
    testFailedCommitKeepsLeaseAndDiscardScope(track);
    testAcceptedCandidateSurvivesReferenceUndo(track);
  }

  std::puts("0.9.9-P4I Pattern Picker lease lifecycle tests passed");
  return 0;
}
