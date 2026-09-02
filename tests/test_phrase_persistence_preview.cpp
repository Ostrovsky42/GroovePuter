#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "src/phrase/phrase_core.h"
#include "src/phrase/phrase_persistence.h"

namespace {

Song makeOneBarSong(int16_t synthA, int16_t synthB, int16_t drums) {
  Song song{};
  song.length = 1;
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      song.positions[row].patterns[track] = -1;
    }
  }
  song.positions[0].patterns[0] = synthA;
  song.positions[0].patterns[1] = synthB;
  song.positions[0].patterns[2] = drums;
  return song;
}

void testFlatPersistenceRoundTrip() {
  PhraseCore::PhraseBank source{};
  PhraseCore::reset(source);
  Song song = makeOneBarSong(1, 2, 3);
  assert(PhraseCore::captureSongRegion(
      source,
      PhraseCore::SlotId::A,
      song,
      0,
      0,
      1,
      PhraseCore::Role::Main,
      PhraseCore::Source::Generated));
  assert(PhraseCore::deriveReferenceView(
      source,
      PhraseCore::SlotId::B,
      PhraseCore::SlotId::A,
      PhraseCore::Role::Variation));

  int32_t values[PhraseCore::kPersistValueCount]{};
  for (int i = 0; i < PhraseCore::kPersistValueCount; ++i) {
    values[i] = PhraseCore::persistentValueAt(source, i);
  }

  PhraseCore::PhraseBank decoded{};
  PhraseCore::beginPersistentDecode(decoded);
  for (int i = 0; i < PhraseCore::kPersistValueCount; ++i) {
    assert(PhraseCore::applyPersistentValue(decoded, i, values[i]));
  }
  assert(!PhraseCore::sanitize(decoded));
  assert(std::memcmp(&source, &decoded, sizeof(source)) == 0);
}

void testUnknownVersionResets() {
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  bank.version = 99;
  bank.slots[0].metadata.flags = PhraseCore::kFlagValid;
  assert(PhraseCore::sanitize(bank));
  assert(bank.version == PhraseCore::kPersistenceVersion);
  assert(bank.nextPhraseId == 1);
  assert(!PhraseCore::summarize(bank, PhraseCore::SlotId::A).valid);
}

void testPreviewMasksAndEnergy() {
  Scene scene{};
  SynthPattern& synthA = scene.synthABanks[0].patterns[0];
  SynthPattern& synthB = scene.synthBBanks[0].patterns[1];
  DrumPatternSet& drums = scene.drumBanks[0].patterns[2];

  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    synthA.steps[step].note = -1;
    synthB.steps[step].note = -1;
  }
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      drums.voices[voice].steps[step].hit = false;
    }
  }

  synthA.steps[0].note = 48;
  synthA.steps[4].note = 52;
  synthB.steps[2].note = 60;
  drums.voices[0].steps[0].hit = true;
  drums.voices[0].steps[8].hit = true;
  drums.voices[1].steps[8].hit = true;

  const int16_t refA = songPatternFromPageBankIndex(0, 0, 0);
  const int16_t refB = songPatternFromPageBankIndex(0, 0, 1);
  const int16_t refD = songPatternFromPageBankIndex(0, 0, 2);
  Song song = makeOneBarSong(refA, refB, refD);

  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  assert(PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::A,
      song,
      0,
      0,
      1,
      PhraseCore::Role::Main,
      PhraseCore::Source::InternalPattern));

  PhraseCore::BarPreview preview{};
  assert(PhraseCore::buildBarPreview(
      bank.slots[0], 0, scene, 0, preview));
  assert(preview.patternRefs[0] == refA);
  assert(preview.patternRefs[1] == refB);
  assert(preview.patternRefs[2] == refD);
  assert(preview.synthAMask == 0x0011u);
  assert(preview.synthBMask == 0x0004u);
  assert(preview.drumMask == 0x0101u);
  assert(preview.resolvedMask == PhraseCore::kAllTracks);
  assert(preview.energy == 24u);
}

void testPreviewKeepsUnresolvedReference() {
  Scene scene{};
  const int16_t remoteRef = songPatternFromPageBankIndex(1, 0, 0);
  Song song = makeOneBarSong(remoteRef, -1, -1);
  PhraseCore::PhraseBank bank{};
  PhraseCore::reset(bank);
  assert(PhraseCore::captureSongRegion(
      bank,
      PhraseCore::SlotId::C,
      song,
      0,
      0,
      1,
      PhraseCore::Role::Break,
      PhraseCore::Source::InternalPattern,
      PhraseCore::kTrackSynthA));

  PhraseCore::BarPreview preview{};
  assert(PhraseCore::buildBarPreview(
      bank.slots[2], 0, scene, 0, preview));
  assert(preview.patternRefs[0] == remoteRef);
  assert(preview.resolvedMask == 0);
  assert(preview.synthAMask == 0);
  assert(preview.energy == 4u);
}

}  // namespace

int main() {
  testFlatPersistenceRoundTrip();
  testUnknownVersionResets();
  testPreviewMasksAndEnergy();
  testPreviewKeepsUnresolvedReference();
  std::cout << "Phrase persistence/preview tests passed\n";
  return 0;
}
