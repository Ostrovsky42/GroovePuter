#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../scenes.h"
#include "../src/state/material_identity.h"

#if __has_include("../src/state/material_resolution.h")
#include "../src/state/material_resolution.h"
#define GROOVEPUTER_HAS_IDENTITY_BOUND_RESOLUTION 1
#else
#define GROOVEPUTER_HAS_IDENTITY_BOUND_RESOLUTION 0
#endif

namespace {

#if GROOVEPUTER_HAS_IDENTITY_BOUND_RESOLUTION
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
  const int globalSlot = static_cast<int>(address.globalSlot);
  return (songPatternBank(globalSlot) * Bank<SynthPattern>::kPatterns) +
         songPatternIndexInBank(globalSlot);
}
#endif

}  // namespace

int main() {
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
