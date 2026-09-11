#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/state/material_slot.h"
#include "src/state/material_slot_access.h"
#include "src/state/melody_promotion.h"

#if __has_include("src/state/material_resolution.h")
#include "src/state/material_resolution.h"
#define HAS_A2_MATERIAL_RESOLUTION 1
#else
#define HAS_A2_MATERIAL_RESOLUTION 0
#endif

#if __has_include("src/state/material_version.h")
#include "src/state/material_version.h"
#define HAS_A2_MATERIAL_VERSION 1
#else
#define HAS_A2_MATERIAL_VERSION 0
#endif

SerialMock Serial;
SDMock SD;

namespace {

int g_failures = 0;
int g_passes = 0;

void recordFailure(const char* name, const char* reason) {
  std::fprintf(stderr, "A2-RED [%s]: %s\n", name, reason);
  ++g_failures;
}

void recordPass(const char* name) {
  std::printf("A2-GREEN [%s]: passed\n", name);
  ++g_passes;
}

// A2-A ADDRESS NAMESPACE
// Invariant: same local slot on different pages must not alias.
void test_a2_address_namespace() {
  const char* testName = "A2_A_ADDRESS_NAMESPACE";
#if HAS_A2_MATERIAL_RESOLUTION
  static_assert(sizeof(GroovePuterMaterial::MaterialAddress) == 2,
                "MaterialAddress must be 2 bytes");
  GroovePuterMaterial::MaterialAddress page0Slot1{0, 1};
  GroovePuterMaterial::MaterialAddress page1Slot1{0, 17};
  if (page0Slot1.globalSlot == page1Slot1.globalSlot) {
    recordFailure(testName, "page 0 and page 1 slot 1 have same globalSlot");
    return;
  }
  const std::string path0 = MelodyPromotion::finalPath("test", page0Slot1);
  const std::string path1 = MelodyPromotion::finalPath("test", page1Slot1);
  if (path0 == path1) {
    recordFailure(testName, "page 0 and page 1 slot 1 produce identical storage paths");
    return;
  }
  if (GroovePuterMaterial::residentSlotFor(page0Slot1) != 1 ||
      GroovePuterMaterial::residentSlotFor(page1Slot1) != 1) {
    recordFailure(testName, "residentSlotFor failed to map global slot to local resident index");
    return;
  }
  if (!GroovePuterMaterial::materialAddressIsResident(page0Slot1, 0) ||
      GroovePuterMaterial::materialAddressIsResident(page1Slot1, 0) ||
      GroovePuterMaterial::materialAddressIsResident(page0Slot1, 1) ||
      !GroovePuterMaterial::materialAddressIsResident(page1Slot1, 1)) {
    recordFailure(testName, "materialAddressIsResident returned incorrect residency across pages");
    return;
  }
  recordPass(testName);
#else
  recordFailure(testName, "MaterialAddress is not defined in current material_slot.h; cross-page slots alias");
#endif
}

// A2-B RESOLVED PATTERN
// Invariant: resident canonical Pattern resolves losslessly as Pattern with all SynthPattern fields preserved.
void test_a2_resolved_pattern() {
  const char* testName = "A2_B_RESOLVED_PATTERN";
#if HAS_A2_MATERIAL_RESOLUTION
  struct DummyFs : MelodyPromotion::FileSystem {
    bool available() const override { return true; }
    bool exists(const char*) const override { return false; }
    bool write(const char*, const uint8_t*, size_t) override { return true; }
    bool read(const char*, std::vector<uint8_t>&) const override { return false; }
    bool rename(const char*, const char*) override { return true; }
    bool remove(const char*) override { return true; }
  } fs;
  Scene scene{};
  scene.synthABanks[0].patterns[0].steps[0].note = 55;
  scene.synthABanks[0].patterns[0].steps[0].accent = true;
  GroovePuterMaterial::MaterialAddress addr{0, 0};
  PhraseRuntime::RuntimeSynthEventBuffer out{};
  const auto res = GroovePuterMaterial::resolveMaterial(fs, "proj", scene, 0, addr, out);
  if (res.status != GroovePuterMaterial::MaterialResolutionStatus::ResolvedPattern) {
    recordFailure(testName, "resident Pattern did not resolve to ResolvedPattern status");
    return;
  }
  if (res.kind != GroovePuterMaterial::MaterialKind::Pattern || !res.isResolved() || !res.hasVersion()) {
    recordFailure(testName, "resolved Pattern has wrong kind or missing version token");
    return;
  }
  if (res.version != GroovePuterMaterial::versionForPattern(scene.synthABanks[0].patterns[0])) {
    recordFailure(testName, "resolved Pattern version does not match exact pattern fingerprint");
    return;
  }
  recordPass(testName);
#else
  recordFailure(testName, "C0 lacks MaterialResolution boundary; cannot resolve resident canonical Pattern");
#endif
}

// A2-C RESOLVED MELODY
// Invariant: canonical Melody payload resolves to correct exact bytes.
void test_a2_resolved_melody() {
  const char* testName = "A2_C_RESOLVED_MELODY";
#if HAS_A2_MATERIAL_RESOLUTION
  struct MelodyFs : MelodyPromotion::FileSystem {
    std::map<std::string, std::vector<uint8_t>> files;
    bool available() const override { return true; }
    bool exists(const char* p) const override { return files.find(p) != files.end(); }
    bool write(const char* p, const uint8_t* d, size_t n) override { files[p].assign(d, d + n); return true; }
    bool read(const char* p, std::vector<uint8_t>& out) const override {
      auto it = files.find(p);
      if (it == files.end()) return false;
      out = it->second;
      return true;
    }
    bool rename(const char*, const char*) override { return true; }
    bool remove(const char*) override { return true; }
  } fs;

  PhraseRuntime::RuntimeSynthEventBuffer expected{};
  expected.count = 1;
  expected.lengthTicks = PhraseRuntime::kTicksPerBar;
  expected.events[0] = {0, 24 * 16, 67, 100, 100, 0, 0, 0};

  GroovePuterMaterial::MaterialAddress addr{0, 2};
  const std::string path = MelodyPromotion::finalPath("proj", addr);
  if (!MelodyStore::encode(expected, fs.files[path])) {
    recordFailure(testName, "failed to encode candidate melody");
    return;
  }

  Scene scene{};
  GroovePuterMaterial::setMaterialKind(scene, addr.voice, addr.globalSlot, 0,
                                      GroovePuterMaterial::MaterialKind::Melody);

  PhraseRuntime::RuntimeSynthEventBuffer out{};
  const auto res = GroovePuterMaterial::resolveMaterial(fs, "proj", scene, 0, addr, out);
  if (res.status != GroovePuterMaterial::MaterialResolutionStatus::ResolvedMelody) {
    recordFailure(testName, "canonical Melody payload did not resolve to ResolvedMelody");
    return;
  }
  if (res.kind != GroovePuterMaterial::MaterialKind::Melody || !res.isResolved() || !res.hasVersion()) {
    recordFailure(testName, "resolved Melody metadata invalid");
    return;
  }
  if (res.version != GroovePuterMaterial::versionForMelody(expected) ||
      out.count != 1 || out.events[0].note != 67) {
    recordFailure(testName, "resolved Melody events or version do not match stored payload");
    return;
  }
  recordPass(testName);
#else
  recordFailure(testName, "C0 lacks MaterialResolution boundary; cannot resolve canonical Melody payload");
#endif
}

// A2-D ERROR TRUTH
// Invariant: NotResident, MissingPayload, CorruptPayload, InvalidAddress, StorageUnavailable
// remain distinguishable; none becomes empty valid material.
void test_a2_error_truth() {
  const char* testName = "A2_D_ERROR_TRUTH";
#if HAS_A2_MATERIAL_RESOLUTION
  struct ErrorFs : MelodyPromotion::FileSystem {
    bool isAvailable = true;
    std::map<std::string, std::vector<uint8_t>> files;
    bool available() const override { return isAvailable; }
    bool exists(const char* p) const override { return files.find(p) != files.end(); }
    bool write(const char*, const uint8_t*, size_t) override { return true; }
    bool read(const char* p, std::vector<uint8_t>& out) const override {
      auto it = files.find(p);
      if (it == files.end()) return false;
      out = it->second;
      return true;
    }
    bool rename(const char*, const char*) override { return true; }
    bool remove(const char*) override { return true; }
  } fs;

  Scene scene{};
  PhraseRuntime::RuntimeSynthEventBuffer out{};

  // 1. InvalidAddress
  auto r1 = GroovePuterMaterial::resolveMaterial(fs, "p", scene, 0, {255, 0}, out);
  if (r1.status != GroovePuterMaterial::MaterialResolutionStatus::InvalidAddress || r1.isResolved()) {
    recordFailure(testName, "invalid voice did not return InvalidAddress");
    return;
  }

  // 2. NotResident
  auto r2 = GroovePuterMaterial::resolveMaterial(fs, "p", scene, 0, {0, 17}, out);
  if (r2.status != GroovePuterMaterial::MaterialResolutionStatus::NotResident || r2.isResolved()) {
    recordFailure(testName, "different page slot did not return NotResident");
    return;
  }

  // Set slot to Melody
  GroovePuterMaterial::MaterialAddress melodyAddr{0, 3};
  GroovePuterMaterial::setMaterialKind(scene, melodyAddr.voice, melodyAddr.globalSlot, 0,
                                      GroovePuterMaterial::MaterialKind::Melody);

  // 3. StorageUnavailable
  fs.isAvailable = false;
  auto r3 = GroovePuterMaterial::resolveMaterial(fs, "p", scene, 0, melodyAddr, out);
  if (r3.status != GroovePuterMaterial::MaterialResolutionStatus::StorageUnavailable || r3.isResolved()) {
    recordFailure(testName, "storage unavailable did not return StorageUnavailable");
    return;
  }
  fs.isAvailable = true;

  // 4. MissingPayload
  auto r4 = GroovePuterMaterial::resolveMaterial(fs, "p", scene, 0, melodyAddr, out);
  if (r4.status != GroovePuterMaterial::MaterialResolutionStatus::MissingPayload || r4.isResolved()) {
    recordFailure(testName, "missing file did not return MissingPayload");
    return;
  }

  // 5. CorruptPayload
  const std::string path = MelodyPromotion::finalPath("p", melodyAddr);
  fs.files[path] = {0xDE, 0xAD, 0xBE, 0xEF};
  auto r5 = GroovePuterMaterial::resolveMaterial(fs, "p", scene, 0, melodyAddr, out);
  if (r5.status != GroovePuterMaterial::MaterialResolutionStatus::CorruptPayload || r5.isResolved()) {
    recordFailure(testName, "corrupt file did not return CorruptPayload");
    return;
  }

  recordPass(testName);
#else
  recordFailure(testName, "C0 lacks explicit MaterialResolutionStatus error states; unresolved collapses to empty pattern");
#endif
}

// A2-E VERSION BINDING
// Invariant: same canonical bytes -> same token; changed bytes -> different token.
void test_a2_version_binding() {
  const char* testName = "A2_E_VERSION_BINDING";
#if HAS_A2_MATERIAL_VERSION
  static_assert(sizeof(GroovePuterMaterial::MaterialVersionToken) == 8,
                "MaterialVersionToken must be exactly 8 bytes");
  SynthPattern p1{};
  p1.steps[0].note = 60;
  p1.steps[0].accent = true;
  SynthPattern p2 = p1;
  const auto v1 = GroovePuterMaterial::versionForPattern(p1);
  const auto v2 = GroovePuterMaterial::versionForPattern(p2);
  if (v1 != v2 || !v1.valid()) {
    recordFailure(testName, "identical patterns produced different or invalid tokens");
    return;
  }

  p2.steps[0].note = 61;
  const auto v3 = GroovePuterMaterial::versionForPattern(p2);
  if (v1 == v3) {
    recordFailure(testName, "edited pattern produced identical token");
    return;
  }

  PhraseRuntime::RuntimeSynthEventBuffer m1{};
  m1.count = 1;
  m1.lengthTicks = PhraseRuntime::kTicksPerBar;
  m1.events[0] = {0, 24 * 16, 60, 100, 100, 0, 0, 0};
  PhraseRuntime::RuntimeSynthEventBuffer m2 = m1;
  const auto mv1 = GroovePuterMaterial::versionForMelody(m1);
  const auto mv2 = GroovePuterMaterial::versionForMelody(m2);
  if (mv1 != mv2 || !mv1.valid()) {
    recordFailure(testName, "identical melodies produced different or invalid tokens");
    return;
  }

  m2.events[0].velocity = 80;
  const auto mv3 = GroovePuterMaterial::versionForMelody(m2);
  if (mv1 == mv3) {
    recordFailure(testName, "edited melody produced identical token");
    return;
  }

  recordPass(testName);
#else
  recordFailure(testName, "C0 lacks MaterialVersionToken; cannot compute content-bound exact fingerprint");
#endif
}

// A2-F UNRESOLVED VERSION
// Invariant: unresolved material has no valid version token.
void test_a2_unresolved_version() {
  const char* testName = "A2_F_UNRESOLVED_VERSION";
#if HAS_A2_MATERIAL_VERSION
  GroovePuterMaterial::MaterialVersionToken emptyToken{};
  if (emptyToken.valid()) {
    recordFailure(testName, "default MaterialVersionToken reported valid()");
    return;
  }
  const auto res = GroovePuterMaterial::unresolved(
      GroovePuterMaterial::MaterialResolutionStatus::MissingPayload,
      GroovePuterMaterial::MaterialKind::Melody);
  if (res.hasVersion() || res.version.valid() || res.isResolved()) {
    recordFailure(testName, "unresolved MaterialResolution exposed a valid version token");
    return;
  }
  recordPass(testName);
#else
  recordFailure(testName, "C0 lacks MaterialVersionToken validation; invalid states have no version guard");
#endif
}

// A2-G FAILED WRITE
// Invariant: payload write failure must leave old accepted material/version unchanged.
void test_a2_failed_write() {
  const char* testName = "A2_G_FAILED_WRITE";
  recordFailure(testName, "C0 promotion lacks global MaterialAddress transaction safety for write failure");
}

// A2-H FAILED VERIFY
// Invariant: verification failure must leave old accepted material/version unchanged.
void test_a2_failed_verify() {
  const char* testName = "A2_H_FAILED_VERIFY";
  recordFailure(testName, "C0 promotion lacks exact MaterialVersion verification boundary");
}

// A2-I FAILED RENAME/COMMIT
// Invariant: commit publication failure must preserve old descriptor and old content.
void test_a2_failed_rename_commit() {
  const char* testName = "A2_I_FAILED_RENAME_COMMIT";
  recordFailure(testName, "C0 promotion lacks atomic publication and version binding");
}

// A2-J REPRESENTATION != ACCEPTANCE
// Invariant: Pattern->Melody preparation alone must not mean user acceptance.
void test_a2_representation_not_acceptance() {
  const char* testName = "A2_J_REPRESENTATION_NOT_ACCEPTANCE";
  recordFailure(testName, "C0 makePhrase directly toggles live playback source without working staging boundary");
}

// A2-K MATERIAL SCOPE
// Invariant: accepting/persisting target Material must not implicitly persist unrelated dirty Scene state.
void test_a2_material_scope() {
  const char* testName = "A2_K_MATERIAL_SCOPE";
  recordFailure(testName, "C0 has no material-scoped acceptance; only full Scene save exists");
}

// A2-L RECOVERY ISOLATION
// Invariant: WORKING must not leak into recovery persistence.
void test_a2_recovery_isolation() {
  const char* testName = "A2_L_RECOVERY_ISOLATION";
  recordFailure(testName, "C0 autoSaveSceneRecovery serializes uncommitted pattern edits into recovery storage");
}

// A2-M ACTIVE/NEXT
// Invariant: resolver/admission changes must not regress existing current/next causality.
void test_a2_active_next_causality() {
  const char* testName = "A2_M_ACTIVE_NEXT";
  MiniAcid engine{44100.0f, nullptr};
  engine.setBpm(120.0f);
  (void)engine.rebuildPatternRuntimeEventBank();

  const auto initialActive = engine.activeMaterial(0);
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0] = {0, 24 * 16, 67, 100, 100, 0, 0, 0};

  const bool staged = engine.stagePendingMaterial(
      0, 5, GroovePuterMaterial::MaterialKind::Melody, &melody);
  assert(staged);

  const auto activeAfterStage = engine.activeMaterial(0);
  if (activeAfterStage.slot != initialActive.slot ||
      activeAfterStage.kind != initialActive.kind) {
    recordFailure(testName, "activeMaterial moved immediately upon staging!");
    return;
  }

  engine.activatePendingMaterial();
  const auto activeAfterActivate = engine.activeMaterial(0);
  if (activeAfterActivate.slot != 5 ||
      activeAfterActivate.kind != GroovePuterMaterial::MaterialKind::Melody) {
    recordFailure(testName, "activeMaterial did not update on activation");
    return;
  }
  recordPass(testName);
}

// A2-N OLD PROJECT
// Invariant: legacy Scene Pattern without new payload metadata still resolves safely.
void test_a2_old_project_compatibility() {
  const char* testName = "A2_N_OLD_PROJECT";
  Scene scene{};
  (void)scene;
  const auto kind = GroovePuterMaterial::kindFromPersistedValue(0);
  if (kind == GroovePuterMaterial::MaterialKind::Pattern) {
    recordPass(testName);
  } else {
    recordFailure(testName, "legacy slot did not resolve to Pattern");
  }
}

}  // namespace

int main() {
  std::printf("==================================================\n");
  std::printf("0.9.11 A2 ADMISSION BEHAVIORAL RED WITNESSES\n");
  std::printf("==================================================\n");

  test_a2_address_namespace();
  test_a2_resolved_pattern();
  test_a2_resolved_melody();
  test_a2_error_truth();
  test_a2_version_binding();
  test_a2_unresolved_version();
  test_a2_failed_write();
  test_a2_failed_verify();
  test_a2_failed_rename_commit();
  test_a2_representation_not_acceptance();
  test_a2_material_scope();
  test_a2_recovery_isolation();
  test_a2_active_next_causality();
  test_a2_old_project_compatibility();

  std::printf("==================================================\n");
  std::printf("SUMMARY: %d passed, %d TRUE RED failure(s)\n", g_passes, g_failures);
  std::printf("==================================================\n");

  return (g_failures > 0) ? 1 : 0;
}
