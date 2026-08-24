#include "platform_sdl/arduino_compat.h"
#include "scenes.h"
#include "src/phrase/phrase_keep.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

SerialMock Serial;
SDMock SD;

namespace {

constexpr int kPage = 0;
constexpr uint8_t kAll = SongPatternMaterializer::kEditableTrackMask;

PatternAddress addressOf(int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  assert(address.valid());
  assert(address.page == kPage);
  return address;
}

void resetProject(SceneManager& manager) {
  GroovePuterUndo::undoOwner().clear();
  manager.currentScene() = Scene{};
  PhraseCore::reset(manager.currentScene().phraseBank);
  GroovePuterState::restoreSceneRevision({400, 400});
}

void writeCandidate(Scene& scene, int globalPattern, int token) {
  const PatternAddress address = addressOf(globalPattern);
  scene.synthABanks[address.bank].patterns[address.slot].steps[0].note =
      static_cast<int8_t>(40 + token);
  scene.synthBBanks[address.bank].patterns[address.slot].steps[0].note =
      static_cast<int8_t>(52 + token);
  scene.drumBanks[address.bank].patterns[address.slot]
      .voices[0].steps[0].hit = 1;
  scene.drumBanks[address.bank].patterns[address.slot]
      .voices[0].steps[0].velocity = static_cast<uint8_t>(90 + token);
}

bool backingPresent(const Scene& scene, int globalPattern) {
  const PatternAddress address = addressOf(globalPattern);
  const int local = address.bank * Bank<SynthPattern>::kPatterns + address.slot;
  return !SongPatternMaterializer::slotContentIsEmpty(
             scene, SongTrack::SynthA, local) &&
         !SongPatternMaterializer::slotContentIsEmpty(
             scene, SongTrack::SynthB, local) &&
         !SongPatternMaterializer::slotContentIsEmpty(
             scene, SongTrack::Drums, local);
}

bool backingCleared(const Scene& scene, int globalPattern) {
  const PatternAddress address = addressOf(globalPattern);
  const int local = address.bank * Bank<SynthPattern>::kPatterns + address.slot;
  return SongPatternMaterializer::slotContentIsEmpty(
             scene, SongTrack::SynthA, local) &&
         SongPatternMaterializer::slotContentIsEmpty(
             scene, SongTrack::SynthB, local) &&
         SongPatternMaterializer::slotContentIsEmpty(
             scene, SongTrack::Drums, local);
}

PhrasePatternLease::PatternLease acquireCandidate(Scene& scene) {
  PhrasePatternLease::PatternLease lease{};
  const auto acquired = PhrasePatternLease::patternLeaseOwner().acquire(
      scene, kPage, 1, kAll, lease, 0);
  assert(acquired);
  writeCandidate(scene, lease.globalPattern[0], 1);
  return lease;
}

GroovePuterUndo::UndoResult togglePhrase(Scene& scene) {
  return GroovePuterUndo::undoOwner().togglePrepared<
      GroovePuterUndo::PhraseUndoPayload>(
      GroovePuterUndo::UndoKind::Phrase,
      [](const GroovePuterUndo::PhraseUndoPayload& receipt) {
        return receipt.pageIndex == kPage;
      },
      [&](GroovePuterUndo::PhraseUndoPayload& receipt) {
        GroovePuterUndo::exchangeFixedValue(scene.phraseBank, receipt.before);
      });
}

void assertLoadedKeep(const Scene& scene, int globalPattern) {
  const PhraseCore::PhraseSlot* slot =
      PhraseCore::slotAt(scene.phraseBank, PhraseCore::SlotId::A);
  assert(slot != nullptr);
  assert(PhraseCore::isValid(*slot));
  assert(slot->metadata.source == PhraseCore::Source::Generated);
  assert(slot->metadata.storage == PhraseCore::StorageMode::ReferenceView);
  assert(slot->metadata.lengthBars == 1);
  assert(slot->metadata.trackMask == kAll);
  for (int track = 0; track < PhraseCore::kTrackCount; ++track) {
    assert(slot->patternRefs[0][track] == globalPattern);
  }
  assert(backingPresent(scene, globalPattern));
}

void testAcceptedKeepSurvivesSceneJsonRoundTrip() {
  SceneManager manager;
  resetProject(manager);
  Scene& scene = manager.currentScene();

  auto lease = acquireCandidate(scene);
  const int globalPattern = lease.globalPattern[0];
  assert(PhraseKeep::keep(
      manager, lease, {PhraseCore::SlotId::A, PhraseCore::Role::Main}));
  assert(!lease.isActive());

  const std::string json = manager.dumpCurrentScene();
  assert(!json.empty());
  assert(json.find("\"phraseCore\":[") != std::string::npos);
  assert(json.find("UndoLifecycleMetadata") == std::string::npos);
  assert(json.find("PatternLease") == std::string::npos);

  // A project load follows reboot-style runtime ownership semantics: accepted
  // persistent data survives, the one-slot runtime Undo pair does not.
  assert(manager.loadScene(json));
  assert(!GroovePuterUndo::undoOwner().hasUndo());
  assertLoadedKeep(manager.currentScene(), globalPattern);
}

void testRedoOnlyBackingIsNotSerializedAsProjectMaterial() {
  SceneManager manager;
  resetProject(manager);
  Scene& scene = manager.currentScene();

  auto lease = acquireCandidate(scene);
  const int globalPattern = lease.globalPattern[0];
  assert(PhraseKeep::keep(
      manager, lease, {PhraseCore::SlotId::A, PhraseCore::Role::Main}));
  assert(togglePhrase(scene) == GroovePuterUndo::UndoResult::Restored);
  assert(!PhraseCore::isValid(scene.phraseBank.slots[0]));
  assert(backingPresent(scene, globalPattern));
  assert(GroovePuterUndo::undoOwner().retainsPatternBacking(globalPattern, kAll));

  const std::string json = manager.dumpCurrentScene();
  assert(!json.empty());
  // dumpCurrentScene must sanitize a detached view only; runtime Redo remains
  // usable until the project replacement below.
  assert(backingPresent(scene, globalPattern));
  assert(GroovePuterUndo::undoOwner().hasUndo());

  assert(manager.loadScene(json));
  assert(!GroovePuterUndo::undoOwner().hasUndo());
  assert(!PhraseCore::isValid(manager.currentScene().phraseBank.slots[0]));
  assert(backingCleared(manager.currentScene(), globalPattern));
}

void testFailedSceneLoadDoesNotConsumeRuntimeRedo() {
  SceneManager manager;
  resetProject(manager);
  Scene& scene = manager.currentScene();

  auto lease = acquireCandidate(scene);
  const int globalPattern = lease.globalPattern[0];
  assert(PhraseKeep::keep(manager, lease, {}));
  assert(togglePhrase(scene) == GroovePuterUndo::UndoResult::Restored);

  assert(!manager.loadScene("{not-json"));
  assert(GroovePuterUndo::undoOwner().hasUndo());
  assert(GroovePuterUndo::undoOwner().retainsPatternBacking(globalPattern, kAll));
  assert(backingPresent(scene, globalPattern));

  assert(togglePhrase(scene) == GroovePuterUndo::UndoResult::Restored);
  assertLoadedKeep(scene, globalPattern);
  GroovePuterUndo::undoOwner().clear();
}

}  // namespace

int main() {
  testAcceptedKeepSurvivesSceneJsonRoundTrip();
  testRedoOnlyBackingIsNotSerializedAsProjectMaterial();
  testFailedSceneLoadDoesNotConsumeRuntimeRedo();
  std::puts("0.9.9-P1b Scene JSON persistence: PASS");
  return 0;
}
