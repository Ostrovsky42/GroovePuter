#include <cassert>
#include <cstdint>

#include "src/input/performance_harmony_bridge.h"
#include "src/input/performance_harmony_route_cache.h"
#include "src/generation/harmony/harmonic_fingerprint.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/harmony/song_harmonic_state.h"

using namespace GroovePuterHarmony;
using namespace GroovePuterPerformance;

namespace {
SongHarmonicCandidate candidate() {
  SongHarmonicCandidate result{};
  result.songIdentity = 0x13572468u;
  result.songSlot = 0;
  result.startPosition = 10;
  result.rowCount = 2;
  result.projectSeed = 99;
  result.style = HarmonicStyleId::Acid;
  result.modelVersion = 1;
  result.corpusChecksum = 0x9c01d00du;
  result.genome = deriveSongVariantGenome(result.projectSeed,
                                           result.songIdentity,
                                           result.style,
                                           result.modelVersion);
  result.home = {kQualityMajor,
                 qualityDefinition(kQualityMajor)->intervalMask, 0,
                 kNoBassPc};
  const auto* major = scaleDefinition(1);
  result.homeTonal = {1, 1, major->intervalMask, major->intervalMask, 0, 0};
  result.harmony.materialId = 700;
  result.harmony.publicationGeneration = 17;
  result.harmony.modelVersion = 1;
  result.harmony.authority = HarmonicAuthority::Graph;
  result.harmony.eventCount = 3;
  for (uint8_t i = 0; i < result.harmony.eventCount; ++i) {
    const uint8_t root = i == 2 ? 7 : 0;
    auto& event = result.harmony.events[i];
    event.state = {kQualityMajor,
                   transposePitchSet(qualityDefinition(kQualityMajor)->intervalMask,
                                      root),
                   root, kNoBassPc};
    event.tonal = result.homeTonal;
    event.timingSlotOrdinal = i;
  }
  result.terminal = result.harmony.events[2].state;
  result.terminalTonal = result.harmony.events[2].tonal;
  result.participantCount = 4;
  result.participants[0] = {0, 10, 0, {101}, {0x1001u, 0x2001u}};
  result.participants[1] = {1, 10, 0, {201}, {0x3001u, 0x4001u}};
  result.participants[2] = {0, 11, 0, {102}, {0x1002u, 0x2002u}};
  result.participants[3] = {1, 11, 0, {202}, {0x3002u, 0x4002u}};
  return result;
}

GroovePuterMaterial::GeneratedSynthAOrigin makeOrigin() {
  GroovePuterMaterial::GeneratedSynthAOrigin origin{};
  origin.common.barCount = 2;
  origin.common.harmonicAuthority = HarmonicAuthority::Graph;
  auto& first = origin.bars[0];
  first.phraseBarOrdinal = 0;
  first.material = {{0, 4}, {101}};
  first.originPatternVersion = {0x1001u, 0x2001u};
  first.harmonicRhythm.phraseBarOrdinal = 0;
  first.harmonicRhythm.phraseHarmonicPosition = 0;
  first.harmonicRhythm.eventCount = 2;
  first.harmonicRhythm.onsets = static_cast<GroovePuterRhythm::StepMask>(
      GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8));
  auto& second = origin.bars[1];
  second.phraseBarOrdinal = 1;
  second.material = {{0, 5}, {102}};
  second.originPatternVersion = {0x1002u, 0x2002u};
  second.harmonicRhythm.phraseBarOrdinal = 1;
  second.harmonicRhythm.phraseHarmonicPosition = 2;
  second.harmonicRhythm.eventCount = 1;
  second.harmonicRhythm.onsets = GroovePuterRhythm::stepBit(8);
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

HarmonyPlaybackPosition makePosition() {
  HarmonyPlaybackPosition position{};
  position.activePage = 0;
  position.playbackSongSlot = 0;
  position.songRow = 11;
  position.activeSynthAPatternGlobal = 5;
  position.activeMaterial = {{0, 5}, {102}};
  position.activeMaterialVersion = {0x1002u, 0x2002u};
  position.publicationGeneration = 20;  // Playback seqlock, distinct from route gen 17.
  position.transportEpoch = 8;
  position.absoluteStep = 25;
  return position;
}
}  // namespace

int main() {
  SongHarmonicState state{};
  const auto route = candidate();
  assert(validSongHarmonicCandidate(route));
  assert(commitSongHarmonicCandidate(state, route, 55) ==
         SongHarmonicCommitStatus::Committed);
  SongHarmonicParticipant current[4]{};
  for (uint8_t i = 0; i < 4; ++i) current[i] = state.participants[i];

  const auto origin = makeOrigin();
  const auto recipe = makeRecipe();
  const auto position = makePosition();
  HarmonySnapshot snapshot{};
  assert(makeGraphHarmonySnapshot(state, current, 4, origin, recipe, position,
                                  snapshot));
  assert(validateHarmonySnapshot(snapshot));
  assert(snapshot.status == HarmonyStatus::Ready);
  assert(snapshot.base.rootPc == 7);  // current row maps to phrase event 2.
  assert(snapshot.key.materialId == 102);
  assert(snapshot.key.publicationGeneration == 20);
  assert(snapshot.onsetStep == 24);
  assert(snapshot.durationSteps == 8);

  PerformanceHarmonyRouteCache cache;
  assert(cache.shouldLookup("project", 0, 10, current, 4, 100));
  cache.rememberResult("project", 0, 10, current, 4, 100, true, false,
                       &state);
  assert(!cache.shouldLookup("project", 0, 10, current, 4, 101));
  assert(cache.stateFor("project", 0, 10, current, 4) != nullptr);
  SongHarmonicParticipant changed[4]{};
  for (uint8_t i = 0; i < 4; ++i) changed[i] = current[i];
  changed[3].version.high++;
  assert(cache.shouldLookup("project", 0, 10, changed, 4, 102));
  assert(cache.stateFor("project", 0, 10, changed, 4) == nullptr);
  cache.rememberResult("project", 0, 10, current, 4, 200, false, true,
                       nullptr);
  assert(!cache.shouldLookup("project", 0, 10, current, 4, 1199));
  assert(cache.shouldLookup("project", 0, 10, current, 4, 1200));

  current[1].version.low++;
  HarmonySnapshot stale{};
  assert(!makeGraphHarmonySnapshot(state, current, 4, origin, recipe, position,
                                   stale));
  assert(stale.status == HarmonyStatus::Stale);

  for (uint8_t i = 0; i < 4; ++i) current[i] = state.participants[i];
  current[2].materialId.value++;
  HarmonySnapshot replacedA{};
  assert(!makeGraphHarmonySnapshot(state, current, 4, origin, recipe, position,
                                   replacedA));
  assert(replacedA.status == HarmonyStatus::Stale);

  for (uint8_t i = 0; i < 4; ++i) current[i] = state.participants[i];
  auto staleOrigin = origin;
  staleOrigin.common.harmonicAuthority = HarmonicAuthority::Generic;
  HarmonySnapshot genericOrigin{};
  assert(!makeGraphHarmonySnapshot(state, current, 4, staleOrigin, recipe,
                                   position, genericOrigin));
}
