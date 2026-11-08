#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/harmony/harmonic_source_codec.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"

using namespace GroovePuterHarmony;

namespace {
SongHarmonicState makeState() {
  SongHarmonicCandidate candidate{};
  candidate.songIdentity = 0x76543210u;
  candidate.songSlot = 0;
  candidate.startPosition = 0;
  candidate.rowCount = 8;
  candidate.projectSeed = 0x11223344u;
  candidate.style = HarmonicStyleId::Ambient;
  candidate.modelVersion = 9;
  candidate.corpusChecksum = 0x0123456789abcdefull;
  candidate.genome = deriveSongVariantGenome(
      candidate.projectSeed, candidate.songIdentity, candidate.style,
      candidate.modelVersion);
  candidate.harmony.materialId = 400;
  candidate.harmony.publicationGeneration = 2;
  candidate.harmony.modelVersion = candidate.modelVersion;
  candidate.harmony.authority = HarmonicAuthority::Graph;
  candidate.harmony.eventCount = kMaxSongHarmonyEvents;
  const auto* quality = qualityDefinition(kQualityMajor7);
  const auto* scale = scaleDefinition(2);
  for (uint8_t i = 0; i < candidate.harmony.eventCount; ++i) {
    auto& event = candidate.harmony.events[i];
    const uint8_t root = static_cast<uint8_t>(i % 12u);
    event.state = {kQualityMajor7,
                   transposePitchSet(quality->intervalMask, root), root,
                   kNoBassPc};
    event.tonal = {2, 2, scale->intervalMask, scale->intervalMask, 0, 0};
    event.relationTags = static_cast<uint16_t>(0x1200u + i);
    event.cadenceClass = CadenceClass::HoldHome;
    event.timingSlotOrdinal = i;
  }
  candidate.home = candidate.harmony.events[0].state;
  candidate.homeTonal = candidate.harmony.events[0].tonal;
  candidate.terminal = candidate.harmony.events[31].state;
  candidate.terminalTonal = candidate.harmony.events[31].tonal;
  candidate.participantCount = kMaxSongHarmonyParticipants;
  for (uint8_t i = 0; i < candidate.participantCount; ++i) {
    candidate.participants[i] = {
        static_cast<uint8_t>(i / 8u), static_cast<uint8_t>(i % 8u), 0,
        {static_cast<uint32_t>(0xABCDu + i)},
        {static_cast<uint32_t>(0x10203040u + i),
         static_cast<uint32_t>(0x50607080u + i)}};
  }
  SongHarmonicState state{};
  assert(commitSongHarmonicCandidate(state, candidate, 12) ==
         SongHarmonicCommitStatus::Committed);
  for (uint32_t i = 1; i < 8; ++i) {
    candidate.harmony.materialId = 400u + i;
    candidate.harmony.publicationGeneration = 2u + i;
    assert(commitSongHarmonicCandidate(state, candidate, 12u + i) ==
           SongHarmonicCommitStatus::Committed);
  }
  return state;
}
}  // namespace

int main() {
  const SongHarmonicState original = makeState();
  uint8_t bytes[kMaxSongHarmonicStateBytes]{};
  size_t written = 0;
  assert(encodeSongHarmonicState(original, bytes, sizeof(bytes), written) ==
         SongHarmonicCodecStatus::Ok);
  assert(written > 32 && written <= sizeof(bytes));

  SongHarmonicState decoded{};
  assert(decodeSongHarmonicState(bytes, written, decoded) ==
         SongHarmonicCodecStatus::Ok);
  assert(decoded.songIdentity == original.songIdentity);
  assert(decoded.style == original.style);
  assert(decoded.modelVersion == original.modelVersion);
  assert(decoded.corpusChecksum == original.corpusChecksum);
  assert(decoded.genome.verticalColor == original.genome.verticalColor);
  assert(decoded.harmony.events[0].state.qualityId == kQualityMajor7);
  assert(decoded.harmony.events[0].relationTags == 0x1200);
  assert(decoded.harmony.eventCount == kMaxSongHarmonyEvents);
  assert(decoded.participantCount == kMaxSongHarmonyParticipants);
  assert(decoded.recent.count == 8);
  assert(decoded.participants[0].materialId == original.participants[0].materialId);
  assert(decoded.participants[0].version == original.participants[0].version);
  assert(decoded.recent.count == original.recent.count);

  SongHarmonicState sentinel = original;
  sentinel.songIdentity = 0xdeadbeefu;
  const SongHarmonicState before = sentinel;
  uint8_t corrupt[kMaxSongHarmonicStateBytes]{};
  for (size_t i = 0; i < written; ++i) corrupt[i] = bytes[i];
  corrupt[written - 1u] ^= 0x80u;
  assert(decodeSongHarmonicState(corrupt, written, sentinel) ==
         SongHarmonicCodecStatus::Corrupt);
  assert(sentinel.songIdentity == before.songIdentity);

  corrupt[written - 1u] = bytes[written - 1u];
  corrupt[4] = 0xff;
  corrupt[5] = 0x7f;
  assert(decodeSongHarmonicState(corrupt, written, sentinel) ==
         SongHarmonicCodecStatus::UnsupportedVersion);
  assert(sentinel.songIdentity == before.songIdentity);

  size_t tooSmallWritten = 99;
  assert(encodeSongHarmonicState(original, bytes, written - 1u,
                                 tooSmallWritten) ==
         SongHarmonicCodecStatus::BufferTooSmall);
  assert(tooSmallWritten == 0);
  std::printf("HGS persistence codec: PASS (%zu bytes, state=%zu B)\n", written,
              sizeof(SongHarmonicState));
  return 0;
}
