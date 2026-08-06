#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/dsp/song_pattern_materializer.h"

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++g_failures;
    }
}

void resetRevision(uint32_t revision = 10) {
    GroovePuterState::SceneRevisionState state{};
    state.currentRevision = revision;
    state.persistedRevision = revision;
    GroovePuterState::restoreSceneRevision(state);
}

uint32_t generatorDomain(SongTrack track) {
    switch (track) {
        case SongTrack::SynthA: return 0x13579BDFu;
        case SongTrack::SynthB: return 0x2468ACE1u;
        case SongTrack::Drums: return 0xD12F00D5u;
        case SongTrack::Voice: break;
    }
    return 0;
}

struct TestGenerator {
    int failOnCall = 0;
    int calls = 0;

    bool operator()(SongTrack track,
                    uint32_t seed,
                    SynthPattern& synth,
                    DrumPatternSet& drums) {
        ++calls;
        if (failOnCall > 0 && calls == failOnCall) return false;

        DeterministicRng rng(seed ^ generatorDomain(track));
        if (track == SongTrack::SynthA || track == SongTrack::SynthB) {
            const int step = static_cast<int>(rng.bounded(SynthPattern::kSteps));
            synth.steps[step].note = static_cast<int8_t>(
                (track == SongTrack::SynthA ? 36 : 60) + rng.bounded(12));
            synth.steps[step].velocity =
                static_cast<uint8_t>(90 + rng.bounded(38));
            synth.steps[step].accent = rng.bounded(2) != 0;
            return true;
        }
        if (track == SongTrack::Drums) {
            const int hatStep = static_cast<int>(rng.bounded(8) * 2);
            drums.voices[0].steps[0].hit = true;
            drums.voices[0].steps[0].velocity = 120;
            drums.voices[2].steps[hatStep].hit = true;
            drums.voices[2].steps[hatStep].velocity =
                static_cast<uint8_t>(70 + rng.bounded(31));
            return true;
        }
        return false;
    }
};

SongPatternMaterializer::Request requestFor(
        int row, uint8_t mask, uint32_t seed = 0x12345678u) {
    SongPatternMaterializer::Request request{};
    request.row = row;
    request.pageIndex = 0;
    request.seed = seed;
    request.modeTag = 1;
    request.trackMask = mask;
    request.preferredLocalSlot[0] = 0;
    request.preferredLocalSlot[1] = 0;
    request.preferredLocalSlot[2] = 0;
    return request;
}

const SynthPattern& synthAtGlobal(
        const Scene& scene, SongTrack track, int globalPattern) {
    const int bank = songPatternBank(globalPattern);
    const int index = songPatternIndexInBank(globalPattern);
    return track == SongTrack::SynthA
        ? scene.synthABanks[bank].patterns[index]
        : scene.synthBBanks[bank].patterns[index];
}

const DrumPatternSet& drumsAtGlobal(
        const Scene& scene, int globalPattern) {
    const int bank = songPatternBank(globalPattern);
    const int index = songPatternIndexInBank(globalPattern);
    return scene.drumBanks[bank].patterns[index];
}

template <typename T>
bool byteEqual(const T& left, const T& right) {
    return std::memcmp(&left, &right, sizeof(T)) == 0;
}

void occupySynth(SynthPattern& pattern, int note) {
    pattern.steps[0].note = static_cast<int8_t>(note);
    pattern.steps[0].velocity = 101;
}

void occupyDrums(DrumPatternSet& pattern) {
    pattern.voices[0].steps[0].hit = true;
    pattern.voices[0].steps[0].velocity = 111;
}

void testSingleTrack(SongTrack track, uint8_t mask) {
    Scene scene{};
    resetRevision();

    occupySynth(scene.synthABanks[0].patterns[5], 41);
    occupySynth(scene.synthBBanks[0].patterns[5], 65);
    occupyDrums(scene.drumBanks[0].patterns[5]);
    const Scene before = scene;
    const auto revisionBefore = GroovePuterState::sceneRevisionSnapshot();

    TestGenerator generator{};
    const auto result = SongPatternMaterializer::generate(
        scene,
        requestFor(3, mask),
        generator,
        [](auto&& apply) { apply(); });

    expect(static_cast<bool>(result), "single-cell generation failed");
    expect(result.generatedTracks == 1,
           "single-cell generation committed more than one track");
    const int trackIndex = SongPatternMaterializer::editableTrackIndex(track);
    const int globalPattern = result.globalPattern[trackIndex];
    expect(globalPattern >= 0, "single-cell result has no pattern reference");
    expect(scene.songs[0].positions[3].patterns[trackIndex] == globalPattern,
           "Song cell did not receive generated pattern reference");

    if (track == SongTrack::SynthA || track == SongTrack::SynthB) {
        expect(!SongPatternMaterializer::synthPatternIsStrictlyEmpty(
                   synthAtGlobal(scene, track, globalPattern)),
               "generated synth destination is empty");
    } else {
        expect(!SongPatternMaterializer::drumPatternSetIsStrictlyEmpty(
                   drumsAtGlobal(scene, globalPattern)),
               "generated drum destination is empty");
    }

    if (track != SongTrack::SynthA) {
        expect(std::memcmp(scene.synthABanks, before.synthABanks,
                           sizeof(scene.synthABanks)) == 0,
               "Synth A changed while generating another track");
    }
    if (track != SongTrack::SynthB) {
        expect(std::memcmp(scene.synthBBanks, before.synthBBanks,
                           sizeof(scene.synthBBanks)) == 0,
               "Synth B changed while generating another track");
    }
    if (track != SongTrack::Drums) {
        expect(std::memcmp(scene.drumBanks, before.drumBanks,
                           sizeof(scene.drumBanks)) == 0,
               "Drums changed while generating another track");
    }

    expect(byteEqual(scene.synthABanks[0].patterns[5],
                     before.synthABanks[0].patterns[5]),
           "occupied Synth A slot changed");
    expect(byteEqual(scene.synthBBanks[0].patterns[5],
                     before.synthBBanks[0].patterns[5]),
           "occupied Synth B slot changed");
    expect(byteEqual(scene.drumBanks[0].patterns[5],
                     before.drumBanks[0].patterns[5]),
           "occupied drum slot changed");

    const auto revisionAfter = GroovePuterState::sceneRevisionSnapshot();
    expect(revisionAfter.currentRevision == revisionBefore.currentRevision + 1,
           "successful single-cell generation must be one logical mutation");
}

void testCopyOnWrite() {
    Scene scene{};
    resetRevision();
    occupySynth(scene.synthABanks[0].patterns[0], 48);
    const SynthPattern original = scene.synthABanks[0].patterns[0];
    scene.songs[0].positions[0].patterns[0] = 0;
    scene.songs[0].positions[1].patterns[0] = 0;
    scene.songs[0].length = 2;

    TestGenerator generator{};
    const auto result = SongPatternMaterializer::generate(
        scene,
        requestFor(0, SongPatternMaterializer::kSynthAMask),
        generator,
        [](auto&& apply) { apply(); });

    expect(static_cast<bool>(result), "copy-on-write generation failed");
    expect(scene.songs[0].positions[0].patterns[0] != 0,
           "edited cell still references shared source pattern");
    expect(scene.songs[0].positions[1].patterns[0] == 0,
           "unselected cell lost shared source reference");
    expect(byteEqual(scene.synthABanks[0].patterns[0], original),
           "copy-on-write modified the shared source pattern");
}

void testDeterminism() {
    Scene first{};
    Scene second{};
    resetRevision();
    TestGenerator firstGenerator{};
    const auto firstResult = SongPatternMaterializer::generate(
        first,
        requestFor(4, SongPatternMaterializer::kSynthAMask, 0xABCDEF01u),
        firstGenerator,
        [](auto&& apply) { apply(); });
    resetRevision();
    TestGenerator secondGenerator{};
    const auto secondResult = SongPatternMaterializer::generate(
        second,
        requestFor(4, SongPatternMaterializer::kSynthAMask, 0xABCDEF01u),
        secondGenerator,
        [](auto&& apply) { apply(); });

    expect(firstResult.globalPattern[0] == secondResult.globalPattern[0],
           "same seed allocated different Song references");
    expect(byteEqual(
               synthAtGlobal(first, SongTrack::SynthA,
                             firstResult.globalPattern[0]),
               synthAtGlobal(second, SongTrack::SynthA,
                             secondResult.globalPattern[0])),
           "same seed generated different pattern bytes");
}

void runOrder(Scene& scene, const std::array<SongTrack, 3>& order) {
    for (SongTrack track : order) {
        TestGenerator generator{};
        const uint8_t mask = SongPatternMaterializer::maskForTrack(track);
        const auto result = SongPatternMaterializer::generate(
            scene,
            requestFor(7, mask, 0x10203040u),
            generator,
            [](auto&& apply) { apply(); });
        expect(static_cast<bool>(result), "ordered track generation failed");
    }
}

void testOrderIndependence() {
    Scene forward{};
    Scene reverse{};
    resetRevision();
    runOrder(forward, {SongTrack::SynthA, SongTrack::SynthB, SongTrack::Drums});
    resetRevision();
    runOrder(reverse, {SongTrack::Drums, SongTrack::SynthB, SongTrack::SynthA});

    for (int track = 0; track < 3; ++track) {
        expect(forward.songs[0].positions[7].patterns[track] ==
                   reverse.songs[0].positions[7].patterns[track],
               "track order changed Song reference allocation");
    }
    expect(std::memcmp(forward.synthABanks, reverse.synthABanks,
                       sizeof(forward.synthABanks)) == 0,
           "track order changed Synth A result");
    expect(std::memcmp(forward.synthBBanks, reverse.synthBBanks,
                       sizeof(forward.synthBBanks)) == 0,
           "track order changed Synth B result");
    expect(std::memcmp(forward.drumBanks, reverse.drumBanks,
                       sizeof(forward.drumBanks)) == 0,
           "track order changed drum result");
}

void testNoFreeSlot() {
    Scene scene{};
    for (int bank = 0; bank < kBankCount; ++bank) {
        for (int pattern = 0; pattern < Bank<SynthPattern>::kPatterns; ++pattern) {
            occupySynth(scene.synthABanks[bank].patterns[pattern], 40 + pattern);
        }
    }
    resetRevision(23);
    const Scene before = scene;
    const auto revisionBefore = GroovePuterState::sceneRevisionSnapshot();

    TestGenerator generator{};
    const auto result = SongPatternMaterializer::generate(
        scene,
        requestFor(2, SongPatternMaterializer::kSynthAMask),
        generator,
        [](auto&& apply) { apply(); });

    expect(result.error ==
               SongPatternMaterializer::Error::NoEmptyPatternSlots,
           "full bank did not return NoEmptyPatternSlots");
    expect(byteEqual(scene, before), "full-bank failure changed Scene bytes");
    expect(byteEqual(GroovePuterState::sceneRevisionSnapshot(), revisionBefore),
           "full-bank failure changed dirty revision");
}

void testReferencedEmptySlotIsNotFree() {
    Scene scene{};
    scene.songs[1].positions[9].patterns[0] = 0;
    scene.songs[1].length = 10;
    resetRevision();

    TestGenerator generator{};
    const auto result = SongPatternMaterializer::generate(
        scene,
        requestFor(0, SongPatternMaterializer::kSynthAMask),
        generator,
        [](auto&& apply) { apply(); });

    expect(static_cast<bool>(result), "generation with referenced empty slot failed");
    expect(result.globalPattern[0] == 1,
           "referenced empty slot was overwritten instead of skipped");
    expect(scene.songs[1].positions[9].patterns[0] == 0,
           "other Song slot reference changed");
}

void testRowCommitAndRollback(int failOnCall) {
    Scene scene{};
    scene.activeSongSlot = 1;
    scene.songs[0].positions[6].patterns[0] = 7;
    scene.songs[0].length = 7;
    resetRevision(40);
    const Scene before = scene;
    const auto revisionBefore = GroovePuterState::sceneRevisionSnapshot();

    TestGenerator generator{};
    generator.failOnCall = failOnCall;
    const auto result = SongPatternMaterializer::generate(
        scene,
        requestFor(6, SongPatternMaterializer::kEditableTrackMask,
                   0x55667788u),
        generator,
        [](auto&& apply) { apply(); });

    if (failOnCall > 0) {
        expect(result.error == SongPatternMaterializer::Error::GenerationFailed,
               "injected row failure returned wrong error");
        expect(byteEqual(scene, before),
               "row generation failure left partial Scene changes");
        expect(byteEqual(GroovePuterState::sceneRevisionSnapshot(),
                         revisionBefore),
               "row generation failure changed dirty revision");
        return;
    }

    expect(static_cast<bool>(result), "row generation failed");
    expect(result.generatedTracks == 3,
           "row generation did not commit all editable tracks");
    expect(byteEqual(scene.songs[0], before.songs[0]),
           "row generation changed inactive Song A");
    for (int track = 0; track < 3; ++track) {
        expect(scene.songs[1].positions[6].patterns[track] >= 0,
               "row generation left an editable track empty");
    }
    const auto revisionAfter = GroovePuterState::sceneRevisionSnapshot();
    expect(revisionAfter.currentRevision == revisionBefore.currentRevision + 1,
           "successful row generation must be one logical mutation");
}

}  // namespace

int main() {
    testSingleTrack(SongTrack::SynthA, SongPatternMaterializer::kSynthAMask);
    testSingleTrack(SongTrack::SynthB, SongPatternMaterializer::kSynthBMask);
    testSingleTrack(SongTrack::Drums, SongPatternMaterializer::kDrumsMask);
    testCopyOnWrite();
    testDeterminism();
    testOrderIndependence();
    testNoFreeSlot();
    testReferencedEmptySlotIsNotFree();
    testRowCommitAndRollback(0);
    testRowCommitAndRollback(2);
    testRowCommitAndRollback(3);

    if (g_failures != 0) {
        std::fprintf(stderr, "Song materializer tests failed: %d\n", g_failures);
        return 1;
    }
    std::puts("Song materializer tests passed");
    return 0;
}
