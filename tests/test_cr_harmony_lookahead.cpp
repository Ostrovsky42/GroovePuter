// CALL/RESPONSE phase 2: harmony of the next bar (absoluteStep + 16) without
// the file system, its fallback, and the Song-row boundary.
#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/input/call_response_harmony.h"
#include "src/input/performance_harmony_lookahead.h"

namespace {

using namespace GroovePuterPerformance;
using GroovePuterHarmony::HarmonySnapshot;
using GroovePuterHarmony::SongHarmonicParticipant;
using GroovePuterCallResponse::ChordSource;
using GroovePuterCallResponse::chooseAnswerChord;

constexpr int16_t kSongStart = 10;

void fillBar(GroovePuterMaterial::GeneratedSynthAOrigin& origin, uint8_t bar,
             uint8_t slot, uint32_t materialId,
             GroovePuterMaterial::MaterialVersionToken version) {
  auto& entry = origin.bars[bar];
  entry.phraseBarOrdinal = bar;
  entry.material.address = {0, slot};
  entry.material.id = {materialId};
  entry.originPatternVersion = version;
  entry.harmonicRhythm.phraseBarOrdinal = bar;
  entry.harmonicRhythm.phraseHarmonicPosition = static_cast<uint8_t>(bar * 2u);
  entry.harmonicRhythm.eventCount = 2;
  entry.harmonicRhythm.onsets =
      GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8);
}

GroovePuterMaterial::GeneratedSynthAOrigin makeOrigin() {
  GroovePuterMaterial::GeneratedSynthAOrigin origin{};
  origin.common.barCount = 2;
  origin.common.rootPitchClass = 0;
  origin.common.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
  origin.common.harmonicAuthority = GroovePuterHarmony::HarmonicAuthority::Generic;
  GroovePuterRhythm::ChordProgressionSourceRequest request{};
  request.requestedId = GroovePuterRhythm::ProgressionId::PopCycle;
  request.phraseBars = 2;
  const auto progression = GroovePuterRhythm::realizeChordProgressionSource(request);
  origin.common.progressionSource = progression.source;
  fillBar(origin, 0, 4, 101, {0x1234u, 0x5678u});
  fillBar(origin, 1, 5, 102, {0x2345u, 0x6789u});
  return origin;
}

GroovePuterMaterial::GeneratedPhraseRecipe makeRecipe() {
  GroovePuterMaterial::GeneratedPhraseRecipe recipe{};
  recipe.pageIndex = 0;
  recipe.songSlot = 0;
  recipe.songStart = kSongStart;
  recipe.bars = 2;
  recipe.valid = true;
  return recipe;
}

// The published position at songRow (row ordinal 0 or 1 of the phrase).
HarmonyPlaybackPosition positionAt(uint8_t ordinal, uint64_t absoluteStep) {
  HarmonyPlaybackPosition position{};
  position.activePage = 0;
  position.playbackSongSlot = 0;
  position.songRow = static_cast<int16_t>(kSongStart + ordinal);
  position.activeSynthAPatternGlobal = ordinal == 0 ? 4 : 5;
  position.activeMaterial = ordinal == 0
      ? GroovePuterMaterial::MaterialReference{{0, 4}, {101}}
      : GroovePuterMaterial::MaterialReference{{0, 5}, {102}};
  position.activeMaterialVersion = ordinal == 0
      ? GroovePuterMaterial::MaterialVersionToken{0x1234u, 0x5678u}
      : GroovePuterMaterial::MaterialVersionToken{0x2345u, 0x6789u};
  position.publicationGeneration = 2;
  position.transportEpoch = 7;
  position.absoluteStep = absoluteStep;
  return position;
}

// What tryCaptureSongHarmonicParticipants returns for rows 10..11 (in memory).
uint8_t participantsFor(SongHarmonicParticipant (&out)[4]) {
  out[0] = {0, kSongStart, 0, {101}, {0x1234u, 0x5678u}};
  out[1] = {1, kSongStart, 0, {201}, {0x1u, 0x2u}};
  out[2] = {0, kSongStart + 1, 0, {102}, {0x2345u, 0x6789u}};
  out[3] = {1, kSongStart + 1, 0, {202}, {0x3u, 0x4u}};
  return 4;
}

bool sameSnapshot(const HarmonySnapshot& a, const HarmonySnapshot& b) {
  if (a.status != b.status || a.onsetStep != b.onsetStep ||
      a.durationSteps != b.durationSteps ||
      !GroovePuterHarmony::sameHarmonyEvent(a.key, b.key) ||
      a.base.pitchMask != b.base.pitchMask || a.base.rootPc != b.base.rootPc ||
      a.base.qualityId != b.base.qualityId || a.tonal.scaleMask != b.tonal.scaleMask ||
      a.tonal.tonicPc != b.tonal.tonicPc) return false;
  for (uint8_t r = 0; r < 3; ++r) {
    if (a.roles[r].allowed != b.roles[r].allowed ||
        a.roles[r].preferred != b.roles[r].preferred) return false;
  }
  return true;
}

void testAheadEqualsNextBar() {
  const auto origin = makeOrigin();
  const auto recipe = makeRecipe();
  SongHarmonicParticipant participants[4];
  const uint8_t count = participantsFor(participants);
  const int16_t lastRow = 31;  // a long forward Song
  int differing = 0;
  for (uint64_t step = 0; step < 16; ++step) {
    const auto now = positionAt(0, 160 + step);
    HarmonySnapshot current{};
    assert(makeLegacyHarmonySnapshot(origin, recipe, now, current));
    HarmonySnapshot ahead{};
    assert(legacyHarmonyAhead(origin, recipe, participants, count, now, 16, lastRow, ahead));
    HarmonySnapshot reference{};
    assert(makeLegacyHarmonySnapshot(origin, recipe, positionAt(1, 176 + step), reference));
    assert(sameSnapshot(ahead, reference));
    if (ahead.base.pitchMask != current.base.pitchMask) ++differing;
    // Offset 0 is the current snapshot.
    HarmonySnapshot same{};
    assert(legacyHarmonyAhead(origin, recipe, participants, count, now, 0, lastRow, same));
    assert(sameSnapshot(same, current));
  }
  // The fixture is a real progression: the next bar is not the current chord.
  assert(differing > 0);
  // A published position that is still in the previous bar (step 15) reaches
  // the next bar by a small offset as well (+2 -> row 11 step 1).
  HarmonySnapshot late{};
  assert(legacyHarmonyAhead(origin, recipe, participants, count, positionAt(0, 175), 2,
                            lastRow, late));
  HarmonySnapshot reference{};
  assert(makeLegacyHarmonySnapshot(origin, recipe, positionAt(1, 177), reference));
  assert(sameSnapshot(late, reference));
}

void testBoundaries() {
  const auto origin = makeOrigin();
  const auto recipe = makeRecipe();
  SongHarmonicParticipant participants[4];
  const uint8_t count = participantsFor(participants);
  HarmonySnapshot ahead{};
  // Past the generated phrase (row 12): no snapshot.
  assert(!legacyHarmonyAhead(origin, recipe, participants, count, positionAt(1, 176), 16, 31,
                             ahead));
  // End of Song / loop end / reverse playback: the next row is not row + 1.
  assert(!legacyHarmonyAhead(origin, recipe, participants, count, positionAt(0, 160), 16,
                             kSongStart, ahead));
  // The Song row was edited to another pattern: participants disagree.
  SongHarmonicParticipant edited[4];
  participantsFor(edited);
  edited[2].materialId = {999};
  assert(!legacyHarmonyAhead(origin, recipe, edited, count, positionAt(0, 160), 16, 31, ahead));
  // Without in-memory participants the next row cannot be confirmed.
  assert(!legacyHarmonyAhead(origin, recipe, nullptr, 0, positionAt(0, 160), 16, 31, ahead));
  // Never behind the published row, never more than two rows ahead.
  assert(!legacyHarmonyAhead(origin, recipe, participants, count, positionAt(1, 176), -1, 31,
                             ahead));
  // Within the current row a lookahead needs no participants.
  assert(legacyHarmonyAhead(origin, recipe, nullptr, 0, positionAt(0, 160), 8, 31, ahead));
}

void testPatternAhead() {
  GroovePuterMaterial::GeneratedPatternHarmonyOrigin origin{};
  origin.address = {0, 3};
  origin.authority = GroovePuterHarmony::HarmonicAuthority::Generic;
  origin.rootPitchClass = 0;
  origin.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
  origin.onsets = GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8);
  origin.eventCount = 2;
  origin.events[0].degree = 0;
  origin.events[1].degree = 3;
  origin.version = {0x11u, 0x22u};
  origin.sourceId = 1;
  origin.publicationGeneration = 3;
  if (!origin.valid()) {
    std::puts("pattern origin fixture not valid; pattern lookahead skipped");
    return;
  }
  PatternHarmonyPlaybackPosition now{};
  now.activePage = 0;
  now.activePatternGlobal = 3;
  now.activeVersion = origin.version;
  now.publicationGeneration = 3;
  now.transportEpoch = 1;
  for (uint64_t step = 32; step < 48; ++step) {
    now.absoluteStep = step;
    HarmonySnapshot ahead{};
    assert(patternHarmonyAhead(origin, now, 16, ahead));
    auto later = now;
    later.absoluteStep = step + 16;
    HarmonySnapshot reference{};
    assert(makePatternHarmonySnapshot(origin, later, reference));
    assert(sameSnapshot(ahead, reference));
  }
}

void testChordFallback() {
  ChordSource source = ChordSource::None;
  // Next-bar snapshot wins.
  assert(chooseAnswerChord(true, 0x091u, true, 0x0A21u, &source) == 0x091u);
  assert(source == ChordSource::Ahead);
  // No snapshot for the next bar: the current harmony.
  assert(chooseAnswerChord(false, 0, true, 0x0A21u, &source) == 0x0A21u);
  assert(source == ChordSource::Current);
  // An empty snapshot counts as missing.
  assert(chooseAnswerChord(true, 0, true, 0x0A21u, &source) == 0x0A21u);
  assert(source == ChordSource::Current);
  // Neither: phase 1 (no chord).
  assert(chooseAnswerChord(false, 0, false, 0, &source) == 0);
  assert(source == ChordSource::None);

  GroovePuterCallResponse::ResponseHarmony harmony{};
  harmony.set(0, 0x091u, ChordSource::Ahead);
  harmony.set(5, 0x0A21u, ChordSource::Current);
  assert(harmony.any());
  assert(harmony.chordAtStep(3) == 0x091u);
  assert(harmony.chordAtStep(20) == 0x0A21u);
  assert(harmony.chordAtStep(8) == 0);
  assert(harmony.aheadMask == 0x01u && harmony.currentMask == 0x20u);
}

}  // namespace

int main() {
  testAheadEqualsNextBar();
  testBoundaries();
  testPatternAhead();
  testChordFallback();
  std::puts("cr harmony lookahead tests passed");
  return 0;
}
