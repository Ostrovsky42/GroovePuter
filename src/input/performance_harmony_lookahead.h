#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_HARMONY_LOOKAHEAD_H
#define GROOVEPUTER_PERFORMANCE_HARMONY_LOOKAHEAD_H

#include <cstdint>

#include "performance_harmony_bridge.h"
#include "../generation/idiom/genre_idiom.h"

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
//
// maxRowsAhead: 2 for the 1- and 2-bar answer (phases 2-3); a 4-bar answer
// passes 4 (CALL/RESPONSE phase 4). The same participant check applies to
// every row, so a Song row edited to other material is still refused.
inline bool shiftHarmonyPosition(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const GroovePuterHarmony::SongHarmonicParticipant* participants,
    uint8_t participantCount,
    const HarmonyPlaybackPosition& now,
    int32_t offsetSteps,
    int16_t lastSequentialRow,
    HarmonyPlaybackPosition& out,
    int32_t maxRowsAhead = 2) {
  out = now;
  if (!origin.valid() || !recipe.valid || recipe.songStart < 0 ||
      now.songRow < recipe.songStart || now.songRow >= recipe.songStart + recipe.bars)
    return false;
  const int64_t target = static_cast<int64_t>(now.absoluteStep) + offsetSteps;
  if (target < 0) return false;
  const int64_t nowBar = static_cast<int64_t>(now.absoluteStep / 16u);
  const int64_t rows = target / 16 - nowBar;
  if (rows < 0 || rows > maxRowsAhead) return false;  // never behind the published row
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
    GroovePuterHarmony::HarmonySnapshot& out, int32_t maxRowsAhead = 2) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  HarmonyPlaybackPosition shifted{};
  if (!shiftHarmonyPosition(origin, recipe, participants, participantCount, now,
                            offsetSteps, lastSequentialRow, shifted, maxRowsAhead)) return false;
  return makeLegacyHarmonySnapshot(origin, recipe, shifted, out);
}

inline bool graphHarmonyAhead(
    const GroovePuterHarmony::SongHarmonicState& state,
    const GroovePuterHarmony::SongHarmonicParticipant* participants,
    uint8_t participantCount,
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const HarmonyPlaybackPosition& now, int32_t offsetSteps,
    int16_t lastSequentialRow, GroovePuterHarmony::HarmonySnapshot& out,
    int32_t maxRowsAhead = 2) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  HarmonyPlaybackPosition shifted{};
  if (!shiftHarmonyPosition(origin, recipe, participants, participantCount, now,
                            offsetSteps, lastSequentialRow, shifted, maxRowsAhead)) return false;
  return makeGraphHarmonySnapshot(state, participants, participantCount, origin,
                                  recipe, shifted, out);
}

// Pattern mode: the generated pattern loops every bar, so a step ahead is the
// same pattern at a later absolute step (a queued pattern change is already
// refused by tryCapturePatternHarmonyPlaybackPosition). maxOffsetSteps: 48 for
// the 1- and 2-bar answer, 80 for a 4-bar answer (CALL/RESPONSE phase 4).
inline bool patternHarmonyAhead(
    const GroovePuterMaterial::GeneratedPatternHarmonyOrigin& origin,
    const PatternHarmonyPlaybackPosition& now, int32_t offsetSteps,
    GroovePuterHarmony::HarmonySnapshot& out, int32_t maxOffsetSteps = 48) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  const int64_t target = static_cast<int64_t>(now.absoluteStep) + offsetSteps;
  if (target < 0 || offsetSteps < 0 || offsetSteps > maxOffsetSteps) return false;
  PatternHarmonyPlaybackPosition shifted = now;
  shifted.absoluteStep = static_cast<uint64_t>(target);
  return makePatternHarmonySnapshot(origin, shifted, out);
}

// CALL/RESPONSE phase 8: the chord of a step in a phrase whose harmonic
// authority is Idiom (a genre template wrote bass and lead; the snapshot
// builders above return Unsupported for it). The chord is read from the
// template's stabs (GenreIdiom::idiomChordAt) named by the origin
// (idiomTemplate), over the origin's key. Only the chord mask and
// its root - no HarmonySnapshot, nothing for Smart Play: the PERFORM keyboard
// keeps its own (unchanged) harmony source. Same position rules as the
// Legacy lookahead (shiftHarmonyPosition: participants, Song/loop end, at
// most maxRowsAhead rows). Phrase bars were written with phraseBars 0 (the
// P1R execution never passes ideaPhraseBars), so roles are ordinal % 4.
inline bool idiomChordAhead(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const GroovePuterHarmony::SongHarmonicParticipant* participants,
    uint8_t participantCount, const HarmonyPlaybackPosition& now,
    int32_t offsetSteps, int16_t lastSequentialRow,
    uint16_t& pcs, uint8_t& rootPc, int32_t maxRowsAhead = 2) {
  pcs = 0;
  rootPc = 0xFF;
  if (origin.common.harmonicAuthority != GroovePuterHarmony::HarmonicAuthority::Idiom)
    return false;
  uint8_t level = 0;
  const GenreIdiom::IdiomVariant* variant =
      GenreIdiom::idiomTemplateVariant(origin.common.idiomTemplate, level);
  if (variant == nullptr || !origin.valid() || !recipe.valid ||
      recipe.bars != origin.common.barCount || recipe.pageIndex != now.activePage ||
      recipe.songSlot != now.playbackSongSlot || recipe.songStart < 0 ||
      now.songRow < recipe.songStart || now.songRow >= recipe.songStart + recipe.bars)
    return false;
  // The published row must still play the phrase's own material.
  const auto& current = origin.bars[now.songRow - recipe.songStart];
  if (!GroovePuterMaterial::materialReferenceMatches(now.activeMaterial, current.material.address,
                                                     current.material.id) ||
      current.originPatternVersion != now.activeMaterialVersion)
    return false;
  HarmonyPlaybackPosition shifted{};
  if (!shiftHarmonyPosition(origin, recipe, participants, participantCount, now, offsetSteps,
                            lastSequentialRow, shifted, maxRowsAhead))
    return false;
  return GenreIdiom::idiomChordAt(
      *variant, level, origin.common.rootPitchClass,
      origin.common.scaleTypeValue, static_cast<uint8_t>(shifted.songRow - recipe.songStart),
      0, origin.common.barCount, static_cast<uint8_t>(shifted.absoluteStep % 16u), pcs, rootPc);
}

// Pattern mode: one looping generated bar (written with no bar ordinal, so the
// template's own degrees); the same pattern at a later step.
inline bool idiomPatternChordAhead(
    const GroovePuterMaterial::GeneratedPatternHarmonyOrigin& origin,
    const PatternHarmonyPlaybackPosition& now, int32_t offsetSteps,
    uint16_t& pcs, uint8_t& rootPc, int32_t maxOffsetSteps = 48) {
  pcs = 0;
  rootPc = 0xFF;
  if (origin.authority != GroovePuterHarmony::HarmonicAuthority::Idiom) return false;
  uint8_t level = 0;
  const GenreIdiom::IdiomVariant* variant = GenreIdiom::idiomTemplateVariant(origin.idiomTemplate, level);
  const int64_t target = static_cast<int64_t>(now.absoluteStep) + offsetSteps;
  if (variant == nullptr || target < 0 || offsetSteps < 0 || offsetSteps > maxOffsetSteps ||
      origin.address.voice != 0 || origin.sourceId == 0 || !origin.version.valid() ||
      origin.rootPitchClass >= 12 || now.activePage < 0 || now.activePatternGlobal < 0 ||
      songPatternPage(now.activePatternGlobal) != now.activePage ||
      origin.address.globalSlot != now.activePatternGlobal ||
      now.activeVersion != origin.version ||
      now.publicationGeneration != origin.publicationGeneration)
    return false;
  return GenreIdiom::idiomChordAt(*variant, level, origin.rootPitchClass,
                                  origin.scaleTypeValue, GenreIdiom::kNoBarOrdinal, 0, 1,
                                  static_cast<uint8_t>(static_cast<uint64_t>(target) % 16u),
                                  pcs, rootPc);
}

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_HARMONY_LOOKAHEAD_H
