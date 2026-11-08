#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_HARMONY_ROUTE_CACHE_H
#define GROOVEPUTER_PERFORMANCE_HARMONY_ROUTE_CACHE_H

#include <cstdint>
#include <string>

#include "../generation/harmony/song_harmonic_state.h"

namespace GroovePuterPerformance {

// Control-loop cache for SD-loaded route state. Never read from the audio
// renderer. The dynamic project-name copy is only updated on project changes.
class PerformanceHarmonyRouteCache {
 public:
  static constexpr uint8_t kMaxParticipants =
      GroovePuterHarmony::kMaxSongHarmonyParticipants;

  bool shouldLookup(const std::string& projectName, uint8_t songSlot,
                    uint8_t startPosition,
                    const GroovePuterHarmony::SongHarmonicParticipant* participants,
                    uint8_t participantCount, uint32_t nowMs) const {
    if (!sameRequest(projectName, songSlot, startPosition, participants,
                     participantCount)) return true;
    return transientFailure_ &&
           static_cast<uint32_t>(nowMs - lastAttemptMs_) >= 1000u;
  }

  void rememberResult(
      const std::string& projectName, uint8_t songSlot,
      uint8_t startPosition,
      const GroovePuterHarmony::SongHarmonicParticipant* participants,
      uint8_t participantCount, uint32_t nowMs, bool loaded,
      bool transientFailure,
      const GroovePuterHarmony::SongHarmonicState* state) {
    projectName_ = projectName;
    songSlot_ = songSlot;
    startPosition_ = startPosition;
    participantCount_ = participantCount > kMaxParticipants
        ? kMaxParticipants : participantCount;
    for (uint8_t i = 0; i < participantCount_; ++i)
      participants_[i] = participants[i];
    loaded_ = loaded && state != nullptr;
    transientFailure_ = !loaded_ && transientFailure;
    lastAttemptMs_ = nowMs;
    state_ = loaded_ ? *state : GroovePuterHarmony::SongHarmonicState{};
  }

  const GroovePuterHarmony::SongHarmonicState* stateFor(
      const std::string& projectName, uint8_t songSlot,
      uint8_t startPosition,
      const GroovePuterHarmony::SongHarmonicParticipant* participants,
      uint8_t participantCount) const {
    if (!loaded_ || !sameRequest(projectName, songSlot, startPosition,
                                 participants, participantCount) ||
        !GroovePuterHarmony::songHarmonicParticipantsMatch(
            state_, participants, participantCount)) return nullptr;
    return &state_;
  }

 private:
  static bool sameParticipant(
      const GroovePuterHarmony::SongHarmonicParticipant& a,
      const GroovePuterHarmony::SongHarmonicParticipant& b) {
    return a.track == b.track && a.songPosition == b.songPosition &&
           a.reserved == b.reserved && a.materialId == b.materialId &&
           a.version == b.version;
  }

  bool sameRequest(
      const std::string& projectName, uint8_t songSlot,
      uint8_t startPosition,
      const GroovePuterHarmony::SongHarmonicParticipant* participants,
      uint8_t participantCount) const {
    if (projectName_ != projectName || songSlot_ != songSlot ||
        startPosition_ != startPosition ||
        participantCount_ != participantCount || participants == nullptr ||
        participantCount > kMaxParticipants) return false;
    for (uint8_t i = 0; i < participantCount; ++i) {
      if (!sameParticipant(participants_[i], participants[i])) return false;
    }
    return true;
  }

  std::string projectName_{};
  GroovePuterHarmony::SongHarmonicState state_{};
  GroovePuterHarmony::SongHarmonicParticipant participants_[kMaxParticipants]{};
  uint32_t lastAttemptMs_ = 0;
  uint8_t songSlot_ = 0xff;
  uint8_t startPosition_ = 0xff;
  uint8_t participantCount_ = 0;
  bool loaded_ = false;
  bool transientFailure_ = false;
};

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_HARMONY_ROUTE_CACHE_H
