#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_HARMONY_BRIDGE_H
#define GROOVEPUTER_PERFORMANCE_HARMONY_BRIDGE_H

#include <cstdint>

#include "performance_harmony_context.h"
#include "performance_keyboard.h"
#include "../generation/harmony/harmonic_scale.h"
#include "../generation/harmony/harmonic_role_policy.h"
#include "../generation/harmony/harmonic_snapshot.h"
#include "../generation/harmony/song_harmonic_state.h"

namespace GroovePuterPerformance {

// All fields are copied from one coherent, control-side playback publication.
// Do not assemble this value by reading mutable renderer/Scene fields.
struct HarmonyPlaybackPosition {
  int16_t activePage = -1;
  int16_t playbackSongSlot = -1;
  int16_t songRow = -1;
  int16_t activeSynthAPatternGlobal = -1;
  GroovePuterMaterial::MaterialReference activeMaterial{};
  GroovePuterMaterial::MaterialVersionToken activeMaterialVersion{};
  uint32_t publicationGeneration = 0;
  uint32_t transportEpoch = 0;
  uint64_t absoluteStep = 0;
};

inline uint16_t legacyEventDurationSteps(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    uint8_t barOrdinal, uint8_t eventOnset) {
  const uint16_t eventAbsolute = static_cast<uint16_t>(barOrdinal) * 16u + eventOnset;
  const uint16_t phraseEnd = static_cast<uint16_t>(origin.common.barCount) * 16u;
  for (uint16_t absolute = static_cast<uint16_t>(eventAbsolute + 1u);
       absolute < phraseEnd; ++absolute) {
    const uint8_t bar = static_cast<uint8_t>(absolute / 16u);
    const uint8_t step = static_cast<uint8_t>(absolute % 16u);
    if ((origin.bars[bar].harmonicRhythm.onsets &
         GroovePuterRhythm::stepBit(step)) != 0u) {
      return static_cast<uint16_t>(absolute - eventAbsolute);
    }
  }
  return static_cast<uint16_t>(phraseEnd - eventAbsolute);
}

inline bool makeLegacyHarmonySnapshot(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const HarmonyPlaybackPosition& position,
    GroovePuterHarmony::HarmonySnapshot& out) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  out.status = GroovePuterHarmony::HarmonyStatus::Stale;

  if (!origin.valid() || !recipe.valid || recipe.bars != origin.common.barCount ||
      recipe.pageIndex != position.activePage ||
      recipe.songSlot != position.playbackSongSlot ||
      recipe.songStart < 0 || position.songRow < recipe.songStart ||
      position.songRow >= recipe.songStart + recipe.bars ||
      position.activeSynthAPatternGlobal < 0 ||
      position.activeSynthAPatternGlobal > 255 ||
      position.publicationGeneration == 0 ||
      position.activeMaterial.address.voice != 0u ||
      position.activeMaterial.address.globalSlot !=
          position.activeSynthAPatternGlobal ||
      !position.activeMaterial.id.valid() ||
      !position.activeMaterialVersion.valid()) {
    return false;
  }

  if (origin.common.harmonicAuthority !=
      GroovePuterHarmony::HarmonicAuthority::Generic) {
    out.status = GroovePuterHarmony::HarmonyStatus::Unsupported;
    return false;
  }

  const uint8_t barOrdinal = static_cast<uint8_t>(position.songRow - recipe.songStart);
  if (barOrdinal >= origin.common.barCount) return false;
  const auto& bar = origin.bars[barOrdinal];
  if (bar.phraseBarOrdinal != barOrdinal ||
      !GroovePuterMaterial::materialReferenceMatches(
          position.activeMaterial, bar.material.address, bar.material.id) ||
      bar.originPatternVersion != position.activeMaterialVersion ||
      songPatternPage(position.activeSynthAPatternGlobal) != position.activePage) {
    return false;
  }

  PerformanceContext context{};
  const uint8_t stepInBar = static_cast<uint8_t>(position.absoluteStep % 16u);
  if (!performanceContextAtGeneratedPhrase(
          origin, recipe, position.activePage, position.playbackSongSlot,
          position.songRow, position.activeSynthAPatternGlobal,
          position.activeMaterial.id.value, stepInBar, context)) {
    return false;
  }

  GroovePuterHarmony::QualityId qualityId = 0;
  GroovePuterHarmony::ScaleId scaleId = 0;
  if (!GroovePuterHarmony::legacyQualityToId(context.harmony.quality, qualityId) ||
      !GroovePuterHarmony::legacyScaleToId(origin.common.scaleTypeValue, scaleId)) {
    out.status = GroovePuterHarmony::HarmonyStatus::Unsupported;
    return false;
  }
  const uint64_t stepsSinceEvent =
      static_cast<uint64_t>(stepInBar - context.eventOnset);
  if (position.absoluteStep < stepsSinceEvent || context.eventDuration == 0u) {
    return false;
  }

  const auto* quality = GroovePuterHarmony::qualityDefinition(qualityId);
  const auto* scale = GroovePuterHarmony::scaleDefinition(scaleId);
  if (quality == nullptr || scale == nullptr) {
    out.status = GroovePuterHarmony::HarmonyStatus::Unsupported;
    return false;
  }
  const uint64_t eventOnset = position.absoluteStep - stepsSinceEvent;
  if (eventOnset > UINT32_MAX) {
    out.status = GroovePuterHarmony::HarmonyStatus::CapacityExceeded;
    return false;
  }

  out.key.materialId = position.activeMaterial.id.value;
  out.key.publicationGeneration = position.publicationGeneration;
  out.key.transportEpoch = position.transportEpoch;
  // Absolute event onset makes loop passes distinct even if the musical event
  // has the same root/quality and the caller's transport epoch is unchanged.
  out.key.eventOrdinal = static_cast<uint32_t>(eventOnset);
  out.onsetStep = eventOnset;
  out.durationSteps = legacyEventDurationSteps(origin, barOrdinal,
                                                context.eventOnset);
  out.base.qualityId = qualityId;
  out.base.rootPc = static_cast<uint8_t>(context.rootMidi % 12u);
  out.base.bassPc = out.base.rootPc;
  out.base.pitchMask = GroovePuterHarmony::transposePitchSet(
      quality->intervalMask, out.base.rootPc);
  out.tonal.scaleId = scaleId;
  out.tonal.homeScaleId = scaleId;
  out.tonal.tonicPc = origin.common.rootPitchClass;
  out.tonal.homeTonicPc = origin.common.rootPitchClass;
  out.tonal.scaleMask = GroovePuterHarmony::transposePitchSet(
      scale->intervalMask, out.tonal.tonicPc);
  out.tonal.homeScaleMask = out.tonal.scaleMask;
  for (uint8_t role = 0;
       role < static_cast<uint8_t>(GroovePuterHarmony::HarmonicRole::Count);
       ++role) {
    const auto status = GroovePuterHarmony::rolePitchPolicy(
        out.base, out.tonal,
        static_cast<GroovePuterHarmony::HarmonicRole>(role), 0,
        out.roles[role]);
    if (status != GroovePuterHarmony::HarmonyStatus::Ready) {
      out = GroovePuterHarmony::HarmonySnapshot{};
      out.status = status;
      return false;
    }
  }
  out.status = GroovePuterHarmony::HarmonyStatus::Ready;
  if (!GroovePuterHarmony::validateHarmonySnapshot(out)) {
    out = GroovePuterHarmony::HarmonySnapshot{};
    out.status = GroovePuterHarmony::HarmonyStatus::Invalid;
    return false;
  }
  return true;
}

inline bool makeGraphHarmonySnapshot(
    const GroovePuterHarmony::SongHarmonicState& state,
    const GroovePuterHarmony::SongHarmonicParticipant* currentParticipants,
    uint8_t currentParticipantCount,
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const HarmonyPlaybackPosition& position,
    GroovePuterHarmony::HarmonySnapshot& out) {
  out = GroovePuterHarmony::HarmonySnapshot{};
  out.status = GroovePuterHarmony::HarmonyStatus::Stale;
  if (!GroovePuterHarmony::songHarmonicParticipantsMatch(
          state, currentParticipants, currentParticipantCount) ||
      !origin.valid() || !recipe.valid ||
      origin.common.harmonicAuthority !=
          GroovePuterHarmony::HarmonicAuthority::Graph ||
      origin.common.barCount != recipe.bars || recipe.bars == 0 ||
      recipe.pageIndex != position.activePage ||
      recipe.songSlot != position.playbackSongSlot ||
      recipe.songStart < 0 || recipe.songStart != state.startPosition ||
      recipe.bars != state.rowCount || recipe.songSlot != state.songSlot ||
      position.songRow < recipe.songStart ||
      position.songRow >= recipe.songStart + recipe.bars ||
      position.activeSynthAPatternGlobal < 0 ||
      position.activeSynthAPatternGlobal > 255 ||
      position.publicationGeneration == 0 ||
      position.activeMaterial.address.voice != 0u ||
      position.activeMaterial.address.globalSlot !=
          position.activeSynthAPatternGlobal ||
      !position.activeMaterial.id.valid() ||
      !position.activeMaterialVersion.valid() ||
      songPatternPage(position.activeSynthAPatternGlobal) != position.activePage) {
    return false;
  }

  const uint8_t barOrdinal =
      static_cast<uint8_t>(position.songRow - recipe.songStart);
  const auto& bar = origin.bars[barOrdinal];
  if (bar.phraseBarOrdinal != barOrdinal ||
      !GroovePuterMaterial::materialReferenceMatches(
          position.activeMaterial, bar.material.address, bar.material.id) ||
      bar.originPatternVersion != position.activeMaterialVersion) {
    return false;
  }
  bool foundActiveParticipant = false;
  for (uint8_t i = 0; i < state.participantCount; ++i) {
    const auto& participant = state.participants[i];
    if (participant.track != 0 || participant.songPosition != position.songRow)
      continue;
    if (participant.materialId != position.activeMaterial.id ||
        participant.version != position.activeMaterialVersion) return false;
    foundActiveParticipant = true;
  }
  if (!foundActiveParticipant) return false;

  const auto& rhythm = bar.harmonicRhythm;
  if (rhythm.phraseBarOrdinal != barOrdinal || rhythm.eventCount == 0u ||
      rhythm.eventCount > GroovePuterRhythm::kMaxHarmonicEvents ||
      GroovePuterRhythm::phraseHarmonicPositionCount(rhythm.onsets) !=
          rhythm.eventCount) {
    return false;
  }
  const uint8_t stepInBar = static_cast<uint8_t>(position.absoluteStep % 16u);
  uint8_t eventInBar = 0;
  uint8_t eventOnset = 0;
  bool foundEvent = false;
  uint8_t ordinal = 0;
  for (uint8_t step = 0; step < 16u; ++step) {
    if ((rhythm.onsets & GroovePuterRhythm::stepBit(step)) == 0u) continue;
    if (step <= stepInBar) {
      eventInBar = ordinal;
      eventOnset = step;
      foundEvent = true;
    } else {
      break;
    }
    ++ordinal;
  }
  if (!foundEvent) return false;
  const uint16_t eventOrdinal = static_cast<uint16_t>(
      rhythm.phraseHarmonicPosition + eventInBar);
  if (eventOrdinal >= state.harmony.eventCount) return false;
  const uint64_t stepsSinceEvent = stepInBar - eventOnset;
  if (position.absoluteStep < stepsSinceEvent) return false;
  const uint64_t eventOnsetAbsolute = position.absoluteStep - stepsSinceEvent;
  if (eventOnsetAbsolute > UINT32_MAX) return false;
  const uint16_t duration = legacyEventDurationSteps(
      origin, barOrdinal, eventOnset);
  if (duration == 0) return false;

  const auto& event = state.harmony.events[eventOrdinal];
  out.key.materialId = position.activeMaterial.id.value;
  out.key.publicationGeneration = position.publicationGeneration;
  out.key.transportEpoch = position.transportEpoch;
  out.key.eventOrdinal = static_cast<uint32_t>(eventOnsetAbsolute);
  out.onsetStep = eventOnsetAbsolute;
  out.durationSteps = duration;
  out.base = event.state;
  out.tonal = event.tonal;
  for (uint8_t role = 0;
       role < static_cast<uint8_t>(GroovePuterHarmony::HarmonicRole::Count);
       ++role) {
    const auto status = GroovePuterHarmony::rolePitchPolicy(
        out.base, out.tonal,
        static_cast<GroovePuterHarmony::HarmonicRole>(role), 0,
        out.roles[role]);
    if (status != GroovePuterHarmony::HarmonyStatus::Ready) {
      out = GroovePuterHarmony::HarmonySnapshot{};
      out.status = status;
      return false;
    }
  }
  out.status = GroovePuterHarmony::HarmonyStatus::Ready;
  if (!GroovePuterHarmony::validateHarmonySnapshot(out)) {
    out = GroovePuterHarmony::HarmonySnapshot{};
    out.status = GroovePuterHarmony::HarmonyStatus::Invalid;
    return false;
  }
  return true;
}

class PerformanceHarmonyBridge {
 public:
  void update(PerformanceKeyboard& keyboard,
              const GroovePuterHarmony::HarmonySnapshot& snapshot) {
    if (!GroovePuterHarmony::validateHarmonySnapshot(snapshot)) {
      keyboard.clearPerformanceHarmonyContext();
      hasLastEvent_ = false;
      return;
    }
    if (hasLastEvent_ &&
        GroovePuterHarmony::sameHarmonyEvent(lastEvent_, snapshot.key)) return;
    if (!keyboard.setHarmonySnapshot(snapshot)) {
      keyboard.clearPerformanceHarmonyContext();
      hasLastEvent_ = false;
      return;
    }
    lastEvent_ = snapshot.key;
    hasLastEvent_ = true;
  }

 private:
  GroovePuterHarmony::HarmonyEventKey lastEvent_{};
  bool hasLastEvent_ = false;
};

}  // namespace GroovePuterPerformance

#endif  // GROOVEPUTER_PERFORMANCE_HARMONY_BRIDGE_H
