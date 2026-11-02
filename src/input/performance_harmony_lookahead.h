#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_HARMONY_LOOKAHEAD_H
#define GROOVEPUTER_PERFORMANCE_HARMONY_LOOKAHEAD_H

#include <cstdint>

#include "performance_harmony_bridge.h"

// CALL/RESPONSE phase 2: the harmony of a step slightly ahead of the published
// playback position (e.g. the next bar, absoluteStep + 16), built only from
// data that is already in memory: the generated phrase origin/recipe, the Song
// participants captured from the in-memory Song table and, for Graph harmony,
// the SongHarmonicState already in the route cache. No file system access.
//
// The shifted position is then fed to the existing make*HarmonySnapshot(), so
// the lookahead answer is, by construction, the snapshot the bridge will build
// when playback reaches that step (if the Song still plays that material).
namespace GroovePuterPerformance {

// A row past lastSequentialRow is not reachable by plain +1 row steps from the
// current row (end of Song, loop end, reverse playback): no lookahead there.
// The caller derives it from the engine: reverse -> current row; loop mode with
// the row inside the loop -> loop end; otherwise Song length - 1.
inline bool shiftHarmonyPosition(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const GroovePuterHarmony::SongHarmonicParticipant* participants,
    uint8_t participantCount,
    const HarmonyPlaybackPosition& now,
    int32_t offsetSteps,
    int16_t lastSequentialRow,
    HarmonyPlaybackPosition& out) {
  out = now;
  if (!origin.valid() || !recipe.valid || recipe.songStart < 0 ||
      now.songRow < recipe.songStart || now.songRow >= recipe.songStart + recipe.bars)
    return false;
  const int64_t target = static_cast<int64_t>(now.absoluteStep) + offsetSteps;
  if (target < 0) return false;
  const int64_t nowBar = static_cast<int64_t>(now.absoluteStep / 16u);
  const int64_t rows = target / 16 - nowBar;
  if (rows < 0 || rows > 2) return false;  // never behind the published row
  out.absoluteStep = static_cast<uint64_t>(target);
  if (rows == 0) return true;

  const int32_t row = now.songRow + static_cast<int32_t>(rows);
  if (row > lastSequentialRow || row >= recipe.songStart + recipe.bars || row > 127)
    return false;
  const uint8_t ordinal = static_cast<uint8_t>(row - recipe.songStart);
  if (ordinal >= origin.common.barCount) return false;
  const auto& bar = origin.bars[ordinal];
  // The Song row must still play the generated material of that bar.
  bool confirmed = false;
  for (uint8_t i = 0; participants && i < participantCount; ++i) {
    const auto& participant = participants[i];
    if (participant.track != 0 || participant.songPosition != row) continue;
    if (participant.materialId != bar.material.id ||
        participant.version != bar.originPatternVersion) return false;
    confirmed = true;
  }
  if (!confirmed) return false;
  out.songRow = static_cast<int16_t>(row);
  out.activeSynthAPatternGlobal = static_cast<int16_t>(bar.material.address.globalSlot);
  out.activeMaterial = {bar.material.address, bar.material.id};
  out.activeMaterialVersion = bar.originPatternVersion;
  return true;
}

inline bool legacyHarmonyAhead(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const GroovePuterHarmony::SongHarmonicParticipant* participants,
    uint8_t participantCount, const HarmonyPlaybackPosition& now,
    int32_t offsetSteps, int16_t lastSequentialRow,
    GroovePuterHarmony::HarmonySnapshot& out) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  HarmonyPlaybackPosition shifted{};
  if (!shiftHarmonyPosition(origin, recipe, participants, participantCount, now,
                            offsetSteps, lastSequentialRow, shifted)) return false;
  return makeLegacyHarmonySnapshot(origin, recipe, shifted, out);
}

inline bool graphHarmonyAhead(
    const GroovePuterHarmony::SongHarmonicState& state,
    const GroovePuterHarmony::SongHarmonicParticipant* participants,
    uint8_t participantCount,
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const HarmonyPlaybackPosition& now, int32_t offsetSteps,
    int16_t lastSequentialRow, GroovePuterHarmony::HarmonySnapshot& out) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  HarmonyPlaybackPosition shifted{};
  if (!shiftHarmonyPosition(origin, recipe, participants, participantCount, now,
                            offsetSteps, lastSequentialRow, shifted)) return false;
  return makeGraphHarmonySnapshot(state, participants, participantCount, origin,
                                  recipe, shifted, out);
}

// Pattern mode: the generated pattern loops every bar, so a step ahead is the
// same pattern at a later absolute step (a queued pattern change is already
// refused by tryCapturePatternHarmonyPlaybackPosition).
inline bool patternHarmonyAhead(
    const GroovePuterMaterial::GeneratedPatternHarmonyOrigin& origin,
    const PatternHarmonyPlaybackPosition& now, int32_t offsetSteps,
    GroovePuterHarmony::HarmonySnapshot& out) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  const int64_t target = static_cast<int64_t>(now.absoluteStep) + offsetSteps;
  if (target < 0 || offsetSteps < 0 || offsetSteps > 48) return false;
  PatternHarmonyPlaybackPosition shifted = now;
  shifted.absoluteStep = static_cast<uint64_t>(target);
  return makePatternHarmonySnapshot(origin, shifted, out);
}

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_HARMONY_LOOKAHEAD_H
