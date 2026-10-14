#ifndef GROOVEPUTER_GENERATION_HARMONY_SONG_HARMONIC_STATE_H
#define GROOVEPUTER_GENERATION_HARMONY_SONG_HARMONIC_STATE_H

#include <cstdint>
#include <type_traits>

#include "harmonic_fingerprint.h"
#include "harmonic_source.h"
#include "genre_harmonic_policy.h"
#include "song_variant_genome.h"
#include "src/state/material_identity.h"
#include "src/state/material_version_token.h"

namespace GroovePuterHarmony {

constexpr uint8_t kMaxSongHarmonyEvents = 32;
constexpr uint8_t kMaxSongHarmonyParticipants = 16;

struct SongHarmonicParticipant {
  uint8_t track = 0;
  uint8_t songPosition = 0;
  uint8_t reserved = 0;
  GroovePuterMaterial::MaterialId materialId{};
  GroovePuterMaterial::MaterialVersionToken version{};
};

struct SongHarmonicCandidate {
  uint32_t songIdentity = 0;
  uint8_t songSlot = 0;
  uint8_t startPosition = 0;
  uint8_t rowCount = 0;
  uint32_t projectSeed = 0;
  HarmonicStyleId style = HarmonicStyleId::Acid;
  uint16_t modelVersion = 0;
  uint64_t corpusChecksum = 0;
  SongVariantGenome genome{};
  HarmonicState home{};
  TonalContext homeTonal{};
  PreparedGraphHarmony harmony{};
  HarmonicState terminal{};
  TonalContext terminalTonal{};
  uint8_t participantCount = 0;
  SongHarmonicParticipant participants[kMaxSongHarmonyParticipants]{};
};

struct SongHarmonicState {
  uint32_t songIdentity = 0;
  uint8_t songSlot = 0;
  uint8_t startPosition = 0;
  uint8_t rowCount = 0;
  uint32_t projectSeed = 0;
  HarmonicStyleId style = HarmonicStyleId::Acid;
  uint16_t modelVersion = 0;
  uint64_t corpusChecksum = 0;
  SongVariantGenome genome{};
  HarmonicState home{};
  TonalContext homeTonal{};
  PreparedGraphHarmony harmony{};
  HarmonicState terminal{};
  TonalContext terminalTonal{};
  RecentHarmonyMemory recent{};
  uint32_t lastTransactionId = 0;
  uint32_t lastTransactionMaterialId = 0;
  uint8_t participantCount = 0;
  SongHarmonicParticipant participants[kMaxSongHarmonyParticipants]{};
};

struct SongHarmonicStateWindow {
  SongHarmonicState active{};
  SongHarmonicState next{};
};

enum class SongHarmonicCommitStatus : uint8_t {
  Committed = 0,
  AlreadyCommitted,
  Stale,
  Invalid,
};

bool validSongHarmonicCandidate(const SongHarmonicCandidate& candidate);
bool validSongHarmonicState(const SongHarmonicState& state);
bool songHarmonicParticipantsMatch(
    const SongHarmonicState& state,
    const SongHarmonicParticipant* currentParticipants,
    uint8_t currentParticipantCount);
SongHarmonicCommitStatus commitSongHarmonicCandidate(
    SongHarmonicState& state, const SongHarmonicCandidate& candidate,
    uint32_t transactionId);
SongHarmonicCommitStatus stageNextSongHarmonicState(
    SongHarmonicStateWindow& window, const SongHarmonicCandidate& candidate,
    uint32_t transactionId);
bool promoteNextSongHarmonicState(SongHarmonicStateWindow& window);

static_assert(std::is_trivially_copyable<SongHarmonicParticipant>::value,
              "harmonic material participants must be fixed values");
static_assert(std::is_trivially_copyable<SongHarmonicCandidate>::value,
              "harmonic route candidates must be fixed values");
static_assert(std::is_trivially_copyable<SongHarmonicState>::value,
              "song harmonic state must be a fixed value");
static_assert(sizeof(SongHarmonicState) * 2u <= 4096u,
              "active and next song harmony windows exceed resident budget");
static_assert(sizeof(SongHarmonicStateWindow) <= 4096u,
              "active/next metadata window exceeded resident budget");

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_SONG_HARMONIC_STATE_H
