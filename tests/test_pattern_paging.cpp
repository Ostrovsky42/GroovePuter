#include "../platform_sdl/arduino_compat.h"
#include "../src/audio/pattern_paging.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>

SerialMock Serial;
SDMock SD;

namespace {

constexpr int kPage = 3;

std::filesystem::path pageFile(const std::filesystem::path& root) {
  return root / "patterns" / "page_03.gpp";
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
  Scene source{};
  setMarker(source, 64, 89, 12);
  assert(PatternPagingService::savePage(kPage, source));
  assert(PatternPagingService::pageExists(kPage));

  Scene target{};
  setMarker(target, 31, 22, -7);
  assert(PatternPagingService::loadPage(kPage, target));
  verifyMarker(target, 64, 89, 12);
  assert(std::filesystem::exists(pageFile(root)));
}

void testCorruptOnlyCopyLeavesSceneUntouched(
    const std::filesystem::path& root) {
  PatternPagingService::removePage(kPage);

  Scene source{};
  setMarker(source, 67, 91, 9);
  assert(PatternPagingService::savePage(kPage, source));
  corruptPayload(pageFile(root));

  Scene active{};
  setMarker(active, 42, 55, -4);
  assert(!PatternPagingService::loadPage(kPage, active));
  verifyMarker(active, 42, 55, -4);
}

void testBackupRecovery(const std::filesystem::path& root) {
  PatternPagingService::removePage(kPage);

  Scene first{};
  setMarker(first, 50, 70, 5);
  assert(PatternPagingService::savePage(kPage, first));

  Scene second{};
  setMarker(second, 72, 101, 14);
  assert(PatternPagingService::savePage(kPage, second));
  assert(std::filesystem::exists(pageFile(root).string() + ".bak"));

  corruptPayload(pageFile(root));

  Scene active{};
  setMarker(active, 20, 30, -3);
  assert(PatternPagingService::loadPage(kPage, active));
  verifyMarker(active, 50, 70, 5);
}

void testMissingPageLeavesSceneUntouched() {
  PatternPagingService::removePage(kPage);
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
  testMissingPageLeavesSceneUntouched();

  std::filesystem::remove_all(root, ec);
  return 0;
}
