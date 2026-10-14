#include "../platform_sdl/arduino_compat.h"
#include "../src/audio/pattern_paging.h"
#include "../src/generation/harmony/harmonic_quality.h"
#include "../src/generation/harmony/harmonic_scale.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>

SerialMock Serial;
SDMock SD;

namespace {
using namespace GroovePuterHarmony;

SongHarmonicState makeState(uint32_t transaction, uint32_t routeId,
                            uint32_t materialId, uint32_t versionLow) {
  SongHarmonicCandidate candidate{};
  candidate.songIdentity = 0x10203040u;
  candidate.songSlot = 1;
  candidate.startPosition = 5;
  candidate.rowCount = 1;
  candidate.projectSeed = 42;
  candidate.style = HarmonicStyleId::DeepHouse;
  candidate.modelVersion = 7;
  candidate.corpusChecksum = 0x123456789abcdef0ull;
  candidate.genome = deriveSongVariantGenome(
      candidate.projectSeed, candidate.songIdentity, candidate.style,
      candidate.modelVersion);
  candidate.harmony.materialId = routeId;
  candidate.harmony.publicationGeneration = transaction;
  candidate.harmony.modelVersion = candidate.modelVersion;
  candidate.harmony.authority = HarmonicAuthority::Graph;
  candidate.harmony.eventCount = 1;
  const auto* quality = qualityDefinition(kQualityMajor);
  const auto* scale = scaleDefinition(2);
  auto& event = candidate.harmony.events[0];
  event.state = {kQualityMajor, quality->intervalMask, 0, kNoBassPc};
  event.tonal = {2, 2, scale->intervalMask, scale->intervalMask, 0, 0};
  event.timingSlotOrdinal = 0;
  candidate.home = event.state;
  candidate.homeTonal = event.tonal;
  candidate.terminal = event.state;
  candidate.terminalTonal = event.tonal;
  candidate.participantCount = 2;
  candidate.participants[0] = {0, 5, 0, {materialId}, {versionLow, 7}};
  candidate.participants[1] = {1, 5, 0, {materialId + 1}, {versionLow + 1, 7}};
  SongHarmonicState state{};
  assert(commitSongHarmonicCandidate(state, candidate, transaction) ==
         SongHarmonicCommitStatus::Committed);
  return state;
}
}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    "grooveputer-hgs-song-storage";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  SD.setRoot(root);

  assert(PatternPagingService::setProjectName("hgs-song-src"));
  assert(PatternPagingService::clearProjectPages());
  const SongHarmonicState oldState = makeState(1, 500, 600, 0x12340000u);
  SongHarmonicState newState = makeState(2, 501, 700, 0x56780000u);
  std::swap(newState.participants[0], newState.participants[1]);
  assert(PatternPagingService::saveSongHarmonicState(oldState) ==
         SongHarmonicStorageStatus::Ok);
  assert(PatternPagingService::saveSongHarmonicState(newState) ==
         SongHarmonicStorageStatus::Ok);
  assert(PatternPagingService::saveSongHarmonicState(oldState) ==
         SongHarmonicStorageStatus::Ok);
  const SongHarmonicState conflicting = makeState(3, 502, 600, 0x12340000u);
  assert(PatternPagingService::saveSongHarmonicState(conflicting) ==
         SongHarmonicStorageStatus::InvalidInput);

  SongHarmonicState loaded{};
  assert(PatternPagingService::loadSongHarmonicState(
             1, 5, oldState.participants,
             oldState.participantCount, loaded) ==
         SongHarmonicStorageStatus::Ok);
  assert(loaded.harmony.materialId == oldState.harmony.materialId);
  assert(PatternPagingService::loadSongHarmonicState(
             1, 5, newState.participants,
             newState.participantCount, loaded) ==
         SongHarmonicStorageStatus::Ok);
  assert(loaded.harmony.materialId == newState.harmony.materialId);
  SongHarmonicState restoredAfterUndo{};
  assert(PatternPagingService::loadSongHarmonicState(
             1, 5, oldState.participants,
             oldState.participantCount, restoredAfterUndo) ==
         SongHarmonicStorageStatus::Ok);
  assert(restoredAfterUndo.harmony.materialId == oldState.harmony.materialId);
  SongHarmonicParticipant editedParticipants[2] = {
      newState.participants[0], newState.participants[1]};
  editedParticipants[0].version.low++;
  SongHarmonicState unchangedByStaleLookup = oldState;
  assert(PatternPagingService::loadSongHarmonicState(
             1, 5, editedParticipants, 2, unchangedByStaleLookup) ==
         SongHarmonicStorageStatus::InvalidData);
  assert(unchangedByStaleLookup.harmony.materialId == oldState.harmony.materialId);

  const auto sourceDirectory = root / "patterns" / "hgs-song-src";
  bool corruptedOldRecord = false;
  for (const auto& entry : std::filesystem::directory_iterator(sourceDirectory)) {
    if (entry.path().filename().string().find("m00000258_") ==
        std::string::npos) {
      continue;
    }
    FILE* file = std::fopen(entry.path().string().c_str(), "r+b");
    assert(file != nullptr);
    assert(std::fseek(file, 8, SEEK_SET) == 0);
    const int byte = std::fgetc(file);
    assert(byte != EOF);
    assert(std::fseek(file, 8, SEEK_SET) == 0);
    assert(std::fputc(byte ^ 0x40, file) != EOF);
    std::fclose(file);
    corruptedOldRecord = true;
    break;
  }
  assert(corruptedOldRecord);
  SongHarmonicState unchanged = newState;
  assert(PatternPagingService::loadSongHarmonicState(
             1, 5, oldState.participants,
             oldState.participantCount, unchanged) ==
         SongHarmonicStorageStatus::Corrupt);
  assert(unchanged.harmony.materialId == newState.harmony.materialId);

  assert(PatternPagingService::copyProjectPages("hgs-song-src",
                                                 "hgs-song-copy"));
  assert(PatternPagingService::setProjectName("hgs-song-copy"));
  assert(PatternPagingService::loadSongHarmonicState(
             1, 5, newState.participants,
             newState.participantCount, loaded) ==
         SongHarmonicStorageStatus::Ok);
  assert(loaded.harmony.materialId == newState.harmony.materialId);

  assert(PatternPagingService::clearProjectPages("hgs-song-copy"));
  assert(PatternPagingService::loadSongHarmonicState(
             1, 5, newState.participants,
             newState.participantCount, loaded) ==
         SongHarmonicStorageStatus::NotFound);
  std::filesystem::remove_all(root);
  std::puts("HGS project storage/placement/version history: PASS");
  return 0;
}
