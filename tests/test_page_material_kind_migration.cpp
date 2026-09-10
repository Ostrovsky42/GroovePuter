// M2c: the material kind travels with the page that carries its patterns.
//
// Promotion exists now, so a page switch that moved pattern bytes without their
// kinds would lose the fact that a slot is a Melody -- and lose it during
// ordinary navigation, not during anything that looks dangerous. The runtime
// authority in M3 is going to depend on this, so it has to be closed first.
//
// The migration rule is deliberately asymmetric:
//
//   version 3, no kinds at all   -> every slot Pattern. Proven legacy: those
//                                   files predate promotion, so it is true.
//   version 4, valid kinds       -> restored exactly.
//   version 4, invalid kind      -> the load is REJECTED, not sanitised.
//
// That last one matters. Quietly demoting an unreadable kind to Pattern would
// make Song play the old pattern bytes instead of the melody, and nothing
// downstream could tell it happened. A refused load leaves the page that is
// already in memory alone, which is recoverable.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "scenes.h"
#include "src/audio/pattern_paging.h"
#include "src/state/material_slot_access.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialKind;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "M2c FAIL: %s\n", message);
  ++g_failures;
}

void seedPatterns(Scene& scene) {
  scene.synthABanks[0].patterns[2].steps[3].note = 61;
  scene.synthBBanks[1].patterns[5].steps[9].note = 44;
  scene.drumBanks[0].patterns[1].voices[0].steps[0].hit = 1;
}

}  // namespace

int main() {
  PatternPagingService::setProjectName("m2c_test");

  // 1. A mix of kinds survives a save and load, and the patterns come back
  //    byte-for-byte. The kind is a label; it must not disturb what it labels.
  {
    Scene saved{};
    seedPatterns(saved);
    (void)GroovePuterMaterial::setResidentKind(saved, 0, 3, MaterialKind::Melody);
    (void)GroovePuterMaterial::setResidentKind(saved, 1, 12, MaterialKind::Melody);

    expect(PatternPagingService::savePage(0, saved), "savePage refused");

    Scene loaded{};
    expect(PatternPagingService::loadPage(0, loaded), "loadPage refused");

    expect(GroovePuterMaterial::residentKind(loaded, 0, 3) ==
               MaterialKind::Melody,
           "a promoted slot came back as Pattern");
    expect(GroovePuterMaterial::residentKind(loaded, 1, 12) ==
               MaterialKind::Melody,
           "the second promoted slot was lost");
    expect(GroovePuterMaterial::residentKind(loaded, 0, 4) ==
               MaterialKind::Pattern,
           "an untouched slot came back promoted");

    expect(std::memcmp(saved.synthABanks, loaded.synthABanks,
                       sizeof(saved.synthABanks)) == 0,
           "synth A pattern bytes changed across the round trip");
    expect(std::memcmp(saved.synthBBanks, loaded.synthBBanks,
                       sizeof(saved.synthBBanks)) == 0,
           "synth B pattern bytes changed across the round trip");
    expect(std::memcmp(saved.drumBanks, loaded.drumBanks,
                       sizeof(saved.drumBanks)) == 0,
           "drum pattern bytes changed across the round trip");
  }

  // 2. Page away and back. This is the ordinary navigation that would silently
  //    have demoted a slot before this change.
  {
    Scene scene{};
    seedPatterns(scene);
    (void)GroovePuterMaterial::setResidentKind(scene, 0, 7, MaterialKind::Melody);
    expect(PatternPagingService::savePage(1, scene), "savePage(1) refused");

    Scene other{};
    expect(PatternPagingService::savePage(2, other), "savePage(2) refused");
    expect(PatternPagingService::loadPage(2, scene), "loadPage(2) refused");
    expect(GroovePuterMaterial::residentKind(scene, 0, 7) ==
               MaterialKind::Pattern,
           "the other page carried the first page's kinds");

    expect(PatternPagingService::loadPage(1, scene), "loadPage(1) refused");
    expect(GroovePuterMaterial::residentKind(scene, 0, 7) ==
               MaterialKind::Melody,
           "paging away and back demoted a promoted slot");
  }

  // 3. A corrupted kind rejects the load, and the scene already in memory is
  //    left untouched rather than half-replaced.
  {
    Scene scene{};
    seedPatterns(scene);
    (void)GroovePuterMaterial::setResidentKind(scene, 0, 1, MaterialKind::Melody);
    expect(PatternPagingService::savePage(3, scene), "savePage(3) refused");

    const std::string path = "/patterns/m2c_5Ftest/page_03.gpp";
    std::vector<uint8_t> bytes;
    {
      File in = SD.open(path.c_str(), FILE_READ);
      expect(static_cast<bool>(in), "the saved page could not be reopened");
      bytes.resize(in.size());
      in.read(bytes.data(), bytes.size());
      in.close();
    }
    // The kinds sit at the very end of the payload; a value of 7 is neither
    // Pattern nor Melody.
    bytes[bytes.size() - 1] = 7;
    {
      SD.remove(path.c_str());
      File out = SD.open(path.c_str(), FILE_WRITE);
      out.write(bytes.data(), bytes.size());
      out.close();
    }

    SD.remove("/patterns/m2c_5Ftest/page_03.gpp.bak");

    Scene live{};
    seedPatterns(live);
    (void)GroovePuterMaterial::setResidentKind(live, 1, 2, MaterialKind::Melody);
    const Scene before = live;

    const bool loaded = PatternPagingService::loadPage(3, live);
    expect(!loaded, "a page with an invalid kind was accepted");
    expect(std::memcmp(&before, &live, sizeof(Scene)) == 0,
           "a rejected page load still modified the scene in memory");
  }

  if (g_failures == 0) {
    std::printf("M2c page material kind migration: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M2c page material kind migration: %d failure(s)\n",
               g_failures);
  return 1;
}
