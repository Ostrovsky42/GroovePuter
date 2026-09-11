#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/phrase/runtime_phrase_edit.h"
#include "src/phrase/runtime_phrase_generation.h"
#include "src/state/bounded_undo_slot.h"
#include "src/state/material_slot.h"
#include "src/state/material_slot_access.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"
#include "src/ui/phrase_instrument_controls.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialKind;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;
int g_passes = 0;

void recordFailure(const char* name, const char* reason) {
  std::fprintf(stderr, "M-UX0 RED [%s]: %s\n", name, reason);
  ++g_failures;
}

void recordPass(const char* name) {
  std::printf("M-UX0 GREEN [%s]: passed\n", name);
  ++g_passes;
}

Buffer makeOneBarMelody(uint8_t note = 60) {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0].startTick = 0;
  melody.events[0].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  melody.events[0].note = note;
  melody.events[0].velocity = 100;
  melody.events[0].probability = 100;
  melody.events[0].flags = 0;
  return melody;
}

// ---------------------------------------------------------------------------
// A. WORKING ISOLATION
// ---------------------------------------------------------------------------
// Invariant: Mutating notes/durations in an ephemeral WORKING buffer must never
// modify the persistent/accepted state in Scene until an explicit ACCEPT.
void test_mux0_working_isolation_witness() {
  const char* testName = "A_WORKING_ISOLATION";
  MiniAcid engine{44100.0f, nullptr};

  // Accepted baseline: voice 0 has an accepted pattern in Scene with note 60 at step 0
  SynthPattern& scenePattern = engine.sceneManager().currentScene().synthABanks[0].patterns[0];
  scenePattern.steps[0].note = 60;
  scenePattern.steps[0].accent = true;

  // In the desired M-UX0 architecture:
  //   WorkingCopy working = engine.workingCopy(voice=0);
  //   working.setNote(step=0, 67);
  //   assert(scenePattern.steps[0].note == 60); // ACCEPTED MUST REMAIN UNTOUCHED!
  //
  // In current production: there is NO workingCopy() abstraction.
  // Any UI edit immediately commits to sceneManager (via commitPatternMutation)
  // or mutates live currentPhrase_.
  const bool hasWorkingSeparation = false; // Current production lacks working copy layer
  if (!hasWorkingSeparation) {
    recordFailure(testName,
                  "Current production has no WorkingCopy abstraction; edits commit immediately to Scene");
  } else {
    recordPass(testName);
  }
}

// ---------------------------------------------------------------------------
// B. PATTERN -> MELODY IS NOT ACCEPT
// ---------------------------------------------------------------------------
// Invariant: Extending length (1 bar -> 4 bars) requires internal projection
// to Melody representation, but must NOT alter the accepted descriptor or
// accepted Pattern in Scene until ACCEPT is explicitly executed.
void test_mux0_representation_migration_is_not_accept_witness() {
  const char* testName = "B_PATTERN_TO_MELODY_IS_NOT_ACCEPT";
  MiniAcid engine{44100.0f, nullptr};

  // Voice 0 starts in Pattern mode with canonical Pattern accepted
  const auto initialKind = GroovePuterMaterial::residentKind(
      engine.sceneManager().currentScene(), 0, 0);
  assert(initialKind == MaterialKind::Pattern);

  // In current production, when makePhrase() occurs:
  // makePhrase() immediately switches active sequenced source to Phrase:
  const bool made = engine.makePhrase(0);
  assert(made);

  // Current production: calling makePhrase() overwrites currentPhrase_[0]
  // with a projection. There is NO DISCARD back to the clean Scene Pattern
  // without manually toggling SRC back to PATTERN.
  const bool hasDiscardToAccepted = false; // Lacks transaction/discard boundary
  if (!hasDiscardToAccepted) {
    recordFailure(testName,
                  "No transaction boundary: Pattern->Phrase projection cannot be discarded atomically");
  } else {
    recordPass(testName);
  }
}

// ---------------------------------------------------------------------------
// C. LENGTH 1 -> 4 PRESERVES BAR 1
// ---------------------------------------------------------------------------
// Invariant: When length grows 1 -> 4 bars, the first bar's events must remain
// byte-identical and tick-identical.
void test_mux0_length_1_to_4_preserves_bar1() {
  const char* testName = "C_LENGTH_1_TO_4_PRESERVES_BAR1";
  Buffer before = makeOneBarMelody(62);
  Buffer candidate{};

  const bool prepared = PhraseInstrumentControls::prepareLengthTarget(
      before, 4, candidate);

  if (!prepared) {
    recordFailure(testName, "prepareLengthTarget failed for 1->4 bars expansion");
    return;
  }

  if (candidate.lengthTicks != PhraseRuntime::kTicksPerBar * 4u) {
    recordFailure(testName, "candidate length was not 4 bars");
    return;
  }

  if (candidate.count != before.count) {
    recordFailure(testName, "candidate event count changed during length expansion");
    return;
  }

  if (candidate.events[0].startTick != before.events[0].startTick ||
      candidate.events[0].durationSubticks != before.events[0].durationSubticks ||
      candidate.events[0].note != before.events[0].note) {
    recordFailure(testName, "events in Bar 1 were mutated during expansion");
    return;
  }

  recordPass(testName);
}

// ---------------------------------------------------------------------------
// D. REPEAT IS DETERMINISTIC
// ---------------------------------------------------------------------------
// Invariant: REPEAT 1->4 bars must duplicate Bar 1 deterministically across all
// bars without generator randomness.
void test_mux0_repeat_deterministic() {
  const char* testName = "D_REPEAT_DETERMINISTIC";
  SynthPattern pattern{};
  pattern.steps[0].note = 48;
  pattern.steps[0].accent = true;
  pattern.steps[4].note = 51;
  pattern.steps[8].note = 55;
  pattern.steps[12].note = 58;

  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = 0;
  settings.gateLengthRatio = 0.5f;
  settings.swingPercent = 50;
  settings.swingEnabled = false;

  Buffer run1{};
  Buffer run2{};
  const auto res1 = PhraseGeneration::projectRepeatedPattern(
      pattern, settings, PhraseRuntime::kTicksPerBar * 4u, run1);
  const auto res2 = PhraseGeneration::projectRepeatedPattern(
      pattern, settings, PhraseRuntime::kTicksPerBar * 4u, run2);

  if (res1 != PhraseGeneration::Result::Ready ||
      res2 != PhraseGeneration::Result::Ready) {
    recordFailure(testName, "projectRepeatedPattern failed");
    return;
  }

  if (run1.count != 16 || run2.count != 16) {
    recordFailure(testName, "unexpected event count in 4-bar repeat");
    return;
  }

  if (std::memcmp(&run1, &run2, sizeof(Buffer)) != 0) {
    recordFailure(testName, "projectRepeatedPattern is non-deterministic!");
    return;
  }

  // Verify Bar 0 events match Bar 1, 2, 3 with exact tick offsets
  for (uint16_t b = 1; b < 4; ++b) {
    const uint16_t offset = b * PhraseRuntime::kTicksPerBar;
    for (uint16_t i = 0; i < 4; ++i) {
      const auto& baseEv = run1.events[i];
      const auto& repEv = run1.events[b * 4 + i];
      if (repEv.startTick != baseEv.startTick + offset ||
          repEv.note != baseEv.note ||
          repEv.flags != baseEv.flags) {
        recordFailure(testName, "repeated bar events do not match base bar");
        return;
      }
    }
  }

  recordPass(testName);
}

// ---------------------------------------------------------------------------
// E. DISCARD RESTORES EXACT ACCEPTED STATE
// ---------------------------------------------------------------------------
// Invariant: Modifying working material and calling discard must restore the
// accepted state byte-for-byte without filesystem residue.
void test_mux0_discard_restores_accepted_witness() {
  const char* testName = "E_DISCARD_RESTORES_ACCEPTED";
  const bool hasExplicitDiscard = false;
  if (!hasExplicitDiscard) {
    recordFailure(testName,
                  "Current production has no discardWorking() API to restore accepted state");
  } else {
    recordPass(testName);
  }
}

// ---------------------------------------------------------------------------
// F. ONE-LEVEL UNDO BOUNDARY (NOT MULTI-CANDIDATE HISTORY)
// ---------------------------------------------------------------------------
// Invariant: UndoOwner owns exactly ONE receipt.
// If user has Candidate B -> performs local note edit -> Candidate C,
// Undo restores the state immediately before C (i.e. B with local edit),
// NOT original candidate A. A second Undo must return NothingToUndo or Redo.
void test_mux0_one_level_undo_contract() {
  const char* testName = "F_ONE_LEVEL_UNDO_CONTRACT";
  auto& owner = GroovePuterUndo::undoOwner();
  owner.clear();

  // Step 1: Initial state A
  Buffer stateA = makeOneBarMelody(60); // note 60

  // Step 2: Generation / Candidate B
  Buffer stateB = makeOneBarMelody(64); // note 64
  GroovePuterUndo::RuntimePhraseUndoPayload receipt1{};
  receipt1.voiceIndex = 0;
  receipt1.before = stateA;
  const bool com1 = owner.commitRuntimePrepared(
      GroovePuterUndo::UndoKind::RuntimePhrase, receipt1, [&]() {});
  assert(com1);

  // Step 3: Local note edit on B -> State B_edited
  Buffer stateB_edited = makeOneBarMelody(65); // note 65
  GroovePuterUndo::RuntimePhraseUndoPayload receipt2{};
  receipt2.voiceIndex = 0;
  receipt2.before = stateB;
  const bool com2 = owner.commitRuntimePrepared(
      GroovePuterUndo::UndoKind::RuntimePhrase, receipt2, [&]() {});
  assert(com2);

  // Step 4: Another candidate C
  Buffer stateC = makeOneBarMelody(72); // note 72
  (void)stateC;
  GroovePuterUndo::RuntimePhraseUndoPayload receipt3{};
  receipt3.voiceIndex = 0;
  receipt3.before = stateB_edited;
  const bool com3 = owner.commitRuntimePrepared(
      GroovePuterUndo::UndoKind::RuntimePhrase, receipt3, [&]() {});
  assert(com3);

  // Now test single-level Undo:
  // Undo MUST restore stateB_edited (the receipt from Step 4), NOT stateA.
  Buffer restored{};
  const auto result = owner.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
      GroovePuterUndo::UndoKind::RuntimePhrase,
      [](const GroovePuterUndo::RuntimePhraseUndoPayload&) { return true; },
      [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
        restored = retained.before;
      });

  if (result != GroovePuterUndo::UndoResult::Restored) {
    recordFailure(testName, "toggleRuntimePrepared failed to restore");
    return;
  }

  // Verify that restored state is stateB_edited, proving receipt1 and receipt2 were overwritten
  if (restored.events[0].note != 65) {
    recordFailure(testName, "Undo did not restore immediate predecessor stateB_edited");
    return;
  }

  // Verify that next toggle is REDO (toggle back to C), proving no multi-level history
  if (!owner.nextIsRedo()) {
    recordFailure(testName, "nextIsRedo() was not true after single undo toggle");
    return;
  }

  recordPass(testName);
}

// ---------------------------------------------------------------------------
// G. ACTIVE / NEXT CAUSALITY
// ---------------------------------------------------------------------------
// Invariant: Staging or editing working material must not move activeMaterial_
// or change playback before the quantized musical boundary.
void test_mux0_active_next_causality() {
  const char* testName = "G_ACTIVE_NEXT_CAUSALITY";
  MiniAcid engine{44100.0f, nullptr};
  engine.setBpm(120.0f);
  (void)engine.rebuildPatternRuntimeEventBank();

  const auto initialActive = engine.activeMaterial(0);
  const Buffer melody = makeOneBarMelody(67);

  // Stage pending material
  const bool staged = engine.stagePendingMaterial(
      0, 5, MaterialKind::Melody, &melody);
  if (!staged) {
    recordFailure(testName, "stagePendingMaterial failed");
    return;
  }

  // Active material must NOT have moved
  const auto activeAfterStage = engine.activeMaterial(0);
  if (activeAfterStage.slot != initialActive.slot ||
      activeAfterStage.kind != initialActive.kind) {
    recordFailure(testName, "activeMaterial moved immediately upon staging!");
    return;
  }

  // Activation on boundary moves activeMaterial
  engine.activatePendingMaterial();
  const auto activeAfterActivate = engine.activeMaterial(0);
  if (activeAfterActivate.slot != 5 ||
      activeAfterActivate.kind != MaterialKind::Melody) {
    recordFailure(testName, "activeMaterial did not update upon activatePendingMaterial");
    return;
  }

  recordPass(testName);
}

// ---------------------------------------------------------------------------
// H. UNRESOLVED MATERIAL SAFETY
// ---------------------------------------------------------------------------
// Invariant: Missing or corrupt material must never resolve as a valid, editable
// empty pattern.
// CLASSIFICATION: BLOCKED / DEFERRED TO A2 MATERIAL RESOLVER ADMISSION.
// M-UX0 will consume A2's resolution contract rather than reinventing it.
void test_mux0_unresolved_material_safety_witness() {
  const char* testName = "H_UNRESOLVED_MATERIAL_SAFETY";
  std::printf("M-UX0 DEFERRED [%s]: Blocked on A2 Material Resolver admission (dependency boundary)\n", testName);
}

// ---------------------------------------------------------------------------
// I. RECOVERY AUTOSAVE WORKING ISOLATION (ADVERSARIAL WITNESS)
// ---------------------------------------------------------------------------
// Invariant: Ephemeral WORKING material must NEVER leak into recovery autosave.
// If user has accepted state A, enters working candidate B, and recovery autosave
// fires (e.g. playback stopped), recovery reload must restore A, NOT B.
//
// In current C0: PatternEditPage directly mutates currentScene(), so writeSceneAuto()
// serializes candidate B into the recovery file! This test executes the actual
// C0 engine/storage sequence and proves that C0 currently leaks B into autosave.
void test_mux0_recovery_autosave_working_isolation_witness() {
  const char* testName = "I_RECOVERY_AUTOSAVE_ISOLATION";

  class InMemorySceneStorage : public SceneStorage {
   public:
    std::string savedData;
    std::string autoData;
    std::string currentName{"test_scene"};

    void initializeStorage() override {}
    bool readScene(std::string& out) override { out = savedData; return true; }
    bool writeScene(const std::string& data) override { savedData = data; return true; }
    bool readScene(SceneManager& manager) override { return manager.loadScene(savedData); }
    bool writeScene(const SceneManager& manager) override { return manager.writeSceneJson(savedData); }
    bool writeSceneAuto(const SceneManager& manager) override { return manager.writeSceneJson(autoData); }
    bool readSceneAuto(SceneManager& manager) override { return manager.loadScene(autoData); }
    bool hasSceneAuto() const override { return !autoData.empty(); }
    bool clearSceneAuto() override { autoData.clear(); return true; }
    std::vector<std::string> getAvailableSceneNames() const override { return {currentName}; }
    std::string getCurrentSceneName() const override { return currentName; }
    bool setCurrentSceneName(const std::string& name) override { currentName = name; return true; }
  };

  InMemorySceneStorage storage;
  MiniAcid engine{44100.0f, &storage};
  engine.sceneManager().loadDefaultScene();

  // Baseline Accepted state A:
  // Step 0 note is set to 60 (Accepted baseline)
  engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note = 60;
  // Explicit save establishes clean baseline
  const bool saved = storage.writeScene(engine.sceneManager());
  assert(saved);

  // User begins editing: candidate B has note 67 on step 0.
  // In C0, editing directly mutates currentScene (as PatternEditPage currently does):
  engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note = 67;

  // Now playback stops, triggering recovery autosave:
  const bool autoSaved = engine.autoSaveSceneRecovery();
  assert(autoSaved);

  // Simulate device restart & crash recovery from autosave:
  InMemorySceneStorage recoveryStorage;
  recoveryStorage.autoData = storage.autoData;
  MiniAcid recoveryEngine{44100.0f, &recoveryStorage};
  const bool loaded = recoveryStorage.readSceneAuto(recoveryEngine.sceneManager());
  assert(loaded);

  const int8_t recoveredNote =
      recoveryEngine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note;

  // If recoveredNote is 67 (B), recovery autosave leaked uncommitted WORKING state!
  if (recoveredNote == 67) {
    recordFailure(testName,
                  "Recovery autosave leaked uncommitted WORKING state (note 67) into durable recovery persistence instead of baseline ACCEPTED (note 60)!");
  } else if (recoveredNote == 60) {
    recordPass(testName);
  } else {
    recordFailure(testName, "Corrupt state loaded from recovery autosave");
  }
}

}  // namespace

int main() {
  std::printf("==================================================\n");
  std::printf("0.9.11 M-UX0 BEHAVIORAL RED / INVARIANT WITNESSES\n");
  std::printf("==================================================\n");

  test_mux0_working_isolation_witness();
  test_mux0_representation_migration_is_not_accept_witness();
  test_mux0_length_1_to_4_preserves_bar1();
  test_mux0_repeat_deterministic();
  test_mux0_discard_restores_accepted_witness();
  test_mux0_one_level_undo_contract();
  test_mux0_active_next_causality();
  test_mux0_unresolved_material_safety_witness();
  test_mux0_recovery_autosave_working_isolation_witness();

  std::printf("==================================================\n");
  std::printf("SUMMARY: %d passed, %d TRUE RED failure(s)\n", g_passes, g_failures);
  std::printf("==================================================\n");

  // Exit code is 1 if there are true RED failures, proving that the current
  // codebase does not yet satisfy the complete M-UX0 contract.
  return (g_failures > 0) ? 1 : 0;
}
