#include <cassert>
#include <cstdint>

#include "src/input/performance_harmony_bridge.h"

namespace {

using namespace GroovePuterPerformance;

void fillBar(GroovePuterMaterial::GeneratedSynthAOrigin& origin,
             uint8_t bar, uint8_t slot, uint32_t materialId,
             GroovePuterMaterial::MaterialVersionToken version) {
  auto& entry = origin.bars[bar];
  entry.phraseBarOrdinal = bar;
  entry.material.address = {0, slot};
  entry.material.id = {materialId};
  entry.originPatternVersion = version;
  entry.harmonicRhythm.phraseBarOrdinal = bar;
  entry.harmonicRhythm.phraseHarmonicPosition = static_cast<uint8_t>(bar * 2u);
  entry.harmonicRhythm.eventCount = bar == 0 ? 2 : 1;
  entry.harmonicRhythm.onsets = bar == 0
      ? GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8)
      : GroovePuterRhythm::stepBit(8);
}

GroovePuterMaterial::GeneratedSynthAOrigin makeOrigin() {
  GroovePuterMaterial::GeneratedSynthAOrigin origin{};
  origin.common.barCount = 2;
  origin.common.rootPitchClass = 0;
  origin.common.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
  origin.common.harmonicAuthority = GroovePuterHarmony::HarmonicAuthority::Generic;
  GroovePuterRhythm::ChordProgressionSourceRequest request{};
  request.requestedId = GroovePuterRhythm::ProgressionId::StaticModal;
  request.phraseBars = 2;
  const auto progression = GroovePuterRhythm::realizeChordProgressionSource(request);
  assert(progression.status == GroovePuterRhythm::ChordProgressionStatus::ValidButStatic);
  origin.common.progressionSource = progression.source;
  fillBar(origin, 0, 4, 101, {0x1234u, 0x5678u});
  fillBar(origin, 1, 5, 102, {0x2345u, 0x6789u});
  return origin;
}

GroovePuterMaterial::GeneratedPhraseRecipe makeRecipe() {
  GroovePuterMaterial::GeneratedPhraseRecipe recipe{};
  recipe.pageIndex = 0;
  recipe.songSlot = 0;
  recipe.songStart = 10;
  recipe.bars = 2;
  recipe.valid = true;
  return recipe;
}

HarmonyPlaybackPosition makePosition(uint64_t absoluteStep = 16) {
  HarmonyPlaybackPosition position{};
  position.activePage = 0;
  position.playbackSongSlot = 0;
  position.songRow = 11;
  position.activeSynthAPatternGlobal = 5;
  position.activeMaterial = {{0, 5}, {102}};
  position.activeMaterialVersion = {0x2345u, 0x6789u};
  position.publicationGeneration = 1;
  position.transportEpoch = 9;
  position.absoluteStep = absoluteStep;
  return position;
}

}  // namespace

int main() {
  using namespace GroovePuterHarmony;
  auto origin = makeOrigin();
  const auto recipe = makeRecipe();
  auto position = makePosition(16u + 9u);
  HarmonySnapshot snapshot{};
  assert(makeLegacyHarmonySnapshot(origin, recipe, position, snapshot));
  assert(validateHarmonySnapshot(snapshot));
  assert(snapshot.key.materialId == 102u);
  assert(snapshot.key.publicationGeneration == 1u);
  assert(snapshot.key.transportEpoch == 9u);
  assert(snapshot.key.eventOrdinal > 0u);
  assert(snapshot.onsetStep == 24u);
  assert(snapshot.durationSteps > 0u);
  const uint8_t bassRole = static_cast<uint8_t>(HarmonicRole::Bass);
  const uint8_t chordRole = static_cast<uint8_t>(HarmonicRole::Chord);
  const uint8_t leadRole = static_cast<uint8_t>(HarmonicRole::Lead);
  assert(snapshot.roles[bassRole].allowed == snapshot.base.pitchMask);
  assert((snapshot.roles[bassRole].preferred & ~snapshot.base.pitchMask) == 0);
  assert(snapshot.roles[chordRole].allowed == snapshot.base.pitchMask);
  assert(snapshot.roles[leadRole].allowed == static_cast<PitchSet12>(
      snapshot.tonal.scaleMask | snapshot.base.pitchMask));

  auto beforeBarLine = makePosition(9u);
  beforeBarLine.songRow = 10;
  beforeBarLine.activeSynthAPatternGlobal = 4;
  beforeBarLine.activeMaterial = {{0, 4}, {101}};
  beforeBarLine.activeMaterialVersion = {0x1234u, 0x5678u};
  HarmonySnapshot crossingBar{};
  assert(makeLegacyHarmonySnapshot(origin, recipe, beforeBarLine, crossingBar));
  assert(crossingBar.durationSteps == 16u);

  // A new pass through the same Song event gets a new key, even when harmony
  // root and quality repeat.
  auto nextLoopPosition = position;
  nextLoopPosition.absoluteStep += 32u;
  HarmonySnapshot nextLoop{};
  assert(makeLegacyHarmonySnapshot(origin, recipe, nextLoopPosition, nextLoop));
  assert(nextLoop.base.rootPc == snapshot.base.rootPc);
  assert(nextLoop.base.qualityId == snapshot.base.qualityId);
  assert(!sameHarmonyEvent(snapshot.key, nextLoop.key));

  HarmonySnapshot stale{};
  auto wrong = position;
  wrong.activePage = 1;
  assert(!makeLegacyHarmonySnapshot(origin, recipe, wrong, stale));
  assert(stale.status == HarmonyStatus::Stale);
  wrong = position;
  wrong.playbackSongSlot = 1;
  assert(!makeLegacyHarmonySnapshot(origin, recipe, wrong, stale));
  assert(stale.status == HarmonyStatus::Stale);
  wrong = position;
  wrong.activeMaterial.id.value = 999u;
  assert(!makeLegacyHarmonySnapshot(origin, recipe, wrong, stale));
  assert(stale.status == HarmonyStatus::Stale);
  wrong = position;
  wrong.activeMaterialVersion.low ^= 1u;
  assert(!makeLegacyHarmonySnapshot(origin, recipe, wrong, stale));
  assert(stale.status == HarmonyStatus::Stale);
  wrong = position;
  wrong.activeSynthAPatternGlobal = 6;
  assert(!makeLegacyHarmonySnapshot(origin, recipe, wrong, stale));
  assert(stale.status == HarmonyStatus::Stale);

  // A queued material is not active until the coherent playback position says
  // its slot and row are active.
  wrong = position;
  wrong.songRow = 12;
  assert(!makeLegacyHarmonySnapshot(origin, recipe, wrong, stale));
  assert(stale.status == HarmonyStatus::Stale);

  auto idiomOrigin = origin;
  idiomOrigin.common.harmonicAuthority = HarmonicAuthority::Idiom;
  assert(!makeLegacyHarmonySnapshot(idiomOrigin, recipe, position, stale));
  assert(stale.status == HarmonyStatus::Unsupported);

  // A stopped Pattern generated by the one-bar owner still has a real
  // harmonic source. Its exact material version gates publication.
  GroovePuterMaterial::GeneratedPatternHarmonyOrigin pattern{};
  pattern.address = {0, 5};
  pattern.version = {0x2345u, 0x6789u};
  pattern.sourceId = 102;
  pattern.publicationGeneration = 2;
  pattern.rootPitchClass = 0;
  pattern.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
  pattern.authority = HarmonicAuthority::Generic;
  pattern.eventCount = 2;
  pattern.onsets = GroovePuterRhythm::stepBit(0) |
                   GroovePuterRhythm::stepBit(8);
  pattern.events[0] = {0, GroovePuterRhythm::ChordQuality::Major7, 0};
  pattern.events[1] = {1, GroovePuterRhythm::ChordQuality::Minor7, 0};
  PatternHarmonyPlaybackPosition patternPosition{};
  patternPosition.activePage = 0;
  patternPosition.activePatternGlobal = 5;
  patternPosition.activeVersion = pattern.version;
  patternPosition.absoluteStep = 9;
  patternPosition.publicationGeneration = 2;
  assert(makePatternHarmonySnapshot(pattern, patternPosition, snapshot));
  assert(snapshot.base.rootPc == 2 && snapshot.key.eventOrdinal == 8);
  pattern.authority = HarmonicAuthority::Idiom;
  assert(!makePatternHarmonySnapshot(pattern, patternPosition, stale));
  assert(stale.status == HarmonyStatus::Unsupported);
  pattern.authority = HarmonicAuthority::Generic;
  patternPosition.activeVersion.low ^= 1u;
  assert(!makePatternHarmonySnapshot(pattern, patternPosition, stale));
  assert(stale.status == HarmonyStatus::Stale);
  return 0;
}
