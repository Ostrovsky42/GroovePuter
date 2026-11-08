// M1: material identity foundation.
//
// Names what a slot holds -- Pattern or Melody -- and nothing else. No storage
// for melodies, no playback change, no key handling. The value of this slice is
// that everything after it can say "this slot is a Melody" without inventing a
// second place to keep that fact.
//
// The accessors matter as much as the field. Consumers must not read the array,
// so that step 3 can replace the representation with the resolved runtime
// authority without rewriting every caller.

#include <cstdio>
#include <cstring>
#include <string>

#include "scenes.h"
#include "src/state/material_slot_access.h"

namespace {

using GroovePuterMaterial::MaterialKind;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "M1 FAIL: %s\n", message);
  ++g_failures;
}

}  // namespace

int main() {
  // Evidence for the record: the field costs 2 voices x 16 resident slots.
  std::printf("M1 Scene = %zu bytes\n", sizeof(Scene));

  // 1. A new scene is entirely Pattern. Melody must never appear by default --
  //    a slot that was never promoted is a Pattern, and saying otherwise would
  //    make the engine look for material that does not exist.
  {
    Scene scene{};
    bool allPattern = true;
    for (int voice = 0; voice < 2; ++voice) {
      for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
        if (GroovePuterMaterial::residentKind(scene, voice, slot) !=
            MaterialKind::Pattern) {
          allPattern = false;
        }
      }
    }
    expect(allPattern, "a new scene did not start entirely on Pattern");
  }

  // 2. Setting a kind changes that slot and only that slot.
  {
    Scene scene{};
    expect(GroovePuterMaterial::setResidentKind(scene, 1, 5,
                                                MaterialKind::Melody),
           "setting a kind on a valid slot was refused");
    expect(GroovePuterMaterial::residentKind(scene, 1, 5) ==
               MaterialKind::Melody,
           "the kind did not take effect");
    expect(GroovePuterMaterial::residentKind(scene, 0, 5) ==
               MaterialKind::Pattern,
           "setting a kind on synth B changed synth A");
    expect(GroovePuterMaterial::residentKind(scene, 1, 4) ==
               MaterialKind::Pattern,
           "setting a kind changed a neighbouring slot");
  }

  // 3. Out-of-range access is refused rather than silently clamped: a wrong
  //    slot answering confidently is worse than an answer that says no.
  {
    Scene scene{};
    expect(!GroovePuterMaterial::setResidentKind(scene, 2, 0,
                                                 MaterialKind::Melody),
           "an out-of-range voice was accepted");
    expect(!GroovePuterMaterial::setResidentKind(scene, 0, 99,
                                                 MaterialKind::Melody),
           "an out-of-range slot was accepted");
    expect(GroovePuterMaterial::residentKind(scene, 9, 9) ==
               MaterialKind::Pattern,
           "an out-of-range read did not fall back to Pattern");
  }

  // 4. Changing a kind must not disturb the pattern bytes. This slice adds a
  //    label; it must not touch the music it labels.
  {
    Scene scene{};
    scene.synthABanks[0].patterns[3].steps[7].note = 61;
    scene.synthBBanks[1].patterns[2].steps[0].note = 44;
    Bank<SynthPattern> aBefore[kBankCount];
    Bank<SynthPattern> bBefore[kBankCount];
    std::memcpy(aBefore, scene.synthABanks, sizeof(aBefore));
    std::memcpy(bBefore, scene.synthBBanks, sizeof(bBefore));

    for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
      (void)GroovePuterMaterial::setResidentKind(scene, 0, slot,
                                                 MaterialKind::Melody);
    }
    expect(std::memcmp(aBefore, scene.synthABanks, sizeof(aBefore)) == 0,
           "changing kinds altered synth A pattern bytes");
    expect(std::memcmp(bBefore, scene.synthBBanks, sizeof(bBefore)) == 0,
           "changing kinds altered synth B pattern bytes");
  }

  // 5. Song references are untouched. Nothing about this slice may change what
  //    an arrangement points at.
  {
    Scene scene{};
    scene.songs[0].positions[0].patterns[0] = 37;
    scene.songs[0].positions[1].patterns[2] = 91;
    Song songBefore[2];
    std::memcpy(songBefore, scene.songs, sizeof(songBefore));

    (void)GroovePuterMaterial::setResidentKind(scene, 0, 3, MaterialKind::Melody);
    expect(std::memcmp(songBefore, scene.songs, sizeof(songBefore)) == 0,
           "changing a kind altered Song references");
  }

  // 6. Persistence: a kind survives a round trip through the scene document.
  {
    Scene scene{};
    (void)GroovePuterMaterial::setResidentKind(scene, 0, 2, MaterialKind::Melody);
    (void)GroovePuterMaterial::setResidentKind(scene, 1, 15, MaterialKind::Melody);

    std::string encoded;
    expect(GroovePuterMaterial::encodeKinds(scene, encoded),
           "kinds could not be encoded");

    Scene restored{};
    expect(GroovePuterMaterial::decodeKinds(encoded.c_str(), restored),
           "encoded kinds could not be decoded");
    expect(GroovePuterMaterial::residentKind(restored, 0, 2) ==
               MaterialKind::Melody,
           "a promoted slot did not survive the round trip");
    expect(GroovePuterMaterial::residentKind(restored, 1, 15) ==
               MaterialKind::Melody,
           "the last slot did not survive the round trip");
    expect(GroovePuterMaterial::residentKind(restored, 0, 0) ==
               MaterialKind::Pattern,
           "an untouched slot came back as something else");
  }

  // 7. A scene written before this field existed carries no kinds at all. It
  //    must decode as entirely Pattern rather than as garbage -- this is the
  //    case that decides whether existing projects survive the change.
  {
    Scene restored{};
    (void)GroovePuterMaterial::setResidentKind(restored, 0, 4,
                                               MaterialKind::Melody);
    expect(GroovePuterMaterial::decodeKinds(nullptr, restored),
           "a scene without the field was rejected instead of defaulted");
    bool allPattern = true;
    for (int voice = 0; voice < 2; ++voice) {
      for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
        if (GroovePuterMaterial::residentKind(restored, voice, slot) !=
            MaterialKind::Pattern) {
          allPattern = false;
        }
      }
    }
    expect(allPattern, "a legacy scene did not decode as entirely Pattern");
  }

  // 8. A malformed value decodes as Pattern rather than as an invalid kind.
  {
    Scene restored{};
    expect(GroovePuterMaterial::decodeKinds("7,1,0", restored),
           "a partial list was rejected outright");
    expect(GroovePuterMaterial::residentKind(restored, 0, 0) ==
               MaterialKind::Pattern,
           "an out-of-range value did not fall back to Pattern");
    expect(GroovePuterMaterial::residentKind(restored, 0, 1) ==
               MaterialKind::Melody,
           "a valid value after a bad one was lost");
  }

  if (g_failures == 0) {
    std::printf("M1 material slot identity: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M1 material slot identity: %d failure(s)\n", g_failures);
  return 1;
}
