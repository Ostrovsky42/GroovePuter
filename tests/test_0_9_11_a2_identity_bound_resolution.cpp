#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../scenes.h"
#include "../src/state/material_identity.h"
#include "../src/state/material_resolution.h"
#include "../src/state/melody_promotion.h"

namespace {

using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialResolution;
using GroovePuterMaterial::MaterialResolutionStatus;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int gFailures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "A2_IDENTITY_BOUND_RESOLUTION_FAIL: %s\n", message);
  ++gFailures;
}

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
  return GroovePuterMaterial::residentSlotFor(address);
}

Buffer makeCandidate(uint8_t note) {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0] = {0, 24 * 16, note, 100, 100, 0, 0, 0};
  return melody;
}

void test_durable_global_address_isolation() {
  const int failuresBefore = gFailures;
  constexpr MaterialAddress addr5{0, 5};
  constexpr MaterialAddress addr21{0, 21};
  constexpr MaterialId id5{105};
  constexpr MaterialId id21{121};
  constexpr MaterialReference ref5{addr5, id5};
  constexpr MaterialReference ref21{addr21, id21};

  const int slot5 = residentSlot(addr5);
  const int slot21 = residentSlot(addr21);

  expect(slot5 == 5 && slot21 == 5,
         "global 5 and global 21 did not project to the same resident slot 5 witness");
  expect(songPatternPage(addr5.globalSlot) == 0 &&
             songPatternPage(addr21.globalSlot) == 1,
         "global 5/page 0 and global 21/page 1 witness changed");

  const std::string path5 = MelodyPromotion::finalPath(kProject, addr5);
  const std::string path21 = MelodyPromotion::finalPath(kProject, addr21);
  expect(path5 != path21,
         "global 5 and global 21 still collide on one durable path");
  expect(path5.find("v0_g005.gpml") != std::string::npos,
         "global 5 durable path is not keyed by global slot 5");
  expect(path21.find("v0_g021.gpml") != std::string::npos,
         "global 21 durable path is not keyed by global slot 21");

  FakeFs fs;
  Scene scenePage0{};
  Scene scenePage1{};
  scenePage0.materialSlots[addr5.voice][slot5].id = id5;
  scenePage1.materialSlots[addr21.voice][slot21].id = id21;
  const Buffer melody5 = makeCandidate(60);
  const Buffer melody21 = makeCandidate(72);

  const auto err5 = MelodyPromotion::promoteResident(
      fs, kProject, scenePage0, ref5, slot5, melody5);
  const auto err21 = MelodyPromotion::promoteResident(
      fs, kProject, scenePage1, ref21, slot21, melody21);
  expect(err5 == MelodyPromotion::Error::None,
         "promotion of global 5 failed");
  expect(err21 == MelodyPromotion::Error::None,
         "promotion of global 21 through resident slot 5 failed");

  Buffer loaded5{};
  Buffer loaded21{};
  const bool loaded5Ok = MelodyPromotion::loadMaterial(
      fs, kProject, addr5, loaded5);
  const bool loaded21Ok = MelodyPromotion::loadMaterial(
      fs, kProject, addr21, loaded21);
  expect(loaded5Ok, "global 5 durable material could not be loaded");
  expect(loaded21Ok, "global 21 durable material could not be loaded");
  if (loaded5Ok) {
    expect(loaded5.count == 1 && loaded5.events[0].note == 60,
           "global 21 overwrote global 5 payload");
  }
  if (loaded21Ok) {
    expect(loaded21.count == 1 && loaded21.events[0].note == 72,
           "global 21 payload was not stored independently");
  }

  if (gFailures == failuresBefore) {
    std::printf(
        "A2_DURABLE_GLOBAL_ADDRESS_GREEN: global 5 -> %s; global 21 -> %s; resident slot 5 reused without durable collision\n",
        path5.c_str(), path21.c_str());
  }
}

void test_identity_bound_resolution() {
  const int failuresBefore = gFailures;
  FakeFs fs;
  Scene scene{};
  Buffer out{};

  const int slot = residentSlot(kAddress);
  const MaterialId idM{101};
  const MaterialId idN{102};
  const MaterialReference refM{kAddress, idM};
  const MaterialReference refN{kAddress, idN};

  scene.materialSlots[kAddress.voice][slot].id = idM;
  const MaterialResolution liveM = GroovePuterMaterial::resolveMaterial(
      fs, kProject, scene, kPage, refM, out);
  expect(liveM.status == MaterialResolutionStatus::ResolvedPattern,
         "live M@A did not resolve as Pattern");
  expect(liveM.isResolved(), "live M@A reported unresolved");
  expect(liveM.hasVersion(), "live M@A exposed no exact version");

  // Replace only identity. Pattern bytes are deliberately unchanged, so M and
  // N have the same exact-state token. Version equality must not rescue refM.
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
         "identity replacement accidentally changed exact-state token");

  const MaterialReference invalid{kAddress, MaterialId{}};
  const MaterialResolution invalidResult = GroovePuterMaterial::resolveMaterial(
      fs, kProject, scene, kPage, invalid, out);
  expect(invalidResult.status == MaterialResolutionStatus::InvalidReference,
         "id=0 reference did not fail closed as InvalidReference");
  expect(!invalidResult.isResolved(), "id=0 reference reported resolved");
  expect(!invalidResult.hasVersion(), "id=0 reference exposed a version token");

  if (gFailures == failuresBefore) {
    std::puts(
        "A2_IDENTITY_BOUND_RESOLUTION_GREEN: stale MaterialReference rejected before version; equal exact-state token does not substitute for identity");
  }
}

}  // namespace

int main() {
  test_durable_global_address_isolation();
  test_identity_bound_resolution();

  if (gFailures == 0) {
    std::puts("0.9.11 A2 identity-bound material resolution: PASS");
    return 0;
  }

  std::fprintf(stderr,
               "0.9.11 A2 identity-bound material resolution: %d failure(s)\n",
               gFailures);
  return 1;
}
