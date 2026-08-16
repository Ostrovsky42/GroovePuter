#include <cstdint>
#include <cstdio>

#include "src/dsp/song_pattern_materializer.h"

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++g_failures;
    }
}

void occupySynth(SynthPattern& pattern, int note) {
    pattern.steps[0].note = static_cast<int8_t>(note);
    pattern.steps[0].velocity = 101;
}

void fillSynthAPage(Scene& scene) {
    for (int localSlot = 0; localSlot < kPatternsPerPage; ++localSlot) {
        const int bank = localSlot / Bank<SynthPattern>::kPatterns;
        const int index = localSlot % Bank<SynthPattern>::kPatterns;
        occupySynth(scene.synthABanks[bank].patterns[index], 36 + localSlot);
    }
}

void setReferencePhrase(Scene& scene,
                        PhraseCore::Source source,
                        int globalPattern) {
    PhraseCore::reset(scene.phraseBank);
    PhraseCore::PhraseSlot& phrase = scene.phraseBank.slots[0];
    phrase.metadata.phraseId = 1;
    phrase.metadata.parentId = source == PhraseCore::Source::Derived ? 2 : 0;
    phrase.metadata.lengthBars = 1;
    phrase.metadata.role = PhraseCore::Role::Main;
    phrase.metadata.source = source;
    phrase.metadata.storage = PhraseCore::StorageMode::ReferenceView;
    phrase.metadata.flags = PhraseCore::kFlagValid |
                            PhraseCore::kFlagReferenceView |
                            PhraseCore::kFlagMutableBacking;
    phrase.metadata.sourceSongSlot = 0;
    phrase.metadata.sourceStartRow = 0;
    phrase.metadata.trackMask = PhraseCore::kTrackSynthA;
    phrase.patternRefs[0][0] = static_cast<int16_t>(globalPattern);
}

void testPhraseOnlyReferenceProtectsGeneratedSlot(PhraseCore::Source source) {
    Scene scene{};
    fillSynthAPage(scene);

    constexpr int kProtectedLocalSlot = 3;
    const int bank = kProtectedLocalSlot / Bank<SynthPattern>::kPatterns;
    const int index = kProtectedLocalSlot % Bank<SynthPattern>::kPatterns;
    const int protectedGlobal = songPatternFromPageBankIndex(0, bank, index);
    SongPatternMaterializer::markSlotSongGenerated(
        scene, SongTrack::SynthA, kProtectedLocalSlot);
    setReferencePhrase(scene, source, protectedGlobal);

    expect(SongPatternMaterializer::globalPatternReferenceCount(
               scene, SongTrack::SynthA, protectedGlobal) == 1,
           "Phrase-only Pattern reference was not counted");

    SongPatternMaterializer::Request request{};
    request.row = 1;
    request.pageIndex = 0;
    request.trackMask = SongPatternMaterializer::kSynthAMask;
    request.preferredLocalSlot[0] = kProtectedLocalSlot;
    bool reusedGenerated = false;
    const int reusable = SongPatternMaterializer::findReusableLocalSlot(
        scene,
        request,
        SongTrack::SynthA,
        kProtectedLocalSlot,
        reusedGenerated);

    expect(reusable == -1,
           "Phrase-only generated Pattern was reclaimed as an orphan");
    expect(!reusedGenerated,
           "Phrase-only generated Pattern was reported as reusable");
}

void testSongAndPhraseReferenceForcesCopyOnWrite() {
    Scene scene{};
    PhraseCore::reset(scene.phraseBank);

    constexpr int kSharedLocalSlot = 0;
    occupySynth(scene.synthABanks[0].patterns[kSharedLocalSlot], 48);
    SongPatternMaterializer::markSlotSongGenerated(
        scene, SongTrack::SynthA, kSharedLocalSlot);
    scene.songs[0].positions[0].patterns[0] = 0;
    scene.songs[0].length = 1;
    setReferencePhrase(scene, PhraseCore::Source::Generated, 0);

    expect(SongPatternMaterializer::globalPatternReferenceCount(
               scene, SongTrack::SynthA, 0) == 2,
           "Song + Phrase references did not share the same liveness count");

    SongPatternMaterializer::Request request{};
    request.row = 0;
    request.pageIndex = 0;
    request.trackMask = SongPatternMaterializer::kSynthAMask;
    request.preferredLocalSlot[0] = 1;
    bool reusedGenerated = false;
    const int reusable = SongPatternMaterializer::findReusableLocalSlot(
        scene, request, SongTrack::SynthA, 1, reusedGenerated);

    expect(reusable == 1,
           "shared Song/Phrase Pattern did not use copy-on-write destination");
    expect(!reusedGenerated,
           "copy-on-write destination was incorrectly reported as generated reuse");
}

}  // namespace

int main() {
    testPhraseOnlyReferenceProtectsGeneratedSlot(
        PhraseCore::Source::InternalPattern);
    testPhraseOnlyReferenceProtectsGeneratedSlot(
        PhraseCore::Source::Generated);
    testPhraseOnlyReferenceProtectsGeneratedSlot(
        PhraseCore::Source::Derived);
    testSongAndPhraseReferenceForcesCopyOnWrite();

    if (g_failures != 0) {
        std::fprintf(stderr, "Phrase liveness tests failed: %d\n", g_failures);
        return 1;
    }
    std::puts("Phrase liveness tests passed");
    return 0;
}
