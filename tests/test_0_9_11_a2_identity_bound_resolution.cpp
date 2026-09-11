#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../scenes.h"
#include "../src/state/material_identity.h"
#include "../src/state/melody_promotion.h"

#if __has_include("../src/state/material_resolution.h")
#include "../src/state/material_resolution.h"
#define GROOVEPUTER_HAS_IDENTITY_BOUND_RESOLUTION 1
#else
#define GROOVEPUTER_HAS_IDENTITY_BOUND_RESOLUTION 0
#endif

namespace {

using GroovePuterMaterial::MaterialAddress;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct FakeFs : MelodyPromotion::FileSystem {
  std::map<std::string, std::vector<uint8_t>> files;
  bool present = true;

  bool available() const override { return present; }
  bool exists(const char* path) const override {
    return files.find(path) != files.end();
  }
  bool write(const char* path, const uint8_t* data, size_t length) override {
    if (!present) return false;
    files[path].assign(data, data + length);
    return true;
  }
  bool read(const char* path, std::vector<uint8_t>& out) const override {
    if (!present) return false;
    const auto it = files.find(path);
    if (it == files.end()) return false;
    out = it->second;
    return true;
  }
  bool rename(const char* from, const char* to) override {
    if (!present) return false;
    const auto it = files.find(from);
    if (it == files.end()) return false;
    files[to] = it->second;
    files.erase(it);
    return true;
  }
  bool remove(const char* path) override { return files.erase(path) > 0; }
};

constexpr MaterialAddress kAddress{0, 5};
constexpr int kPage = 0;
const std::string kProject = "a2-identity-bound-resolution";

int residentSlot(MaterialAddress address) {
  const int globalSlot = static_cast<int>(address.globalSlot);
  return (songPatternBank(globalSlot) * Bank<SynthPattern>::kPatterns) +
         songPatternIndexInBank(globalSlot);
}

Buffer makeCandidate(uint8_t note) {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0] = {0, 24 * 16, note, 100, 100, 0, 0, 0};
  return melody;
}

void test_durable_slot_collision() {
  constexpr MaterialAddress addr5{0, 5};
  constexpr MaterialAddress addr21{0, 21};

  const int slot5 = residentSlot(addr5);
  const int slot21 = residentSlot(addr21);

  // Invariant setup: both global 5 and global 21 project to resident slot 5 across different pages.
  if (slot5 != 5 || slot21 != 5) {
    std::fprintf(stderr, "A2_SETUP_FAIL: expected resident slot 5 for both global 5 and global 21\n");
    return;
  }
  if (songPatternPage(addr5.globalSlot) != 0 || songPatternPage(addr21.globalSlot) != 1) {
    std::fprintf(stderr, "A2_SETUP_FAIL: expected global 5 on page 0 and global 21 on page 1\n");
    return;
  }

  // Under current production MelodyPromotion, slotPath/finalPath uses resident slot index: /melody/v%d_s%02d.gpml
  const std::string path5 = MelodyPromotion::finalPath(kProject, addr5.voice, slot5);
  const std::string path21 = MelodyPromotion::finalPath(kProject, addr21.voice, slot21);

  // Prove that production currently collides on durable v0_s05.gpml
  if (path5 == path21 && path5.find("v0_s05.gpml") != std::string::npos) {
    std::fprintf(
        stderr,
        "A2_DURABLE_COLLISION_RED: global 5 (page 0) and global 21 (page 1) both map to resident slot 5 and collide on durable path '%s'\n",
        path5.c_str());
  }

  // Prove actual data loss: promoting global 21 on page 1 overwrites global 5 on page 0
  FakeFs fs;
  Scene scenePage0{};
  Scene scenePage1{};
  const Buffer melody5 = makeCandidate(60);
  const Buffer melody21 = makeCandidate(72);

  const auto err5 = MelodyPromotion::promoteResident(
      fs, kProject, scenePage0, addr5.voice, slot5, melody5);
  if (err5 != MelodyPromotion::Error::None) {
    std::fprintf(stderr, "A2_SETUP_FAIL: promote global 5 failed\n");
    return;
  }

  const auto err21 = MelodyPromotion::promoteResident(
      fs, kProject, scenePage1, addr21.voice, slot21, melody21);
  if (err21 != MelodyPromotion::Error::None) {
    std::fprintf(stderr, "A2_SETUP_FAIL: promote global 21 failed\n");
    return;
  }

  Buffer loaded5{};
  if (!MelodyPromotion::loadResident(
          fs, kProject, addr5.voice, slot5, loaded5)) {
    std::fprintf(stderr, "A2_SETUP_FAIL: loadResident global 5 failed\n");
    return;
  }

  if (loaded5.events[0].note == 72) {
    std::fprintf(
        stderr,
        "A2_DURABLE_COLLISION_RED: promoting global 21 overwrote global 5 payload (note 60 was replaced by note 72 in '%s')\n",
        path5.c_str());
  }
}

#if GROOVEPUTER_HAS_IDENTITY_BOUND_RESOLUTION
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialResolution;
using GroovePuterMaterial::MaterialResolutionStatus;

int gFailures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "A2_IDENTITY_BOUND_RESOLUTION_FAIL: %s\n", message);
  ++gFailures;
}
#endif

}  // namespace

int main() {
  test_durable_slot_collision();

#if !GROOVEPUTER_HAS_IDENTITY_BOUND_RESOLUTION
  std::fprintf(
      stderr,
      "A2_IDENTITY_BOUND_RESOLUTION_RED: MaterialReference-bound resolver is missing; address-only resolution cannot distinguish stale M@A from current N@A\n");
  return 1;
#else
  FakeFs fs;
  Scene scene{};
  Buffer out{};

  const int slot = residentSlot(kAddress);
  const MaterialId idM{101};
  const MaterialId idN{102};
  const MaterialReference refM{kAddress, idM};
  const MaterialReference refN{kAddress, idN};

  // First prove M is a valid live reference and capture the exact-state token.
  scene.materialSlots[kAddress.voice][slot].id = idM;
  const MaterialResolution liveM = GroovePuterMaterial::resolveMaterial(
      fs, kProject, scene, kPage, refM, out);
  expect(liveM.status == MaterialResolutionStatus::ResolvedPattern,
         "live M@A did not resolve as Pattern");
  expect(liveM.isResolved(), "live M@A reported unresolved");
  expect(liveM.hasVersion(), "live M@A exposed no exact version");

  // Replace only identity. Pattern bytes are deliberately unchanged, so M and
  // N have the same content/version. Version equality must not rescue refM.
  scene.materialSlots[kAddress.voice][slot].id = idN;
  const MaterialResolution stale = GroovePuterMaterial::resolveMaterial(
      fs, kProject, scene, kPage, refM, out);
  expect(stale.status == MaterialResolutionStatus::IdentityMismatch,
         "stale {A,idM} did not fail with IdentityMismatch after N reused A");
  expect(!stale.isResolved(), "stale {A,idM} reported resolved");
  expect(!stale.hasVersion(), "stale {A,idM} leaked a version token");

  const MaterialResolution liveN = GroovePuterMaterial::resolveMaterial(
      fs, kProject, scene, kPage, refN, out);
  expect(liveN.status == MaterialResolutionStatus::ResolvedPattern,
         "live N@A did not resolve as Pattern");
  expect(liveN.isResolved(), "live N@A reported unresolved");
  expect(liveN.hasVersion(), "live N@A exposed no exact version");
  expect(liveM.version == liveN.version,
         "test witness accidentally changed content/version while replacing identity");

  const MaterialReference invalid{kAddress, MaterialId{}};
  const MaterialResolution invalidResult = GroovePuterMaterial::resolveMaterial(
      fs, kProject, scene, kPage, invalid, out);
  expect(invalidResult.status == MaterialResolutionStatus::InvalidReference,
         "id=0 reference did not fail closed as InvalidReference");
  expect(!invalidResult.isResolved(), "id=0 reference reported resolved");
  expect(!invalidResult.hasVersion(), "id=0 reference exposed a version token");

  if (gFailures == 0) {
    std::puts("0.9.11 A2 identity-bound material resolution: PASS");
    return 0;
  }

  std::fprintf(stderr,
               "0.9.11 A2 identity-bound material resolution: %d failure(s)\n",
               gFailures);
  return 1;
#endif
}
