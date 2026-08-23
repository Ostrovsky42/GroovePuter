#include <cassert>
#include <cstdint>
#include <cstdio>

#include "platform_sdl/arduino_compat.h"
#include "src/audio/audio_config.h"
#include "src/dsp/miniacid_engine.h"

SerialMock Serial;
SDMock SD;

#if !defined(GROOVEPUTER_P1C_TEST_SEAM)
#error "P1c runtime regression requires GROOVEPUTER_P1C_TEST_SEAM"
#endif

void setP1cEffectivePatternRefOverrideForTest(int songSlot,
                                              int position,
                                              SongTrack track,
                                              int16_t globalPattern);
void clearP1cEffectivePatternRefOverrideForTest();

namespace {

constexpr int kPage = 0;
constexpr int kOldBank = 0;
constexpr int kOldSlot = 1;
constexpr int kLeaseBank = 1;
constexpr int kLeaseSlot = 2;
constexpr int8_t kOldSynthANote = 36;
constexpr int8_t kLeaseSynthANote = 61;
constexpr int8_t kOldSynthBNote = 48;
constexpr int8_t kLeaseSynthBNote = 73;

int globalPattern(int bank, int slot) {
  return songPatternFromPageBankIndex(kPage, bank, slot);
}

void writeSynthMaterial(Scene& scene,
                        SongTrack track,
                        int bank,
                        int slot,
                        int8_t note) {
  SynthPattern& pattern = track == SongTrack::SynthA
      ? scene.synthABanks[bank].patterns[slot]
      : scene.synthBBanks[bank].patterns[slot];
  pattern.steps[0].note = note;
  pattern.steps[0].velocity = 99;
}

void writeDrumMaterial(Scene& scene,
                       int bank,
                       int slot,
                       bool kickAtZero) {
  DrumPatternSet& pattern = scene.drumBanks[bank].patterns[slot];
  pattern.voices[0].steps[0].hit = kickAtZero;
  pattern.voices[0].steps[0].velocity = kickAtZero ? 117 : 47;
  pattern.voices[1].steps[4].hit = !kickAtZero;
}

void expectCanonicalRef(const MiniAcid& engine,
                        int row,
                        SongTrack track,
                        int expected) {
  assert(engine.songPatternAtSlot(0, row, track) == expected);
}

void expectSynthSelection(const MiniAcid& engine,
                          int voice,
                          int global,
                          int8_t note) {
  assert(engine.display303PatternIndex(voice) == global);
  assert(engine.current303BankIndex(voice) == songPatternBank(global));
  assert(engine.current303PatternIndex(voice) == songPatternIndexInBank(global));
  assert(engine.display303LocalPatternIndex(voice) ==
         songPatternIndexInBank(global));
  assert(engine.pattern303Steps(voice)[0] == note);
}

void expectDrumSelection(const MiniAcid& engine,
                         int global,
                         bool kickAtZero) {
  assert(engine.displayDrumPatternIndex() == global);
  assert(engine.currentDrumBankIndex() == songPatternBank(global));
  assert(engine.currentDrumPatternIndex() == songPatternIndexInBank(global));
  assert(engine.displayDrumLocalPatternIndex() == songPatternIndexInBank(global));
  assert(engine.patternKickSteps()[0] == kickAtZero);
}

void configureFixture(MiniAcid& engine) {
  clearP1cEffectivePatternRefOverrideForTest();
  SceneManager& manager = engine.sceneManager();
  manager.wipeToZero();

  engine.setCurrentPage(kPage);
  engine.setSongMode(false);
  engine.setActiveSongSlot(0);
  engine.setSongPlaybackSlot(0);
  engine.setSongLength(2);
  engine.setSongPosition(0);
  engine.set303BankIndex(0, kOldBank);
  engine.set303BankIndex(1, kOldBank);
  engine.setDrumBankIndex(kOldBank);
  engine.set303PatternIndex(0, kOldSlot);
  engine.set303PatternIndex(1, kOldSlot);
  engine.setDrumPatternIndex(kOldSlot);

  Scene& scene = manager.currentScene();
  writeSynthMaterial(scene, SongTrack::SynthA,
                     kOldBank, kOldSlot, kOldSynthANote);
  writeSynthMaterial(scene, SongTrack::SynthA,
                     kLeaseBank, kLeaseSlot, kLeaseSynthANote);
  writeSynthMaterial(scene, SongTrack::SynthB,
                     kOldBank, kOldSlot, kOldSynthBNote);
  writeSynthMaterial(scene, SongTrack::SynthB,
                     kLeaseBank, kLeaseSlot, kLeaseSynthBNote);
  writeDrumMaterial(scene, kOldBank, kOldSlot, false);
  writeDrumMaterial(scene, kLeaseBank, kLeaseSlot, true);

  const int oldRef = globalPattern(kOldBank, kOldSlot);
  for (int row = 0; row < 2; ++row) {
    engine.setSongPattern(row, SongTrack::SynthA,
                          static_cast<int16_t>(oldRef));
    engine.setSongPattern(row, SongTrack::SynthB,
                          static_cast<int16_t>(oldRef));
    engine.setSongPattern(row, SongTrack::Drums,
                          static_cast<int16_t>(oldRef));
  }

  // Existing setSongMode(true) publishes songMode_ after its internal selection
  // attempt. Re-applying the current position is the normal public selection
  // path and keeps this P1c test independent from that pre-existing ordering.
  engine.setSongMode(true);
  engine.setSongPosition(0);
}

void testNoOverrideIsIdentical(MiniAcid& engine) {
  configureFixture(engine);
  const int oldRef = globalPattern(kOldBank, kOldSlot);

  expectCanonicalRef(engine, 0, SongTrack::SynthA, oldRef);
  expectCanonicalRef(engine, 0, SongTrack::SynthB, oldRef);
  expectCanonicalRef(engine, 0, SongTrack::Drums, oldRef);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);
  expectSynthSelection(engine, 1, oldRef, kOldSynthBNote);
  expectDrumSelection(engine, oldRef, false);
}

void testSynthAOverrideDrivesMaterialAndReflection(MiniAcid& engine) {
  configureFixture(engine);
  const int oldRef = globalPattern(kOldBank, kOldSlot);
  const int leaseRef = globalPattern(kLeaseBank, kLeaseSlot);

  setP1cEffectivePatternRefOverrideForTest(
      0, 0, SongTrack::SynthA, static_cast<int16_t>(leaseRef));
  engine.setSongPosition(0);

  expectCanonicalRef(engine, 0, SongTrack::SynthA, oldRef);
  expectSynthSelection(engine, 0, leaseRef, kLeaseSynthANote);
  // Track identity is part of the key: the aligned Synth B/Drums lanes remain
  // on their canonical Song refs.
  expectSynthSelection(engine, 1, oldRef, kOldSynthBNote);
  expectDrumSelection(engine, oldRef, false);

  clearP1cEffectivePatternRefOverrideForTest();
  engine.setSongPosition(0);
  expectCanonicalRef(engine, 0, SongTrack::SynthA, oldRef);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);
}

void testSynthBOverrideDrivesMaterialAndReflection(MiniAcid& engine) {
  configureFixture(engine);
  const int oldRef = globalPattern(kOldBank, kOldSlot);
  const int leaseRef = globalPattern(kLeaseBank, kLeaseSlot);

  setP1cEffectivePatternRefOverrideForTest(
      0, 0, SongTrack::SynthB, static_cast<int16_t>(leaseRef));
  engine.setSongPosition(0);

  expectCanonicalRef(engine, 0, SongTrack::SynthB, oldRef);
  expectSynthSelection(engine, 1, leaseRef, kLeaseSynthBNote);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);

  clearP1cEffectivePatternRefOverrideForTest();
  engine.setSongPosition(0);
  expectSynthSelection(engine, 1, oldRef, kOldSynthBNote);
}

void testDrumOverrideDrivesMaterialAndReflection(MiniAcid& engine) {
  configureFixture(engine);
  const int oldRef = globalPattern(kOldBank, kOldSlot);
  const int leaseRef = globalPattern(kLeaseBank, kLeaseSlot);

  setP1cEffectivePatternRefOverrideForTest(
      0, 0, SongTrack::Drums, static_cast<int16_t>(leaseRef));
  engine.setSongPosition(0);

  expectCanonicalRef(engine, 0, SongTrack::Drums, oldRef);
  expectDrumSelection(engine, leaseRef, true);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);
  expectSynthSelection(engine, 1, oldRef, kOldSynthBNote);

  clearP1cEffectivePatternRefOverrideForTest();
  engine.setSongPosition(0);
  expectDrumSelection(engine, oldRef, false);
}

void testSharedPersistentRefDoesNotLeakAcrossRows(MiniAcid& engine) {
  configureFixture(engine);
  const int oldRef = globalPattern(kOldBank, kOldSlot);
  const int leaseRef = globalPattern(kLeaseBank, kLeaseSlot);

  // Rows 0 and 1 intentionally share the same canonical Pattern ref.
  expectCanonicalRef(engine, 0, SongTrack::SynthA, oldRef);
  expectCanonicalRef(engine, 1, SongTrack::SynthA, oldRef);

  setP1cEffectivePatternRefOverrideForTest(
      0, 0, SongTrack::SynthA, static_cast<int16_t>(leaseRef));

  engine.setSongPosition(0);
  expectSynthSelection(engine, 0, leaseRef, kLeaseSynthANote);

  engine.setSongPosition(1);
  expectCanonicalRef(engine, 1, SongTrack::SynthA, oldRef);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);

  // Returning to the keyed row re-applies the still-active effective view.
  engine.setSongPosition(0);
  expectSynthSelection(engine, 0, leaseRef, kLeaseSynthANote);
  expectCanonicalRef(engine, 0, SongTrack::SynthA, oldRef);

  clearP1cEffectivePatternRefOverrideForTest();
  engine.setSongPosition(0);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);
}

void testOverrideDoesNotLeakAcrossPlaybackSlots(MiniAcid& engine) {
  configureFixture(engine);
  const int oldRef = globalPattern(kOldBank, kOldSlot);
  const int leaseRef = globalPattern(kLeaseBank, kLeaseSlot);

  Scene& scene = engine.sceneManager().currentScene();
  scene.songs[1].length = 1;
  scene.songs[1].positions[0].patterns[static_cast<int>(SongTrack::SynthA)] =
      static_cast<int16_t>(oldRef);
  scene.songs[1].positions[0].patterns[static_cast<int>(SongTrack::SynthB)] =
      static_cast<int16_t>(oldRef);
  scene.songs[1].positions[0].patterns[static_cast<int>(SongTrack::Drums)] =
      static_cast<int16_t>(oldRef);

  setP1cEffectivePatternRefOverrideForTest(
      0, 0, SongTrack::SynthA, static_cast<int16_t>(leaseRef));
  engine.setSongPlaybackSlot(0);
  engine.setSongPosition(0);
  expectSynthSelection(engine, 0, leaseRef, kLeaseSynthANote);

  // Song B uses the same persistent ref, but the override is keyed to playback
  // slot A and therefore must not follow the ref into the other Song.
  engine.setSongPlaybackSlot(1);
  engine.setSongPosition(0);
  assert(engine.songPatternAtSlot(1, 0, SongTrack::SynthA) == oldRef);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);

  engine.setSongPlaybackSlot(0);
  engine.setSongPosition(0);
  expectSynthSelection(engine, 0, leaseRef, kLeaseSynthANote);

  clearP1cEffectivePatternRefOverrideForTest();
  engine.setSongPosition(0);
  expectSynthSelection(engine, 0, oldRef, kOldSynthANote);
}

}  // namespace

int main() {
  MiniAcid engine(kSampleRate, nullptr);

  testNoOverrideIsIdentical(engine);
  testSynthAOverrideDrivesMaterialAndReflection(engine);
  testSynthBOverrideDrivesMaterialAndReflection(engine);
  testDrumOverrideDrivesMaterialAndReflection(engine);
  testSharedPersistentRefDoesNotLeakAcrossRows(engine);
  testOverrideDoesNotLeakAcrossPlaybackSlots(engine);

  clearP1cEffectivePatternRefOverrideForTest();
  std::puts("0.9.9-P1c effective Song Pattern ref tests passed");
  return 0;
}
