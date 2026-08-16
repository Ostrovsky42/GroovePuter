#include "../platform_sdl/arduino_compat.h"
#include "../src/audio/pattern_paging.h"
#include "../src/dsp/song_pattern_materializer.h"

#include <cassert>
#include <filesystem>
#include <string>

SerialMock Serial;
SDMock SD;

Scene& sceneTransactionScratch() {
    static Scene scratch{};
    return scratch;
}

namespace {

constexpr int kPage = 5;
constexpr int kSynthLocalSlot = 2;
constexpr int kDrumLocalSlot = 11;

void selectEmptyProject(const std::string& project) {
    assert(PatternPagingService::setProjectName(project));
    assert(PatternPagingService::clearProjectPages());
}

void setOwnershipMarkers(Scene& scene) {
    const int synthBank = kSynthLocalSlot / Bank<SynthPattern>::kPatterns;
    const int synthIndex = kSynthLocalSlot % Bank<SynthPattern>::kPatterns;
    scene.synthABanks[synthBank].patterns[synthIndex].steps[0].note = 52;
    SongPatternMaterializer::markSlotSongGenerated(
        scene, SongTrack::SynthA, kSynthLocalSlot);

    const int drumBank = kDrumLocalSlot / Bank<DrumPatternSet>::kPatterns;
    const int drumIndex = kDrumLocalSlot % Bank<DrumPatternSet>::kPatterns;
    scene.drumBanks[drumBank].patterns[drumIndex].voices[0].steps[0].hit = true;
    SongPatternMaterializer::markSlotSongGenerated(
        scene, SongTrack::Drums, kDrumLocalSlot);
}

void verifyOwnershipMarkers(const Scene& scene) {
    assert(SongPatternMaterializer::slotIsSongGenerated(
        scene, SongTrack::SynthA, kSynthLocalSlot));
    assert(SongPatternMaterializer::slotIsSongGenerated(
        scene, SongTrack::Drums, kDrumLocalSlot));
}

void testRoundTrip() {
    selectEmptyProject("ownership-roundtrip");
    Scene source{};
    setOwnershipMarkers(source);
    assert(PatternPagingService::savePage(kPage, source));

    Scene loaded{};
    assert(PatternPagingService::loadPage(kPage, loaded));
    verifyOwnershipMarkers(loaded);
}

void testProjectIsolationAndReload() {
    selectEmptyProject("ownership-a");
    Scene source{};
    setOwnershipMarkers(source);
    assert(PatternPagingService::savePage(kPage, source));

    selectEmptyProject("ownership-b");
    Scene other{};
    assert(PatternPagingService::savePage(kPage, other));

    assert(PatternPagingService::setProjectName("ownership-a"));
    Scene loadedA{};
    assert(PatternPagingService::loadPage(kPage, loadedA));
    verifyOwnershipMarkers(loadedA);

    assert(PatternPagingService::setProjectName("ownership-b"));
    Scene loadedB{};
    assert(PatternPagingService::loadPage(kPage, loadedB));
    assert(!SongPatternMaterializer::slotIsSongGenerated(
        loadedB, SongTrack::SynthA, kSynthLocalSlot));
    assert(!SongPatternMaterializer::slotIsSongGenerated(
        loadedB, SongTrack::Drums, kDrumLocalSlot));
}

void testBackupRecovery() {
    selectEmptyProject("ownership-backup");
    Scene first{};
    setOwnershipMarkers(first);
    assert(PatternPagingService::savePage(kPage, first));

    Scene second{};
    assert(PatternPagingService::savePage(kPage, second));

    const std::filesystem::path root = SD.root();
    char fileName[32];
    std::snprintf(fileName, sizeof(fileName), "page_%02d.gpp", kPage);
    const std::filesystem::path main =
        root / "patterns" / "ownership-backup" / fileName;

    std::fstream file(main, std::ios::binary | std::ios::in | std::ios::out);
    assert(file.is_open());
    file.seekg(-1, std::ios::end);
    char value = 0;
    file.read(&value, 1);
    value ^= static_cast<char>(0x5A);
    file.seekp(-1, std::ios::end);
    file.write(&value, 1);
    file.close();

    Scene recovered{};
    assert(PatternPagingService::loadPage(kPage, recovered));
    verifyOwnershipMarkers(recovered);
}

}  // namespace

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        "grooveputer-pattern-ownership-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    SD.setRoot(root);

    testRoundTrip();
    testProjectIsolationAndReload();
    testBackupRecovery();

    std::filesystem::remove_all(root, ec);
    return 0;
}
