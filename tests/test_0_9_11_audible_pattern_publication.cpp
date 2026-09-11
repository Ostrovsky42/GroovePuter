#include "../scenes.h"
#include "../src/phrase/runtime_synth_events.h"
#include "../src/phrase/runtime_pattern_event_bank.h"
#include "../src/generation/migration/strong_rhythm_migration.h"
#include "../src/state/generation_request_state.h"
#include "../src/state/scene_revision.h"
#include "../src/state/undo_owner.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>

class TestSceneManager {
 public:
  int currentPageIndex() const { return 0; }
  int getCurrentBankIndex(int) const { return 0; }
  int getCurrentSynthPatternIndex(int) const { return 0; }
  int getCurrentDrumPatternIndex() const { return 0; }

  Scene& currentScene() { return scene_; }
  const Scene& currentScene() const { return scene_; }

  void setMode(GrooveboxMode mode) { mode_ = mode; }
  GrooveboxMode getMode() const { return mode_; }
  void setBpm(float bpm) { bpm_ = bpm; }
  float getBpm() const { return bpm_; }

 private:
  Scene scene_{};
  GrooveboxMode mode_ = GrooveboxMode::Minimal;
  float bpm_ = 100.0f;
};

class MiniAcid;

class GrooveboxModeManager {
 public:
  explicit GrooveboxModeManager(MiniAcid&) {}
  int flavor() const { return 0; }
  uint32_t generationSeed() const { return 1; }
  void setModeLocal(GrooveboxMode) {}
  void setFlavorLocal(int) {}
  void setGenerationSeed(uint32_t) {}
  void generatePattern(SynthPattern&,
                       float,
                       const GenerativeParams&,
                       const GenreBehavior&,
                       int = 0) const {}
  void generateDrumPattern(DrumPatternSet&,
                           const GenerativeParams&,
                           const GenreBehavior&) const {}
};

class TestGenreManager {
 public:
  using PendingCommitHook = bool (*)(TestSceneManager&);
  void setPendingCommitHook(PendingCommitHook hook) { hook_ = hook; }

 private:
  PendingCommitHook hook_ = nullptr;
};

class MiniAcid {
 public:
  MiniAcid() : modeManager_(*this) {}

  TestSceneManager& sceneManager() { return scenes_; }
  const TestSceneManager& sceneManager() const { return scenes_; }

  bool isPlaying() const { return playing_; }
  void setPlaying(bool playing) { playing_ = playing; }

  float bpm() const { return bpm_; }
  void setBpm(float bpm) { bpm_ = bpm; }

  GrooveboxMode grooveboxMode() const { return mode_; }
  void setGrooveboxMode(GrooveboxMode mode) { mode_ = mode; }
  void activateCommittedGrooveboxModeRuntime(GrooveboxMode mode) { mode_ = mode; }

  GrooveboxModeManager& modeManager() { return modeManager_; }
  const GrooveboxModeManager& modeManager() const { return modeManager_; }

  TestGenreManager& genreManager() { return genreManager_; }
  const TestGenreManager& genreManager() const { return genreManager_; }

  void regeneratePatternsWithGenre() {}

  bool rebuildPatternRuntimeEventBank() {
    ++rebuildCalls_;
    publishedNotes_[0] = scenes_.currentScene().synthABanks[0].patterns[0].steps[0].note;
    publishedNotes_[1] = scenes_.currentScene().synthBBanks[0].patterns[0].steps[0].note;
    return true;
  }

  bool refreshPatternRuntimeEvents(int synthIndex, int bankIndex, int patternIndex) {
    ++refreshCalls_;
    lastRefreshSynth_ = synthIndex;
    lastRefreshBank_ = bankIndex;
    lastRefreshPattern_ = patternIndex;
    if (synthIndex == 0) {
      publishedNotes_[0] = scenes_.currentScene()
          .synthABanks[bankIndex].patterns[patternIndex].steps[0].note;
    } else {
      publishedNotes_[1] = scenes_.currentScene()
          .synthBBanks[bankIndex].patterns[patternIndex].steps[0].note;
    }
    return true;
  }

  const PhraseRuntime::RuntimePatternEventBuffer& activePatternRuntimeEvents(
      int synthIndex) const {
    return runtimeEvents_[synthIndex == 1 ? 1 : 0];
  }

  void barrierPatternRuntimeSourceTransition() { ++barrierCalls_; }

  int refreshCalls() const { return refreshCalls_; }
  int rebuildCalls() const { return rebuildCalls_; }
  int barrierCalls() const { return barrierCalls_; }
  int publishedNote(int voice) const { return publishedNotes_[voice == 1 ? 1 : 0]; }
  int lastRefreshSynth() const { return lastRefreshSynth_; }
  int lastRefreshBank() const { return lastRefreshBank_; }
  int lastRefreshPattern() const { return lastRefreshPattern_; }

 private:
  TestSceneManager scenes_{};
  bool playing_ = false;
  float bpm_ = 100.0f;
  GrooveboxMode mode_ = GrooveboxMode::Minimal;
  GrooveboxModeManager modeManager_;
  TestGenreManager genreManager_{};
  PhraseRuntime::RuntimePatternEventBuffer runtimeEvents_[2]{};
  int refreshCalls_ = 0;
  int rebuildCalls_ = 0;
  int barrierCalls_ = 0;
  int publishedNotes_[2] = {-127, -127};
  int lastRefreshSynth_ = -1;
  int lastRefreshBank_ = -1;
  int lastRefreshPattern_ = -1;
};

struct AtlasRuntimeMetadata {
  const char* atlasRecipeId = nullptr;
  const char* displayName = nullptr;
  const char* atlasPatternId = nullptr;
  const char* slotId = nullptr;
  const char* slotFunction = nullptr;
  uint16_t bpm = 120;
  uint8_t swingPercent = 50;
};

namespace AtlasRuntime {
inline bool applyRecipe(uint8_t,
                        uint8_t,
                        SynthPattern&,
                        SynthPattern&,
                        DrumPatternSet&,
                        AtlasRuntimeMetadata* metadata = nullptr) {
  if (metadata != nullptr) {
    metadata->bpm = 120;
    metadata->swingPercent = 50;
  }
  return true;
}
}  // namespace AtlasRuntime

namespace GenreCatalog {
GenerativeParams compiledGenerativeParams(const GenreSettings&) { return {}; }
GenreBehavior behavior(const GenreSettings&) { return {}; }
}  // namespace GenreCatalog

static GroovePuterRhythm::StrongRhythmMigrationStatus g_testMigrationStatus =
    GroovePuterRhythm::StrongRhythmMigrationStatus::Applied;

namespace GroovePuterRhythm {
inline StrongRhythmRoute testSelectStrongRhythmRoute(const GenreSettings&) {
  return StrongRhythmRoute::TechnoBase;
}

inline StrongRhythmMigrationResult testMigrateStrongRhythmMaterial(
    const GenreSettings&,
    const StrongRhythmMigrationContext&,
    DrumPatternSet&,
    SynthPattern&,
    SynthPattern&) {
  StrongRhythmMigrationResult result{};
  result.status = ::g_testMigrationStatus;
  result.route = StrongRhythmRoute::TechnoBase;
  return result;
}

inline StrongRhythmMigrationResult testMigrateStrongRhythmSynths(
    const GenreSettings&,
    const StrongRhythmMigrationContext&,
    DrumPatternSet&,
    SynthPattern&,
    SynthPattern&) {
  StrongRhythmMigrationResult result{};
  result.status = ::g_testMigrationStatus;
  result.route = StrongRhythmRoute::TechnoBase;
  return result;
}
}  // namespace GroovePuterRhythm

#define GROOVEPUTER_DSP_ATLAS_RUNTIME_H
#define MINIACID_ENGINE_H
#define GROOVEPUTER_DSP_MODE_MANAGER_H
#define SceneManager TestSceneManager
#define selectStrongRhythmRoute testSelectStrongRhythmRoute
#define migrateStrongRhythmMaterial testMigrateStrongRhythmMaterial
#define migrateStrongRhythmSynths testMigrateStrongRhythmSynths
#define commitQuantizedGenerationAtBarStart legacyCommitQuantizedGenerationAtBarStart
#define regenerateWithQuantizedCommit legacyRegenerateWithQuantizedCommit
#define regenerateSynthWithQuantizedCommit legacyRegenerateSynthWithQuantizedCommit
#include "../src/generation/migration/quantized_generation_commit_impl.h"
#undef regenerateSynthWithQuantizedCommit
#undef regenerateWithQuantizedCommit
#undef commitQuantizedGenerationAtBarStart
#include "../src/generation/migration/quantized_generation_undo_owner_impl.h"
#undef migrateStrongRhythmSynths
#undef migrateStrongRhythmMaterial
#undef selectStrongRhythmRoute
#undef SceneManager

namespace {

using namespace GroovePuterRhythm;
using namespace GroovePuterRhythm::QuantizedGenerationDetail;

void resetState() {
  g_publishedSlot.store(-1, std::memory_order_release);
  for (int i = 0; i < 2; ++i) {
    g_slotState[i].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    g_slots[i] = PendingGeneration{};
  }
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::Idle),
      std::memory_order_release);
  g_commitSerial.store(0, std::memory_order_release);
  GroovePuterUndo::undoOwner().clear();
  GroovePuterState::restoreSceneRevision({100, 100});
  GroovePuterState::resetGenerationAttemptState();
}

void testCompactActivationPublishesCommittedRuntimeBeforeReady() {
  resetState();
  MiniAcid engine;
  engine.setPlaying(true);

  Scene& scene = engine.sceneManager().currentScene();
  scene.synthABanks[0].patterns[0].steps[0].note = 36;
  const PatternTarget target = captureGenerationActivationTarget(engine.sceneManager());
  const SynthPattern before = scene.synthABanks[0].patterns[0];

  const int slot = armCompactSynthActivation(engine, target, 0, before);
  assert(slot >= 0);
  assert(g_slotState[slot].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Armed));

  // Simulate the persistent COMMIT performed by PatternEditPage. The visible
  // Scene now contains the new note while the pending snapshot still owns the
  // old audible side until BAR_START.
  scene.synthABanks[0].patterns[0].steps[0].note = 72;
  completeArmedActivation(slot, 100);

  // RED on 85f5...: completeArmedActivation marks the slot Ready without
  // publishing the newly committed Pattern into the runtime event bank.
  assert(engine.refreshCalls() == 1);
  assert(engine.lastRefreshSynth() == 0);
  assert(engine.lastRefreshBank() == 0);
  assert(engine.lastRefreshPattern() == 0);
  assert(engine.publishedNote(0) == 72);
  assert(g_slotState[slot].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Ready));

  assert(commitQuantizedGenerationAtBarStart(engine.sceneManager()));
  assert(engine.barrierCalls() == 1);
  assert(g_publishedSlot.load(std::memory_order_acquire) == -1);
}

void testGenerationUndoRepublishesSynthRuntime() {
  resetState();
  MiniAcid engine;
  Scene& scene = engine.sceneManager().currentScene();
  scene.synthABanks[0].patterns[0].steps[0].note = 74;

  GenerationUndoPayload before{};
  before.target = captureGenerationActivationTarget(engine.sceneManager());
  before.scope = QuantizedGenerationScope::SynthA;
  before.synth[0] = scene.synthABanks[0].patterns[0];
  before.synth[0].steps[0].note = 41;

  restoreGenerationUndo(engine, before);

  assert(scene.synthABanks[0].patterns[0].steps[0].note == 41);
  assert(engine.refreshCalls() == 1);
  assert(engine.publishedNote(0) == 41);
}

void testGenerationToggleRepublishesFullRuntimeBank() {
  resetState();
  MiniAcid engine;
  Scene& scene = engine.sceneManager().currentScene();
  scene.synthABanks[0].patterns[0].steps[0].note = 60;
  scene.synthBBanks[0].patterns[0].steps[0].note = 67;

  GenerationUndoPayload retained{};
  retained.target = captureGenerationActivationTarget(engine.sceneManager());
  retained.scope = QuantizedGenerationScope::Full;
  retained.synth[0] = scene.synthABanks[0].patterns[0];
  retained.synth[1] = scene.synthBBanks[0].patterns[0];
  retained.synth[0].steps[0].note = 43;
  retained.synth[1].steps[0].note = 50;
  retained.genre = scene.genre;
  retained.swingPct = scene.feel.swingPct;
  retained.mode = GrooveboxMode::Minimal;
  retained.bpm = 100.0f;

  exchangeGenerationUndo(engine, retained);

  assert(scene.synthABanks[0].patterns[0].steps[0].note == 43);
  assert(scene.synthBBanks[0].patterns[0].steps[0].note == 50);
  assert(engine.rebuildCalls() == 1);
  assert(engine.publishedNote(0) == 43);
  assert(engine.publishedNote(1) == 50);
}

}  // namespace

int main() {
  testCompactActivationPublishesCommittedRuntimeBeforeReady();
  testGenerationUndoRepublishesSynthRuntime();
  testGenerationToggleRepublishesFullRuntimeBank();
  std::puts("0.9.11 audible Pattern publication: PASS");
  return 0;
}
