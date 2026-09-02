#include <cstdio>

#include "src/dsp/phrase_generator.h"
#include "src/dsp/song_pattern_materializer.h"

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
  }
}

void setPhraseReference(Scene& scene,
                        SongTrack track,
                        int globalPattern,
                        PhraseCore::Source source = PhraseCore::Source::Generated) {
  PhraseCore::reset(scene.phraseBank);
  PhraseCore::PhraseSlot& phrase = scene.phraseBank.slots[0];
  const int trackIndex = SongPatternMaterializer::editableTrackIndex(track);
  phrase.metadata.phraseId = 1;
  phrase.metadata.lengthBars = 1;
  phrase.metadata.role = PhraseCore::Role::Main;
  phrase.metadata.source = source;
  phrase.metadata.storage = PhraseCore::StorageMode::ReferenceView;
  phrase.metadata.flags = PhraseCore::kFlagValid |
                          PhraseCore::kFlagReferenceView |
                          PhraseCore::kFlagMutableBacking;
  phrase.metadata.sourceSongSlot = 0;
  phrase.metadata.sourceStartRow = 0;
  phrase.metadata.trackMask = PhraseCore::maskForTrackIndex(trackIndex);
  phrase.patternRefs[0][trackIndex] = static_cast<int16_t>(globalPattern);
}

void testSongReferenceKeepsPatternAlive() {
  Scene scene{};
  scene.songs[0].positions[0].patterns[0] = 0;
  expect(PhraseGenerator::globalPatternIsReferenced(scene, 0),
         "Song reference did not keep Pattern alive");
}

void testPhraseReferenceKeepsPatternAlive() {
  Scene scene{};
  setPhraseReference(scene, SongTrack::SynthA, 0);
  expect(PhraseGenerator::globalPatternIsReferenced(scene, 0),
         "Phrase reference did not keep Pattern alive");
  expect(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, 0),
         "Phrase-only referenced empty Pattern slot was reclaimable");
}

void testSharedReferenceAndRemoval() {
  Scene scene{};
  scene.songs[0].positions[0].patterns[0] = 0;
  setPhraseReference(scene, SongTrack::SynthA, 0);
  expect(SongPatternMaterializer::globalPatternReferenceCount(
             scene, SongTrack::SynthA, 0) == 2,
         "Song + Phrase shared reference count was not two");

  scene.songs[0].positions[0].patterns[0] = -1;
  expect(PhraseGenerator::globalPatternIsReferenced(scene, 0),
         "removing Song reference reclaimed Pattern still owned by Phrase");

  PhraseCore::reset(scene.phraseBank);
  expect(!PhraseGenerator::globalPatternIsReferenced(scene, 0),
         "Pattern with no Song/Phrase references remained live");
}

void testUnrelatedPatternUnaffected() {
  Scene scene{};
  setPhraseReference(scene, SongTrack::Drums, 3);
  expect(PhraseGenerator::globalPatternIsReferenced(scene, 3),
         "Drums Phrase reference did not keep exact Pattern alive");
  expect(!PhraseGenerator::globalPatternIsReferenced(scene, 4),
         "liveness leaked to unrelated Pattern identity");
}

void testAllPersistedReferenceSourcesAreRoots() {
  const PhraseCore::Source sources[] = {
      PhraseCore::Source::InternalPattern,
      PhraseCore::Source::Generated,
      PhraseCore::Source::Derived,
  };
  for (const PhraseCore::Source source : sources) {
    Scene scene{};
    setPhraseReference(scene, SongTrack::SynthB, 2, source);
    expect(PhraseGenerator::globalPatternIsReferenced(scene, 2),
           "valid persisted Phrase reference source was not a liveness root");
  }
}

}  // namespace

int main() {
  testSongReferenceKeepsPatternAlive();
  testPhraseReferenceKeepsPatternAlive();
  testSharedReferenceAndRemoval();
  testUnrelatedPatternUnaffected();
  testAllPersistedReferenceSourcesAreRoots();

  if (g_failures != 0) {
    std::fprintf(stderr, "0.9.9-D1 liveness failures: %d\n", g_failures);
    return 1;
  }
  std::puts("0.9.9-D1 Pattern/Phrase liveness: PASS");
  return 0;
}
