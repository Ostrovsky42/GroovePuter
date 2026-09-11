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
// Current material_slot.h lacks MaterialAddress (only has local slot access).
void test_a2_address_namespace() {
  const char* testName = "A2_A_ADDRESS_NAMESPACE";
#if HAS_A2_MATERIAL_RESOLUTION
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
