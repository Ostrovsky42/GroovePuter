#include <cassert>
#include <cstdio>
#include <filesystem>
#include "src/audio/pattern_paging.h"
#include "src/state/melody_promotion.h"

SerialMock Serial;
SDMock SD;

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
Buffer melody(uint8_t note) {
  Buffer out{};
  out.lengthTicks = 8 * PhraseRuntime::kTicksPerBar;
  out.count = 8;
  for (uint16_t i = 0; i < out.count; ++i) {
    auto& e = out.events[i];
    e.startTick = i * PhraseRuntime::kTicksPerBar + 24;
    e.durationSubticks = 48 * PhraseRuntime::kSubticksPerTick;
    e.note = note + i;
    e.velocity = 95 + i;
    e.probability = 100;
  }
  return out;
}
int main() {
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() / "gp-project-melody-copy";
  fs::remove_all(root);
  fs::create_directories(root);
  SD.setRoot(root);
  auto& io = MelodyPromotion::defaultFileSystem();
  const char* source = "melody source";
  const char* target = "melody target";
  assert(PatternPagingService::setProjectName(source));
  Scene scene{};
  assert(PatternPagingService::savePage(0, scene));
  const GroovePuterMaterial::MaterialAddress addresses[] = {{0, 0}, {1, 255}};
  for (const auto& address : addresses) {
    uint32_t gen = 0;
    assert(MelodyPromotion::commitMelodyCandidate(io, source, address, melody(60), gen) == MelodyPromotion::Error::None);
    // Both generations must travel, including the slot at the very last page.
    assert(MelodyPromotion::commitMelodyCandidate(io, source, address, melody(65), gen) == MelodyPromotion::Error::None);
    // A destination with a higher old generation must not beat the copied data.
    for (int i = 0; i < 4; ++i)
      assert(MelodyPromotion::commitMelodyCandidate(io, target, address, melody(80), gen) == MelodyPromotion::Error::None);
  }
  assert(PatternPagingService::copyProjectPages(source, target));
  int failures = 0;
  for (const auto& address : addresses) {
    Buffer restored{};
    if (!MelodyPromotion::loadMaterial(io, target, address, restored) ||
        !MelodyPromotion::sameMelody(restored, melody(65))) {
      std::fprintf(stderr, "COPY FAIL: 8-bar Melody v%u g%u missing or stale after Save As\n", address.voice, address.globalSlot);
      ++failures;
    }
    Buffer original{};
    assert(MelodyPromotion::loadMaterial(io, source, address, original));
    assert(MelodyPromotion::sameMelody(original, melody(65)));
  }
  assert(PatternPagingService::setProjectName(target));
  assert(PatternPagingService::clearProjectPages());
  for (const auto& address : addresses) {
    Buffer restored{};
    if (MelodyPromotion::loadMaterial(io, target, address, restored)) {
      std::fprintf(stderr, "CLEAR FAIL: stale Melody survives project clear\n");
      ++failures;
    }
  }
  uint32_t gen = 0;
  assert(MelodyPromotion::commitMelodyCandidate(io, target, addresses[0], melody(70), gen) == MelodyPromotion::Error::None);
  assert(PatternPagingService::setProjectName(source));
  assert(PatternPagingService::clearProjectPages(target));
  Buffer cleared{};
  if (MelodyPromotion::loadMaterial(io, target, addresses[0], cleared)) {
    std::fprintf(stderr, "NAMED CLEAR FAIL: stale Melody survives cleanup\n"); ++failures;
  }
  assert(MelodyPromotion::loadMaterial(io, source, addresses[0], cleared));
  assert(MelodyPromotion::sameMelody(cleared, melody(65)));
  fs::remove_all(root);
  std::fprintf(stderr, "Project Melody copy/clear: %d failure(s)\n", failures);
  return failures ? 1 : 0;
}
