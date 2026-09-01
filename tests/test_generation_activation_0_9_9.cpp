#include "../scenes.h"
#include "../src/generation/migration/strong_rhythm_migration.h"
#include "../src/state/generation_request_state.h"
#include "../src/state/scene_revision.h"
#include "../src/state/undo_owner.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <type_traits>

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

 private:
  TestSceneManager scenes_{};
  bool playing_ = false;
  float bpm_ = 100.0f;
  GrooveboxMode mode_ = GrooveboxMode::Minimal;
  GrooveboxModeManager modeManager_;
  TestGenreManager genreManager_{};
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

using GroovePuterRhythm::QuantizedGenerationResult;
using GroovePuterRhythm::StrongRhythmMigrationStatus;

const char* resultName(QuantizedGenerationResult result) {
  switch (result) {
    case QuantizedGenerationResult::Failed: return "Failed";
    case QuantizedGenerationResult::CommittedNow: return "CommittedNow";
    case QuantizedGenerationResult::PendingNextBar: return "PendingNextBar";
    case QuantizedGenerationResult::AttemptUnavailable: return "AttemptUnavailable";
  }
  return "Unknown";
}

void resetQuantizedState() {
  using namespace GroovePuterRhythm;
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;

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

bool expectResult(const char* label,
                  QuantizedGenerationResult actual,
                  QuantizedGenerationResult expected) {
  if (actual == expected) {
    std::printf("%s: OK (%s)\n", label, resultName(actual));
    return true;
  }
  std::fprintf(stderr,
               "%s: expected %s, got %s\n",
               label,
               resultName(expected),
               resultName(actual));
  return false;
}

QuantizedGenerationResult runGenerationCase(
    bool playing,
    StrongRhythmMigrationStatus migrationStatus) {
  resetQuantizedState();
  ::g_testMigrationStatus = migrationStatus;

  MiniAcid engine;
  engine.setPlaying(playing);

  GenreSettings requested{};
  requested.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
  requested.recipe = kBaseRecipeId;

  return GroovePuterRhythm::regenerateWithQuantizedCommit(
      engine, requested, GrooveboxMode::Minimal, false, 100.0f);
}

void testBoundedActivationStorage() {
  using namespace GroovePuterRhythm;
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;

  static_assert(std::is_trivially_copyable<PendingGeneration>::value,
                "pending activation storage must remain a fixed value");
  static_assert(static_cast<uint8_t>(SlotState::Armed) <
                    static_cast<uint8_t>(SlotState::Ready),
                "Armed must precede Ready publication");
  static_assert(static_cast<uint8_t>(QuantizedGenerationStatus::Activated) >
                    static_cast<uint8_t>(QuantizedGenerationStatus::AttemptUnavailable),
                "C statuses must append without renumbering A/B identities");

  resetQuantizedState();

  const WriteLease prepared = acquireWriteLease();
  assert(prepared.slot >= 0 && prepared.slot < 2);
  assert(g_slotState[prepared.slot].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Writing));

  const int activation = acquireCompanionActivationSlot(prepared.slot);
  assert(activation >= 0 && activation < 2 && activation != prepared.slot);
  g_slots[activation].scope = QuantizedGenerationScope::SynthA;
  g_slots[activation].committedRevision = 0;
  armActivationSlot(activation);
  assert(g_publishedSlot.load(std::memory_order_acquire) == activation);
  assert(g_slotState[activation].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Armed));

  const WriteLease rejected = acquireWriteLease();
  assert(rejected.slot < 0);

  g_slots[activation].committedRevision = 77;
  g_slotState[activation].store(
      static_cast<uint8_t>(SlotState::Ready), std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::PendingNextBar),
      std::memory_order_release);
  assert(g_slots[activation].committedRevision == 77);
  assert(g_slotState[activation].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Ready));

  abortArmedActivation(
      activation, QuantizedGenerationStatus::CancelledExplicit);
  assert(g_publishedSlot.load(std::memory_order_acquire) == -1);
  assert(g_slotState[activation].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Empty));
  releaseWriteSlot(prepared.slot);

  const WriteLease next = acquireWriteLease();
  assert(next.slot >= 0);
  releaseWriteSlot(next.slot);

  std::printf("0.9.9-C PendingGeneration=%zu bytes, fixed slots=2\n",
              sizeof(PendingGeneration));
}

}  // namespace

int main() {
  testBoundedActivationStorage();

  bool ok = true;
  ok &= expectResult(
      "STOP Applied",
      runGenerationCase(false, StrongRhythmMigrationStatus::Applied),
      QuantizedGenerationResult::CommittedNow);
  ok &= expectResult(
      "PLAY Applied",
      runGenerationCase(true, StrongRhythmMigrationStatus::Applied),
      QuantizedGenerationResult::PendingNextBar);
  ok &= expectResult(
      "STOP non-Applied",
      runGenerationCase(false, StrongRhythmMigrationStatus::InvalidContext),
      QuantizedGenerationResult::Failed);
  ok &= expectResult(
      "PLAY non-Applied",
      runGenerationCase(true, StrongRhythmMigrationStatus::InvalidContext),
      QuantizedGenerationResult::Failed);

  return ok ? 0 : 1;
}
