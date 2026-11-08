#include "song_harmonic_state.h"

#include "harmonic_quality.h"
#include "harmonic_scale.h"

#include <cstring>

namespace GroovePuterHarmony {
namespace {

bool validState(const HarmonicState& state) {
  if (state.rootPc > 11 ||
      (state.bassPc != kNoBassPc && state.bassPc > 11)) return false;
  const ChordQualityDefinition* quality = qualityDefinition(state.qualityId);
  return quality != nullptr &&
         state.pitchMask == transposePitchSet(quality->intervalMask,
                                              state.rootPc);
}

bool validContext(const TonalContext& context) {
  if (context.tonicPc > 11 || context.homeTonicPc > 11) return false;
  const ScaleDefinition* scale = scaleDefinition(context.scaleId);
  const ScaleDefinition* home = scaleDefinition(context.homeScaleId);
  return scale != nullptr && home != nullptr &&
         context.scaleMask == transposePitchSet(scale->intervalMask,
                                                context.tonicPc) &&
         context.homeScaleMask == transposePitchSet(home->intervalMask,
                                                    context.homeTonicPc);
}

bool sameState(const HarmonicState& a, const HarmonicState& b) {
  return a.qualityId == b.qualityId && a.pitchMask == b.pitchMask &&
         a.rootPc == b.rootPc && a.bassPc == b.bassPc;
}

bool sameContext(const TonalContext& a, const TonalContext& b) {
  return a.scaleId == b.scaleId && a.homeScaleId == b.homeScaleId &&
         a.scaleMask == b.scaleMask && a.homeScaleMask == b.homeScaleMask &&
         a.tonicPc == b.tonicPc && a.homeTonicPc == b.homeTonicPc;
}

bool sameHome(const TonalContext& a, const TonalContext& b) {
  return a.homeScaleId == b.homeScaleId &&
         a.homeScaleMask == b.homeScaleMask &&
         a.homeTonicPc == b.homeTonicPc;
}

bool validCandidate(const SongHarmonicCandidate& candidate) {
  if (candidate.songIdentity == 0 || candidate.modelVersion == 0 ||
      candidate.songSlot > 1 || candidate.rowCount == 0 ||
      candidate.rowCount > 8 ||
      static_cast<uint16_t>(candidate.startPosition) + candidate.rowCount > 128 ||
      candidate.corpusChecksum == 0 || policyFor(candidate.style) == nullptr ||
      !validState(candidate.home) || !validContext(candidate.homeTonal) ||
      candidate.home.rootPc != candidate.homeTonal.homeTonicPc ||
      candidate.harmony.materialId == 0 ||
      candidate.harmony.publicationGeneration == 0 ||
      candidate.harmony.modelVersion != candidate.modelVersion ||
      candidate.harmony.authority != HarmonicAuthority::Graph ||
      candidate.harmony.eventCount == 0 ||
      candidate.harmony.eventCount > kMaxSongHarmonyEvents ||
      candidate.participantCount != candidate.rowCount * 2u ||
      candidate.participantCount > kMaxSongHarmonyParticipants ||
      !validState(candidate.terminal) ||
      !validContext(candidate.terminalTonal) ||
      !sameHome(candidate.homeTonal, candidate.terminalTonal)) {
    return false;
  }

  uint8_t previousTimingSlot = 0;
  for (uint8_t i = 0; i < candidate.harmony.eventCount; ++i) {
    const ResolvedHarmonicEvent& event = candidate.harmony.events[i];
    if (!validState(event.state) || !validContext(event.tonal) ||
        !sameHome(candidate.homeTonal, event.tonal) ||
        static_cast<uint8_t>(event.cadenceClass) >=
            static_cast<uint8_t>(CadenceClass::Count) ||
        (i != 0 && event.timingSlotOrdinal <= previousTimingSlot)) {
      return false;
    }
    previousTimingSlot = event.timingSlotOrdinal;
  }

  const ResolvedHarmonicEvent& last =
      candidate.harmony.events[candidate.harmony.eventCount - 1u];
  if (!sameState(candidate.terminal, last.state) ||
      !sameContext(candidate.terminalTonal, last.tonal)) {
    return false;
  }

  for (uint8_t i = 0; i < candidate.participantCount; ++i) {
    const SongHarmonicParticipant& participant = candidate.participants[i];
    if (participant.track > 1 || participant.reserved != 0 ||
        participant.songPosition < candidate.startPosition ||
        participant.songPosition >=
            candidate.startPosition + candidate.rowCount ||
        !participant.materialId.valid() || !participant.version.valid()) {
      return false;
    }
    for (uint8_t j = 0; j < i; ++j) {
      const SongHarmonicParticipant& previous = candidate.participants[j];
      if (participant.track == previous.track &&
          participant.songPosition == previous.songPosition) return false;
    }
  }

  for (uint8_t row = 0; row < candidate.rowCount; ++row) {
    for (uint8_t track = 0; track < 2; ++track) {
      bool found = false;
      const uint8_t position =
          static_cast<uint8_t>(candidate.startPosition + row);
      for (uint8_t i = 0; i < candidate.participantCount; ++i) {
        const auto& participant = candidate.participants[i];
        if (participant.songPosition == position &&
            participant.track == track) {
          found = true;
          break;
        }
      }
      if (!found) return false;
    }
  }
  return true;
}

bool sameParticipant(const SongHarmonicParticipant& a,
                     const SongHarmonicParticipant& b) {
  return a.track == b.track && a.songPosition == b.songPosition &&
         a.reserved == b.reserved && a.materialId == b.materialId &&
         a.version == b.version;
}

bool sameCommittedCandidate(const SongHarmonicState& state,
                            const SongHarmonicCandidate& candidate) {
  if (state.songIdentity != candidate.songIdentity ||
      state.projectSeed != candidate.projectSeed ||
      state.songSlot != candidate.songSlot ||
      state.startPosition != candidate.startPosition ||
      state.rowCount != candidate.rowCount ||
      state.style != candidate.style ||
      state.modelVersion != candidate.modelVersion ||
      state.corpusChecksum != candidate.corpusChecksum ||
      std::memcmp(&state.genome, &candidate.genome,
                  sizeof(SongVariantGenome)) != 0 ||
      !sameState(state.home, candidate.home) ||
      !sameContext(state.homeTonal, candidate.homeTonal) ||
      state.harmony.materialId != candidate.harmony.materialId ||
      state.harmony.publicationGeneration !=
          candidate.harmony.publicationGeneration ||
      state.harmony.modelVersion != candidate.harmony.modelVersion ||
      state.harmony.authority != candidate.harmony.authority ||
      state.harmony.eventCount != candidate.harmony.eventCount ||
      !sameState(state.terminal, candidate.terminal) ||
      !sameContext(state.terminalTonal, candidate.terminalTonal) ||
      state.participantCount != candidate.participantCount) {
    return false;
  }
  for (uint8_t i = 0; i < candidate.harmony.eventCount; ++i) {
    const auto& a = state.harmony.events[i];
    const auto& b = candidate.harmony.events[i];
    if (!sameState(a.state, b.state) || !sameContext(a.tonal, b.tonal) ||
        a.relationTags != b.relationTags ||
        a.cadenceClass != b.cadenceClass ||
        a.timingSlotOrdinal != b.timingSlotOrdinal) return false;
  }
  for (uint8_t i = 0; i < candidate.participantCount; ++i) {
    if (!sameParticipant(state.participants[i], candidate.participants[i]))
      return false;
  }
  return true;
}

}  // namespace

bool validSongHarmonicCandidate(const SongHarmonicCandidate& candidate) {
  return validCandidate(candidate);
}

bool validSongHarmonicState(const SongHarmonicState& state) {
  if (state.lastTransactionId == 0 ||
      state.lastTransactionMaterialId != state.harmony.materialId) return false;
  SongHarmonicCandidate candidate{};
  candidate.songIdentity = state.songIdentity;
  candidate.songSlot = state.songSlot;
  candidate.startPosition = state.startPosition;
  candidate.rowCount = state.rowCount;
  candidate.projectSeed = state.projectSeed;
  candidate.style = state.style;
  candidate.modelVersion = state.modelVersion;
  candidate.corpusChecksum = state.corpusChecksum;
  candidate.genome = state.genome;
  candidate.home = state.home;
  candidate.homeTonal = state.homeTonal;
  candidate.harmony = state.harmony;
  candidate.terminal = state.terminal;
  candidate.terminalTonal = state.terminalTonal;
  candidate.participantCount = state.participantCount;
  for (uint8_t i = 0; i < state.participantCount &&
                      i < kMaxSongHarmonyParticipants; ++i) {
    candidate.participants[i] = state.participants[i];
  }
  if (!validCandidate(candidate) || state.recent.count > 8 ||
      state.recent.next > 7 || state.recent.edgeCount > 8 ||
      state.recent.edgeNext > 7) return false;
  return true;
}

bool songHarmonicParticipantsMatch(
    const SongHarmonicState& state,
    const SongHarmonicParticipant* currentParticipants,
    uint8_t currentParticipantCount) {
  if (!validSongHarmonicState(state) || currentParticipants == nullptr ||
      currentParticipantCount != state.participantCount) return false;
  for (uint8_t i = 0; i < state.participantCount; ++i) {
    bool found = false;
    for (uint8_t j = 0; j < currentParticipantCount; ++j) {
      if (sameParticipant(state.participants[i], currentParticipants[j])) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

SongHarmonicCommitStatus commitSongHarmonicCandidate(
    SongHarmonicState& state, const SongHarmonicCandidate& candidate,
    uint32_t transactionId) {
  if (transactionId == 0 || !validCandidate(candidate))
    return SongHarmonicCommitStatus::Invalid;
  if (state.lastTransactionId == transactionId) {
    return state.lastTransactionMaterialId == candidate.harmony.materialId &&
                   sameCommittedCandidate(state, candidate)
               ? SongHarmonicCommitStatus::AlreadyCommitted
               : SongHarmonicCommitStatus::Stale;
  }
  if (state.songIdentity != 0 && state.songIdentity != candidate.songIdentity)
    return SongHarmonicCommitStatus::Stale;

  SongHarmonicState next{};
  next.songIdentity = candidate.songIdentity;
  next.songSlot = candidate.songSlot;
  next.startPosition = candidate.startPosition;
  next.rowCount = candidate.rowCount;
  next.projectSeed = candidate.projectSeed;
  next.style = candidate.style;
  next.modelVersion = candidate.modelVersion;
  next.corpusChecksum = candidate.corpusChecksum;
  next.genome = candidate.genome;
  next.home = candidate.home;
  next.homeTonal = candidate.homeTonal;
  next.harmony = candidate.harmony;
  next.terminal = candidate.terminal;
  next.terminalTonal = candidate.terminalTonal;
  next.lastTransactionId = transactionId;
  next.lastTransactionMaterialId = candidate.harmony.materialId;
  next.participantCount = candidate.participantCount;
  for (uint8_t i = 0; i < candidate.participantCount; ++i)
    next.participants[i] = candidate.participants[i];
  if (state.songIdentity == candidate.songIdentity) next.recent = state.recent;

  const ResolvedRouteView route{
      candidate.harmony.events, candidate.harmony.eventCount,
      candidate.home, candidate.homeTonal};
  if (commitAcceptedRoute(next.recent, route) != HarmonyStatus::Ready)
    return SongHarmonicCommitStatus::Invalid;
  state = next;
  return SongHarmonicCommitStatus::Committed;
}

SongHarmonicCommitStatus stageNextSongHarmonicState(
    SongHarmonicStateWindow& window, const SongHarmonicCandidate& candidate,
    uint32_t transactionId) {
  if (transactionId == 0 || !validCandidate(candidate))
    return SongHarmonicCommitStatus::Invalid;
  if (window.next.songIdentity != 0) {
    if (!validSongHarmonicState(window.next))
      return SongHarmonicCommitStatus::Invalid;
    if (window.next.lastTransactionId != transactionId)
      return SongHarmonicCommitStatus::Stale;
    return commitSongHarmonicCandidate(window.next, candidate, transactionId);
  }

  if (window.active.songIdentity != 0) {
    if (!validSongHarmonicState(window.active) ||
        candidate.songIdentity != window.active.songIdentity ||
        candidate.songSlot != window.active.songSlot ||
        candidate.projectSeed != window.active.projectSeed ||
        candidate.style != window.active.style ||
        candidate.modelVersion != window.active.modelVersion ||
        candidate.corpusChecksum != window.active.corpusChecksum ||
        candidate.startPosition !=
            window.active.startPosition + window.active.rowCount ||
        !sameState(candidate.home, window.active.home) ||
        !sameContext(candidate.homeTonal, window.active.homeTonal)) {
      return SongHarmonicCommitStatus::Stale;
    }
    SongHarmonicState staged = window.active;
    const SongHarmonicCommitStatus status = commitSongHarmonicCandidate(
        staged, candidate, transactionId);
    if (status == SongHarmonicCommitStatus::Committed ||
        status == SongHarmonicCommitStatus::AlreadyCommitted) {
      window.next = staged;
    }
    return status;
  }

  SongHarmonicState staged{};
  const SongHarmonicCommitStatus status = commitSongHarmonicCandidate(
      staged, candidate, transactionId);
  if (status == SongHarmonicCommitStatus::Committed ||
      status == SongHarmonicCommitStatus::AlreadyCommitted) {
    window.next = staged;
  }
  return status;
}

bool promoteNextSongHarmonicState(SongHarmonicStateWindow& window) {
  if (!validSongHarmonicState(window.next)) return false;
  window.active = window.next;
  window.next = SongHarmonicState{};
  return true;
}

}  // namespace GroovePuterHarmony
