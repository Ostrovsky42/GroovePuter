#include "../platform_sdl/arduino_compat.h"
#include "../src/audio/pattern_paging.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

SerialMock Serial;
SDMock SD;

namespace {

constexpr int kPage = 3;

std::filesystem::path pageFile(const std::filesystem::path& root,
                               const std::string& project,
                               int page = kPage) {
  char fileName[32];
  std::snprintf(fileName, sizeof(fileName), "page_%02d.gpp", page);
  return root / "patterns" / project / fileName;
}

std::filesystem::path legacyPageFile(const std::filesystem::path& root,
                                     int page = kPage) {
  char fileName[32];
  std::snprintf(fileName, sizeof(fileName), "page_%02d.gpp", page);
  return root / "patterns" / fileName;
}

void selectEmptyProject(const std::string& project) {
  assert(PatternPagingService::setProjectName(project));
  assert(PatternPagingService::clearProjectPages());
}

void setMarker(Scene& scene, int note, int velocity, int timing) {
  SynthStep& synth = scene.synthABanks[1].patterns[7].steps[15];
  synth.note = static_cast<int8_t>(note);
  synth.velocity = static_cast<uint8_t>(velocity);
  synth.timing = static_cast<int8_t>(timing);
  synth.ghost = true;

  DrumStep& drum = scene.drumBanks[1].patterns[6].voices[7].steps[14];
  drum.hit = true;
  drum.velocity = static_cast<uint8_t>(velocity - 1);
  drum.timing = static_cast<int8_t>(-timing);
}

void verifyMarker(const Scene& scene, int note, int velocity, int timing) {
  const SynthStep& synth = scene.synthABanks[1].patterns[7].steps[15];
  assert(synth.note == note);
  assert(synth.velocity == velocity);
  assert(synth.timing == timing);
  assert(synth.ghost);

  const DrumStep& drum =
      scene.drumBanks[1].patterns[6].voices[7].steps[14];
  assert(drum.hit);
  assert(drum.velocity == velocity - 1);
  assert(drum.timing == -timing);
}

void corruptPayload(const std::filesystem::path& path) {
  std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
  assert(file.is_open());
  file.seekg(-1, std::ios::end);
  char value = 0;
  file.read(&value, 1);
  value ^= static_cast<char>(0x5A);
  file.seekp(-1, std::ios::end);
  file.write(&value, 1);
  file.flush();
}

void testRoundTrip(const std::filesystem::path& root) {
  selectEmptyProject("roundtrip");
  Scene source{};
  setMarker(source, 64, 89, 12);
  assert(PatternPagingService::savePage(kPage, source));
  assert(PatternPagingService::pageExists(kPage));
  assert(PatternPagingService::activePageIndex() == kPage);

  Scene target{};
  setMarker(target, 31, 22, -7);
  assert(PatternPagingService::loadPage(kPage, target));
  verifyMarker(target, 64, 89, 12);
  assert(std::filesystem::exists(pageFile(root, "roundtrip")));
}

void testCorruptOnlyCopyLeavesSceneUntouched(
    const std::filesystem::path& root) {
  selectEmptyProject("corrupt");
  Scene source{};
  setMarker(source, 67, 91, 9);
  assert(PatternPagingService::savePage(kPage, source));
  corruptPayload(pageFile(root, "corrupt"));

  Scene active{};
  setMarker(active, 42, 55, -4);
  assert(!PatternPagingService::loadPage(kPage, active));
  verifyMarker(active, 42, 55, -4);
}

void testBackupRecovery(const std::filesystem::path& root) {
  selectEmptyProject("backup");
  Scene first{};
  setMarker(first, 50, 70, 5);
  assert(PatternPagingService::savePage(kPage, first));

  Scene second{};
  setMarker(second, 72, 101, 14);
  assert(PatternPagingService::savePage(kPage, second));
  assert(std::filesystem::exists(pageFile(root, "backup").string() + ".bak"));

  corruptPayload(pageFile(root, "backup"));

  Scene active{};
  setMarker(active, 20, 30, -3);
  assert(PatternPagingService::loadPage(kPage, active));
  verifyMarker(active, 50, 70, 5);
}

void testProjectIsolation() {
  selectEmptyProject("project-a");
  Scene a{};
  setMarker(a, 61, 81, 7);
  assert(PatternPagingService::savePage(kPage, a));

  selectEmptyProject("project-b");
  assert(!PatternPagingService::pageExists(kPage));
  Scene b{};
  setMarker(b, 73, 99, 11);
  assert(PatternPagingService::savePage(kPage, b));

  assert(PatternPagingService::setProjectName("project-a"));
  Scene loadedA{};
  assert(PatternPagingService::loadPage(kPage, loadedA));
  verifyMarker(loadedA, 61, 81, 7);

  assert(PatternPagingService::setProjectName("project-b"));
  Scene loadedB{};
  assert(PatternPagingService::loadPage(kPage, loadedB));
  verifyMarker(loadedB, 73, 99, 11);
}

void testProjectNameEncodingDoesNotCollide(const std::filesystem::path& root) {
  selectEmptyProject("space name");
  Scene spaced{};
  setMarker(spaced, 58, 78, 3);
  assert(PatternPagingService::savePage(kPage, spaced));

  selectEmptyProject("space_20name");
  assert(!PatternPagingService::pageExists(kPage));
  Scene escapedLiteral{};
  setMarker(escapedLiteral, 69, 90, 13);
  assert(PatternPagingService::savePage(kPage, escapedLiteral));

  const std::filesystem::path spacedPath =
      pageFile(root, "space_20name");
  const std::filesystem::path literalPath =
      pageFile(root, "space_5F20name");
  assert(spacedPath != literalPath);
  assert(std::filesystem::exists(spacedPath));
  assert(std::filesystem::exists(literalPath));

  assert(PatternPagingService::setProjectName("space name"));
  Scene loadedSpaced{};
  assert(PatternPagingService::loadPage(kPage, loadedSpaced));
  verifyMarker(loadedSpaced, 58, 78, 3);

  assert(PatternPagingService::setProjectName("space_20name"));
  Scene loadedLiteral{};
  assert(PatternPagingService::loadPage(kPage, loadedLiteral));
  verifyMarker(loadedLiteral, 69, 90, 13);
}

void testProjectCopy() {
  selectEmptyProject("copy-source");
  Scene source{};
  setMarker(source, 68, 88, 10);
  assert(PatternPagingService::savePage(kPage, source));

  assert(PatternPagingService::copyProjectPages("copy-source", "copy-target"));
  assert(PatternPagingService::setProjectName("copy-target"));
  Scene copied{};
  assert(PatternPagingService::loadPage(kPage, copied));
  verifyMarker(copied, 68, 88, 10);
}

void testProjectClear(const std::filesystem::path& root) {
  selectEmptyProject("clear-target");
  Scene first{};
  setMarker(first, 62, 82, 6);
  assert(PatternPagingService::savePage(kPage, first));
  Scene second{};
  setMarker(second, 63, 83, 8);
  assert(PatternPagingService::savePage(kPage, second));

  const std::filesystem::path main = pageFile(root, "clear-target");
  std::ofstream(main.string() + ".tmp", std::ios::binary).put('x');
  assert(std::filesystem::exists(main));
  assert(std::filesystem::exists(main.string() + ".bak"));
  assert(std::filesystem::exists(main.string() + ".tmp"));

  assert(PatternPagingService::clearProjectPages());
  assert(!std::filesystem::exists(main));
  assert(!std::filesystem::exists(main.string() + ".bak"));
  assert(!std::filesystem::exists(main.string() + ".tmp"));
  assert(!PatternPagingService::pageExists(kPage));
}

void testLegacyMigration(const std::filesystem::path& root) {
  selectEmptyProject("legacy-source");
  Scene source{};
  setMarker(source, 66, 86, 4);
  assert(PatternPagingService::savePage(kPage, source));

  const std::filesystem::path legacy = legacyPageFile(root);
  std::filesystem::create_directories(legacy.parent_path());
  std::filesystem::rename(pageFile(root, "legacy-source"), legacy);
  std::filesystem::remove_all(root / "patterns" / "legacy-source");

  assert(PatternPagingService::setProjectName("legacy-target"));
  assert(!std::filesystem::exists(legacy));
  assert(std::filesystem::exists(pageFile(root, "legacy-target")));

  Scene migrated{};
  assert(PatternPagingService::loadPage(kPage, migrated));
  verifyMarker(migrated, 66, 86, 4);
}

void testMissingPageLeavesSceneUntouched() {
  selectEmptyProject("missing");
  Scene active{};
  setMarker(active, 45, 66, 6);
  assert(!PatternPagingService::loadPage(kPage, active));
  verifyMarker(active, 45, 66, 6);
}

}  // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "grooveputer-pattern-paging-test";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  SD.setRoot(root);

  testRoundTrip(root);
  testCorruptOnlyCopyLeavesSceneUntouched(root);
  testBackupRecovery(root);
  testProjectIsolation();
  testProjectNameEncodingDoesNotCollide(root);
  testProjectCopy();
  testProjectClear(root);
  testLegacyMigration(root);
  testMissingPageLeavesSceneUntouched();

  std::filesystem::remove_all(root, ec);
  return 0;
}
