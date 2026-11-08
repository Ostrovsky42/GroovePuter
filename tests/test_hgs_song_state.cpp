#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/harmony/harmonic_fingerprint.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/harmony/song_harmonic_state.h"

using namespace GroovePuterHarmony;

namespace {
SongHarmonicCandidate candidate(uint32_t transactionSeed) {
  SongHarmonicCandidate value{};
  value.songIdentity = 0x12345678u;
  value.songSlot = 1;
  value.startPosition = 4;
  value.rowCount = 1;
  value.projectSeed = 42;
  value.style = HarmonicStyleId::UkGarage;
  value.modelVersion = 7;
  value.corpusChecksum = 0x9c01d00du;
  value.genome = deriveSongVariantGenome(value.projectSeed,
                                         value.songIdentity,
                                         value.style,
                                         value.modelVersion);
  value.harmony.materialId = 100u + transactionSeed;
  value.harmony.publicationGeneration = 3;
  value.harmony.modelVersion = value.modelVersion;
  value.harmony.authority = HarmonicAuthority::Graph;
  value.harmony.eventCount = 2;
  const auto* major = qualityDefinition(kQualityMajor);
  const auto* dorian = scaleDefinition(2);
  for (uint8_t i = 0; i < value.harmony.eventCount; ++i) {
    const uint8_t root = i == 0 ? 0 : 7;
    auto& event = value.harmony.events[i];
    event.state = {kQualityMajor, transposePitchSet(major->intervalMask, root),
                   root, kNoBassPc};
    event.tonal = {2, 2, dorian->intervalMask, dorian->intervalMask, 0, 0};
    event.timingSlotOrdinal = static_cast<uint8_t>(i * 4);
  }
  value.home = value.harmony.events[0].state;
  value.homeTonal = value.harmony.events[0].tonal;
  value.terminal = value.harmony.events[1].state;
  value.terminalTonal = value.harmony.events[1].tonal;
  value.participantCount = 2;
  value.participants[0] = {0, 4, 0, {0xA101u}, {0x1111u, 0x2222u}};
  value.participants[1] = {1, 4, 0, {0xA102u}, {0x3333u, 0x4444u}};
  return value;
}
}  // namespace

int main() {
  SongHarmonicState state{};
  const SongHarmonicCandidate first = candidate(1);
  assert(validSongHarmonicCandidate(first));
  assert(commitSongHarmonicCandidate(state, first, 77) ==
         SongHarmonicCommitStatus::Committed);
  assert(state.songIdentity == first.songIdentity);
  assert(state.harmony.materialId == first.harmony.materialId);
  assert(state.terminal.rootPc == first.terminal.rootPc);
  assert(state.recent.count == 1);
  assert(state.lastTransactionId == 77);
  SongHarmonicParticipant relocated[2] = {
      state.participants[1], state.participants[0]};
  assert(songHarmonicParticipantsMatch(state, relocated, 2));
  relocated[0].version.low++;
  assert(!songHarmonicParticipantsMatch(state, relocated, 2));

  const SongHarmonicState committed = state;
  assert(commitSongHarmonicCandidate(state, first, 77) ==
         SongHarmonicCommitStatus::AlreadyCommitted);
  assert(state.recent.count == committed.recent.count);
  assert(state.recent.next == committed.recent.next);
  SongHarmonicCandidate reusedTransaction = candidate(2);
  assert(commitSongHarmonicCandidate(state, reusedTransaction, 77) ==
         SongHarmonicCommitStatus::Stale);
  assert(state.harmony.materialId == committed.harmony.materialId);

  SongHarmonicCandidate next = candidate(2);
  next.harmony.events[1].state.rootPc = 9;  // break route integrity
  assert(commitSongHarmonicCandidate(state, next, 78) ==
         SongHarmonicCommitStatus::Invalid);
  assert(state.harmony.materialId == committed.harmony.materialId);
  assert(state.lastTransactionId == committed.lastTransactionId);

  next = candidate(2);
  next.songIdentity++;
  assert(commitSongHarmonicCandidate(state, next, 78) ==
         SongHarmonicCommitStatus::Stale);
  assert(state.harmony.materialId == committed.harmony.materialId);

  next = candidate(2);
  next.harmony.materialId = 102;
  assert(commitSongHarmonicCandidate(state, next, 78) ==
         SongHarmonicCommitStatus::Committed);
  assert(state.recent.count == 2);  // accepted route appended exactly once
  assert(state.lastTransactionId == 78);

  SongHarmonicStateWindow window{};
  assert(commitSongHarmonicCandidate(window.active, first, 77) ==
         SongHarmonicCommitStatus::Committed);
  SongHarmonicCandidate following = candidate(3);
  following.startPosition = 5;
  following.participants[0].songPosition = 5;
  following.participants[1].songPosition = 5;
  following.harmony.events[1].state.rootPc = 9;
  following.harmony.events[1].state.pitchMask =
      transposePitchSet(qualityDefinition(kQualityMajor)->intervalMask, 9);
  following.terminal = following.harmony.events[1].state;
  assert(stageNextSongHarmonicState(window, following, 79) ==
         SongHarmonicCommitStatus::Committed);
  assert(window.active.startPosition == first.startPosition);
  assert(window.active.recent.count == 1);
  assert(window.next.startPosition == 5);
  assert(window.next.recent.count == 2);
  assert(stageNextSongHarmonicState(window, following, 79) ==
         SongHarmonicCommitStatus::AlreadyCommitted);
  assert(promoteNextSongHarmonicState(window));
  assert(window.active.startPosition == 5);
  assert(window.active.terminal.rootPc == 9);
  assert(window.active.recent.count == 2);
  assert(!validSongHarmonicState(window.next));

  std::printf("HGS song state commit/idempotence: PASS (state=%zu B)\n",
              sizeof(SongHarmonicState));
  return 0;
}
