#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/phrase/runtime_pattern_event_bank.h"
#include "src/state/material_slot_access.h"
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"
#include "src/ui/phrase_instrument_controls.h"

SerialMock Serial;
SDMock SD;

namespace {
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using GroovePuterMaterial::MaterialKind;

int g_red = 0;
int g_green = 0;
int g_stop = 0;

void green(const char* id, const char* detail) {
  std::printf("M-WORKING GREEN [%s]: %s\n", id, detail);
  ++g_green;
}

void red(const char* id, const char* detail) {
  std::fprintf(stderr, "M-WORKING RED [%s]: %s\n", id, detail);
  ++g_red;
}

void stop(const char* id, const char* detail) {
  std::fprintf(stderr, "M-WORKING STOP [%s]: %s\n", id, detail);
  ++g_stop;
}

void deferred(const char* id, const char* detail) {
  std::printf("M-WORKING DEFERRED [%s]: %s\n", id, detail);
}

bool sameStep(const SynthStep& a, const SynthStep& b) {
  return a.note == b.note && a.slide == b.slide && a.accent == b.accent &&
         a.ghost == b.ghost && a.velocity == b.velocity &&
         a.timing == b.timing && a.fx == b.fx && a.fxParam == b.fxParam &&
         a.probability == b.probability;
}

Buffer melody(uint8_t note = 60) {
  Buffer out{};
  out.lengthTicks = PhraseRuntime::kTicksPerBar;
  out.count = 1;
  out.events[0] = {0, 24u * PhraseRuntime::kSubticksPerTick,
                   note, 100, 100, 0, 0, 0};
  return out;
}

class InMemorySceneStorage final : public SceneStorage {
 public:
  std::string savedData;
  std::string autoData;
  std::string currentName{"m-working"};

  void initializeStorage() override {}
  bool readScene(std::string& out) override { out = savedData; return true; }
  bool writeScene(const std::string& data) override { savedData = data; return true; }
  bool readScene(SceneManager& manager) override { return manager.loadScene(savedData); }
  bool writeScene(const SceneManager& manager) override {
    return manager.writeSceneJson(savedData);
  }
  bool writeSceneAuto(const SceneManager& manager) override {
    return manager.writeSceneJson(autoData);
  }
  bool readSceneAuto(SceneManager& manager) override {
    return manager.loadScene(autoData);
  }
  bool hasSceneAuto() const override { return !autoData.empty(); }
  bool clearSceneAuto() override { autoData.clear(); return true; }
  std::vector<std::string> getAvailableSceneNames() const override {
    return {currentName};
  }
  std::string getCurrentSceneName() const override { return currentName; }
  bool setCurrentSceneName(const std::string& name) override {
    currentName = name;
    return true;
  }
};

void test_pattern_edit_isolation_and_losslessness() {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  SynthPattern& accepted =
      engine.sceneManager().currentScene().synthABanks[0].patterns[0];
  accepted.steps[0].note = 60;
  accepted.steps[3].note = 61;
  accepted.steps[3].slide = true;
  accepted.steps[3].accent = true;
  accepted.steps[3].ghost = true;
  accepted.steps[3].velocity = 73;
  accepted.steps[3].timing = -7;
  accepted.steps[3].fx = static_cast<uint8_t>(StepFx::Reverse);
  accepted.steps[3].fxParam = 91;
  accepted.steps[3].probability = 47;
  const SynthStep untouched = accepted.steps[3];

  // Build the same resident bank the Pattern audio path reads, before the edit.
  // A correct Working edit must publish B into this bank while ACCEPTED A stays
  // unchanged; rebuilding a private local bank from Scene would not prove that.
  assert(engine.rebuildPatternRuntimeEventBank());

  engine.adjust303StepNote(0, 0, 1);
  const SynthPattern& candidate = engine.activeSynthPattern(0);

  if (accepted.steps[0].note == 60) {
    green("MW-A", "Pattern edit left ACCEPTED Scene unchanged");
  } else {
    red("MW-A", "Pattern edit wrote directly into ACCEPTED Scene");
  }

  if (sameStep(accepted.steps[3], untouched) &&
      sameStep(candidate.steps[3], untouched)) {
    green("MW-B", "untouched SynthPattern fields survived in ACCEPTED and candidate");
  } else {
    red("MW-B", "unrelated SynthPattern semantics changed across the Working boundary");
  }

  const auto* ev =
      engine.patternRuntimeBank_.select(0, 0, 0).eventForSourceStep(0);
  if (ev && ev->note == 61 && candidate.steps[0].note == 61 &&
      accepted.steps[0].note == 60) {
    green("MW-C", "real Pattern audio bank hears Working B while ACCEPTED remains A");
  } else {
    red("MW-C", "Pattern audio bank is stale or depends on mutating ACCEPTED Scene");
  }
}

void test_pattern_undo_is_currently_persistent() {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  auto& manager = engine.sceneManager();
  manager.currentScene().synthABanks[0].patterns[0].steps[0].note = 60;

  auto& owner = GroovePuterUndo::undoOwner();
  owner.clear();
  const auto revisionBefore = GroovePuterState::sceneRevisionSnapshot();

  GroovePuterUndo::SynthPatternUndoPayload before{};
  if (!GroovePuterUndo::captureCurrentSynthPatternUndo(manager, 0, before)) {
    red("MW-D", "could not capture existing Pattern undo owner");
    return;
  }
  auto after = before;
  after.before.steps[0].note = 61;
  const bool committed = owner.commitPrepared(
      GroovePuterUndo::UndoKind::Pattern, before,
      [&]() { GroovePuterUndo::restoreSynthPatternUndo(manager, after); });

  const bool canonicalMoved =
      manager.currentScene().synthABanks[0].patterns[0].steps[0].note == 61;
  const bool revisionMoved =
      GroovePuterState::sceneRevisionSnapshot().currentRevision !=
      revisionBefore.currentRevision;
  if (committed && !canonicalMoved && !revisionMoved) {
    green("MW-D", "Pattern Undo receipt is session-only Working state");
  } else {
    red("MW-D", "existing Pattern Undo commit mutates Scene/revision rather than WORKING");
  }

  owner.clear();
  GroovePuterState::restoreSceneRevision(revisionBefore);
}

void test_discard_and_modified_contract_absence() {
  red("MW-E", "no explicit discardWorking domain operation exists on this baseline");

  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  Scene& scene = engine.sceneManager().currentScene();
  SynthPattern& accepted = scene.synthABanks[0].patterns[0];
  accepted.steps[0].note = 60;

  constexpr GroovePuterMaterial::MaterialReference ref{
      GroovePuterMaterial::MaterialAddress{0, 0},
      GroovePuterMaterial::MaterialId{101}};
  scene.materialSlots[0][0].id = ref.id;

  const bool emptyIsClean = !engine.hasModifiedWorking303Pattern(0);

  SynthPattern working = accepted;
  working.steps[0].note = 61;
  engine.workingMaterial_[0].storePattern(working, ref);
  const bool patternModified = engine.hasModifiedWorking303Pattern(0);
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  const bool survivesSourceToggle = engine.hasModifiedWorking303Pattern(0);

  if (emptyIsClean && patternModified && survivesSourceToggle &&
      accepted.steps[0].note == 60) {
    green("MW-K", "modified state is derived from identity-bound WORKING versus ACCEPTED and survives playback-source toggles");
  } else {
    red("MW-K", "identity-bound derived modified-state contract is missing or coupled to playback source");
  }

  red("MW-L", "no modified-Working target-switch refusal contract exists on this baseline");
}

void test_recovery_serializes_accepted_only() {
  InMemorySceneStorage storage;
  MiniAcid engine{44100.0f, &storage};
  engine.sceneManager().loadDefaultScene();
  engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note = 60;
  assert(storage.writeScene(engine.sceneManager()));

  // This is the actual current Pattern edit ownership failure: candidate B is
  // represented by mutating canonical Scene.
  engine.adjust303StepNote(0, 0, 7);
  assert(engine.autoSaveSceneRecovery());

  InMemorySceneStorage recovery;
  recovery.autoData = storage.autoData;
  MiniAcid restarted{44100.0f, &recovery};
  assert(recovery.readSceneAuto(restarted.sceneManager()));
  const int recovered = restarted.sceneManager().currentScene()
                            .synthABanks[0].patterns[0].steps[0].note;
  if (recovered == 60) {
    green("MW-F", "recovery serialized ACCEPTED A while auditioning Working B");
  } else {
    red("MW-F", "recovery serialized the unaccepted Pattern edit instead of ACCEPTED A");
  }
}

void test_representation_migration_contract() {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  auto& accepted = engine.sceneManager().currentScene().synthABanks[0].patterns[0];
  accepted.steps[0].note = 60;

  const MaterialKind beforeKind = GroovePuterMaterial::residentKind(
      engine.sceneManager().currentScene(), 0, 0);
  const bool made = engine.makePhrase(0);
  const MaterialKind afterKind = GroovePuterMaterial::residentKind(
      engine.sceneManager().currentScene(), 0, 0);
  if (made && beforeKind == MaterialKind::Pattern && afterKind == MaterialKind::Pattern) {
    green("MW-G", "makePhrase does not flip the canonical material descriptor");
  } else {
    red("MW-G", "representation migration changed canonical descriptor or failed");
  }

  MiniAcid workingWitness{44100.0f, nullptr};
  workingWitness.sceneManager().loadDefaultScene();
  auto& scenePattern = workingWitness.sceneManager().currentScene()
                           .synthABanks[0].patterns[0];
  scenePattern.steps[0].note = 60;
  workingWitness.adjust303StepNote(0, 0, 7);
  const bool converted = workingWitness.makePhrase(0);
  const Buffer projected = workingWitness.currentPhraseBuffer(0);
  const bool projectedB = converted && projected.count > 0 && projected.events[0].note == 67;
  if (projectedB && scenePattern.steps[0].note == 60) {
    green("MW-H", "Pattern->Melody conversion used Working B while ACCEPTED stayed A");
  } else {
    red("MW-H", "conversion can only use B after B has already overwritten ACCEPTED Pattern A");
  }

  if (converted && projected.count > 0) {
    Buffer expanded{};
    const bool prepared = PhraseInstrumentControls::prepareLengthTarget(
        projected, 4, expanded);
    bool bar1Same = prepared && expanded.count == projected.count;
    for (uint16_t i = 0; bar1Same && i < projected.count; ++i) {
      bar1Same = std::memcmp(&projected.events[i], &expanded.events[i],
                             sizeof(PhraseRuntime::RuntimeSynthEvent)) == 0;
    }
    if (bar1Same && expanded.lengthTicks == PhraseRuntime::kTicksPerBar * 4u) {
      green("MW-I", "existing length preparation preserves bar-1 event bytes");
    } else {
      red("MW-I", "1->4 representation boundary changed existing bar-1 content");
    }
  } else {
    red("MW-I", "could not characterize 1->4 preservation because projection failed");
  }
}

void test_melody_and_source_round_trip_gate() {
  MiniAcid engine{44100.0f, nullptr};
  Buffer original = melody(77);
  engine.currentPhraseBuffer(0) = original;
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);

  if (!engine.setPhraseLength(0, 2)) {
    red("MW-J", "existing Melody length edit failed");
  } else if (engine.currentPhraseBuffer(0).events[0].note == 77 &&
             engine.currentPhraseBuffer(0).lengthTicks ==
                 PhraseRuntime::kTicksPerBar * 2u) {
    green("MW-J", "existing Melody Working buffer remains editable");
  } else {
    red("MW-J", "existing Melody edit changed retained event content");
  }

  const Buffer beforeToggle = engine.currentPhraseBuffer(0);
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  const Buffer afterToggle = engine.currentPhraseBuffer(0);
  if (std::memcmp(&beforeToggle, &afterToggle, sizeof(Buffer)) == 0) {
    std::printf("M-WORKING GATE [SOURCE_ROUND_TRIP]: playback-only Pattern/Phrase source toggle preserves Melody bytes; do not overwrite Working merely because playback source changes\n");
  } else {
    stop("SOURCE_ROUND_TRIP",
         "accepted behavior requires Melody bytes to survive a playback-only source round-trip");
  }
}

void test_active_next_causality() {
  MiniAcid engine{44100.0f, nullptr};
  const auto initial = engine.activeMaterial(0);
  const Buffer next = melody(69);
  const bool staged = engine.stagePendingMaterial(0, 5, MaterialKind::Melody, &next);
  const auto afterStage = engine.activeMaterial(0);
  if (!staged || afterStage.slot != initial.slot || afterStage.kind != initial.kind) {
    red("MW-O", "staging NEXT moved ACTIVE or failed unexpectedly");
    return;
  }
  engine.activatePendingMaterial();
  const auto afterActivate = engine.activeMaterial(0);
  if (afterActivate.slot == 5 && afterActivate.kind == MaterialKind::Melody) {
    green("MW-O", "ACTIVE moves only at explicit activation boundary");
  } else {
    red("MW-O", "boundary activation did not publish prepared material");
  }
}

}  // namespace

int main() {
  std::printf("0.9.11 M-WORKING behavioral RED characterization\n");
  test_pattern_edit_isolation_and_losslessness();
  test_pattern_undo_is_currently_persistent();
  test_discard_and_modified_contract_absence();
  test_recovery_serializes_accepted_only();
  test_representation_migration_contract();
  test_melody_and_source_round_trip_gate();
  deferred("MW-M", "WorkingMaterialStorage type does not exist yet; size gate belongs to Task 2");
  deferred("MW-N", "exact-head Cardputer ELF/DRAM measurement belongs to final implementation gate");
  test_active_next_causality();

  std::printf("M-WORKING SUMMARY: %d GREEN, %d RED, %d STOP\n",
              g_green, g_red, g_stop);
  if (g_stop != 0) return 2;
  return g_red == 0 ? 0 : 1;
}
