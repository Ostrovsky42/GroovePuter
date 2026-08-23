#include "f08_listen_fixture_player.h"

#include "../../dsp/miniacid_engine.h"
#include "f08_listen_fixture_generated.h"

namespace GroovePuterRhythm {
namespace {

constexpr int kReviewBank = 1;
constexpr int kReviewPattern = 0;
constexpr int kReviewSongSlot = 1;

bool decodeDrums(const uint8_t* source,
                 uint16_t size,
                 DrumPatternSet& destination) {
  constexpr uint16_t kExpected =
      DrumPatternSet::kVoices * DrumPattern::kSteps * 7;
  if (source == nullptr || size != kExpected) return false;

  destination = DrumPatternSet{};
  uint16_t offset = 0;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      DrumStep& event = destination.voices[voice].steps[step];
      event.hit = source[offset++] != 0;
      event.accent = source[offset++] != 0;
      event.velocity = source[offset++];
      event.timing = static_cast<int8_t>(source[offset++]);
      event.fx = source[offset++];
      event.fxParam = source[offset++];
      event.probability = source[offset++];
    }
  }
  return offset == size;
}

bool decodeSynth(const uint8_t* source,
                 uint16_t size,
                 SynthPattern& destination) {
  constexpr uint16_t kExpected = SynthPattern::kSteps * 9;
  if (source == nullptr || size != kExpected) return false;

  destination = SynthPattern{};
  uint16_t offset = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    SynthStep& event = destination.steps[step];
    event.note = static_cast<int8_t>(source[offset++]);
    event.slide = source[offset++] != 0;
    event.accent = source[offset++] != 0;
    event.ghost = source[offset++] != 0;
    event.velocity = source[offset++];
    event.timing = static_cast<int8_t>(source[offset++]);
    event.fx = source[offset++];
    event.fxParam = source[offset++];
    event.probability = source[offset++];
  }
  return offset == size;
}

}  // namespace

uint8_t f08ListenCaseCount() {
  return F08ListenFixtureData::kCaseCount;
}

F08ListenCaseInfo f08ListenCaseInfo(uint8_t index) {
  F08ListenCaseInfo result{};
  if (index >= F08ListenFixtureData::kCaseCount) return result;

  const auto& source = F08ListenFixtureData::kCases[index];
  result.group = source.group;
  result.mode = source.mode;
  result.ordinal = source.ordinal;
  result.voice = source.voice;
  result.focus = source.focus;
  result.progression = source.progression;
  result.oldClock = source.oldClock;
  result.newClock = source.newClock;
  result.bpm = source.bpm;
  result.fingerprintChanged = source.fingerprintChanged;
  return result;
}

bool applyF08ListenCase(MiniAcid& engine,
                        uint8_t index,
                        F08ListenVariant variant) {
  if (index >= F08ListenFixtureData::kCaseCount) return false;

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  const uint8_t* synthASource =
      variant == F08ListenVariant::Old
          ? F08ListenFixtureData::kOldSynthA[index]
          : F08ListenFixtureData::kNewSynthA[index];
  const uint8_t* synthBSource =
      variant == F08ListenVariant::Old
          ? F08ListenFixtureData::kOldSynthB[index]
          : F08ListenFixtureData::kNewSynthB[index];

  if (!decodeDrums(F08ListenFixtureData::kDrums[index],
                   F08ListenFixtureData::kDrumBytes,
                   drums) ||
      !decodeSynth(synthASource,
                   F08ListenFixtureData::kSynthBytes,
                   synthA) ||
      !decodeSynth(synthBSource,
                   F08ListenFixtureData::kSynthBytes,
                   synthB)) {
    return false;
  }

  const auto& meta = F08ListenFixtureData::kCases[index];
  SceneManager& manager = engine.sceneManager();

  // Review fixtures intentionally live only in the same destructive sandbox
  // used by other explicit audition tooling: current-page Bank B, pattern 1,
  // and Song B. Production generation is never called here.
  engine.stop();
  engine.setSongMode(false);
  engine.setDrumBankIndex(kReviewBank);
  engine.setDrumPatternIndex(kReviewPattern);
  engine.set303BankIndex(0, kReviewBank);
  engine.set303BankIndex(1, kReviewBank);
  engine.set303PatternIndex(0, kReviewPattern);
  engine.set303PatternIndex(1, kReviewPattern);

  manager.editDrumPatternSet(kReviewPattern) = drums;
  manager.editSynthPattern(0, kReviewPattern) = synthA;
  manager.editSynthPattern(1, kReviewPattern) = synthB;

  const int patternAddress = songPatternFromPageBankIndex(
      engine.currentPageIndex(), kReviewBank, kReviewPattern);
  if (patternAddress < 0) return false;

  engine.setActiveSongSlot(kReviewSongSlot);
  engine.setSongPattern(0, SongTrack::SynthA, patternAddress);
  engine.setSongPattern(0, SongTrack::SynthB, patternAddress);
  engine.setSongPattern(0, SongTrack::Drums, patternAddress);
  engine.setSongLength(1);
  engine.setSongPosition(0);
  engine.setLoopRange(0, 0);
  engine.setLoopMode(true);
  engine.setSongPlaybackSlot(kReviewSongSlot);
  engine.setBpm(static_cast<float>(meta.bpm));
  engine.setSongMode(true);
  engine.start();
  return true;
}

}  // namespace GroovePuterRhythm
