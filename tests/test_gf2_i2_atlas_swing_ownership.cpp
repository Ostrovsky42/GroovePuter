// GF2-I2 — Atlas swing stays provenance.
//
// GF2-I1 established that Atlas source BPM is corpus provenance and not a
// second runtime tempo owner. The same audit applies to Atlas swingPercent:
// FeelSettings::swingPct is the musician's offbeat swing, and materializing an
// Atlas-backed recipe must not silently rewrite it with the swing the source
// pattern happened to be recorded at.
//
// This test drives the real production entry points
// (GroovePuterRhythm::regenerateWithQuantizedCommit and
// commitQuantizedGenerationAtBarStart from the undo-owner implementation)
// against the real Atlas corpus and the real generation profile table. Only
// MiniAcid/SceneManager/ModeManager are stubbed.

#include "../scenes.h"
#include "../src/generation/composition/generation_profile.h"
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
  int getCurrentDrumPatternIndex() const { return drumSlot_; }
  void moveDrumPatternIndex(int slot) { drumSlot_ = slot; }

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
  int drumSlot_ = 0;
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

  // Material-only helper on the legacy immediate path. It must never write a
  // tempo; the I1 contract is asserted on the production owner below.
  void regeneratePatternsWithGenre() {}

 private:
  TestSceneManager scenes_{};
  bool playing_ = false;
  float bpm_ = 100.0f;
  GrooveboxMode mode_ = GrooveboxMode::Minimal;
  GrooveboxModeManager modeManager_;
  TestGenreManager genreManager_{};
};

namespace GenreCatalog {
GenerativeParams compiledGenerativeParams(const GenreSettings&) { return {}; }
GenreBehavior behavior(const GenreSettings&) { return {}; }
}  // namespace GenreCatalog

namespace {
bool g_forceMigrationFailure = false;
TestSceneManager* g_retargetDuringMigration = nullptr;
}  // namespace

namespace GroovePuterRhythm {
inline StrongRhythmMigrationResult i1MigrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  if (::g_retargetDuringMigration != nullptr) {
    // The audio transport may move Song/page ownership while generation runs.
    ::g_retargetDuringMigration->moveDrumPatternIndex(1);
    ::g_retargetDuringMigration = nullptr;
  }
  if (::g_forceMigrationFailure) {
    StrongRhythmMigrationResult failed{};
    failed.status = StrongRhythmMigrationStatus::InvalidContext;
    return failed;
  }
  return migrateStrongRhythmMaterial(settings, context, drums, synthA, synthB);
}

inline StrongRhythmMigrationResult i1MigrateStrongRhythmSynths(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  if (::g_forceMigrationFailure) {
    StrongRhythmMigrationResult failed{};
    failed.status = StrongRhythmMigrationStatus::InvalidContext;
    return failed;
  }
  return migrateStrongRhythmSynths(settings, context, drums, synthA, synthB);
}
}  // namespace GroovePuterRhythm

#define MINIACID_ENGINE_H
#define GROOVEPUTER_DSP_MODE_MANAGER_H
#define SceneManager TestSceneManager
#define migrateStrongRhythmMaterial i1MigrateStrongRhythmMaterial
#define migrateStrongRhythmSynths i1MigrateStrongRhythmSynths
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
#undef SceneManager

namespace {

using GroovePuterRhythm::QuantizedGenerationResult;

// The musician's swing. Every Atlas-backed recipe below was analysed at a
// different source swing, so any surviving Atlas write is immediately visible.
constexpr uint8_t kUserSwingPct = 50;
constexpr uint8_t kMinimalSpaceRecipe = 11;
constexpr uint8_t kMinimalSpaceAtlasSwing = 51;
constexpr uint8_t kDarkSkippyRecipe = 9;
constexpr uint8_t kDarkSkippyAtlasSwing = 68;
constexpr float kInitialBpm = 120.0f;

int g_failures = 0;

void expectSwing(const char* label, unsigned actual, unsigned expected) {
  if (actual == expected) {
    std::printf("%-58s OK   (%u%%)\n", label, actual);
    return;
  }
  std::fprintf(stderr, "%-58s FAIL expected %u%%, got %u%%\n", label, expected,
               actual);
  ++g_failures;
}

void expectTrue(const char* label, bool condition) {
  if (condition) {
    std::printf("%-58s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-58s FAIL\n", label);
  ++g_failures;
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

GenreSettings requestFor(GenerativeMode genre, uint8_t recipe) {
  GenreSettings requested{};
  requested.generativeMode = static_cast<uint8_t>(genre);
  requested.recipe = recipe;
  requested.regenerateOnApply = true;
  return requested;
}

uint8_t atlasSourceSwing(uint8_t runtimeRecipeId) {
  SynthPattern synthA{};
  SynthPattern synthB{};
  DrumPatternSet drums{};
  AtlasRuntimeMetadata metadata{};
  if (!AtlasRuntime::applyRecipe(runtimeRecipeId, 0, synthA, synthB, drums,
                                 &metadata)) {
    return 0;
  }
  return metadata.swingPercent;
}

void testFixtureRemainsContradictory() {
  expectSwing("fixture: Minimal Space Atlas source swing",
              atlasSourceSwing(kMinimalSpaceRecipe), kMinimalSpaceAtlasSwing);
  expectSwing("fixture: Dark Skippy Atlas source swing",
              atlasSourceSwing(kDarkSkippyRecipe), kDarkSkippyAtlasSwing);
  expectTrue("fixture: source swing differs from the user's swing",
             kMinimalSpaceAtlasSwing != kUserSwingPct &&
             kDarkSkippyAtlasSwing != kUserSwingPct);
}

void testStoppedCommitKeepsUserSwing() {
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(false);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);
  engine.sceneManager().currentScene().feel.swingPct = kUserSwingPct;

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requestFor(GenerativeMode::Reggae, kMinimalSpaceRecipe),
          GrooveboxMode::Minimal, false, 0.0f);

  expectTrue("STOPPED: generation commits immediately",
             result == QuantizedGenerationResult::CommittedNow);
  expectSwing("STOPPED: Atlas does not rewrite the user's swing",
              engine.sceneManager().currentScene().feel.swingPct, kUserSwingPct);
  expectSwing("STOPPED: Atlas source swing metadata is preserved",
              atlasSourceSwing(kMinimalSpaceRecipe), kMinimalSpaceAtlasSwing);
}

void testQuantizedActivationKeepsUserSwing() {
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(true);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);
  engine.sceneManager().currentScene().feel.swingPct = kUserSwingPct;

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requestFor(GenerativeMode::Broken, kDarkSkippyRecipe),
          GrooveboxMode::Minimal, false, 0.0f);

  expectTrue("PLAY: generation is published for the next bar",
             result == QuantizedGenerationResult::PendingNextBar);

  // Persistent COMMIT already happened on the control thread; the audible
  // overlay keeps the old swing until BAR_START. Both must read the user's
  // swing, not the swing the Atlas source pattern was analysed at.
  expectSwing("PLAY: persistent swing after COMMIT is the user's swing",
              engine.sceneManager().currentScene().feel.swingPct, kUserSwingPct);

  const int slot = g_publishedSlot.load(std::memory_order_acquire);
  expectTrue("PLAY: a candidate is published", slot >= 0 && slot < 2);
  if (slot >= 0 && slot < 2) {
    expectSwing("PLAY: audible activation snapshot keeps the user's swing",
                g_slots[slot].swingPct, kUserSwingPct);
  }

  expectTrue("PLAY: BAR_START activates the candidate",
             GroovePuterRhythm::commitQuantizedGenerationAtBarStart(
                 engine.sceneManager()));
  expectSwing("PLAY: committed swing is still the user's swing",
              engine.sceneManager().currentScene().feel.swingPct, kUserSwingPct);
  expectSwing("PLAY: Atlas source swing metadata is preserved",
              atlasSourceSwing(kDarkSkippyRecipe), kDarkSkippyAtlasSwing);
}

// A deliberately non-default user swing must survive just as well.
void testNonDefaultUserSwingSurvives() {
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(false);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);
  engine.sceneManager().currentScene().feel.swingPct = 62;

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requestFor(GenerativeMode::Reggae, kMinimalSpaceRecipe),
          GrooveboxMode::Minimal, false, 0.0f);

  expectTrue("CUSTOM SWING: generation commits",
             result == QuantizedGenerationResult::CommittedNow);
  expectSwing("CUSTOM SWING: the user's 62% survives Atlas materialization",
              engine.sceneManager().currentScene().feel.swingPct, 62);
}

}  // namespace

int main() {
  testFixtureRemainsContradictory();
  testStoppedCommitKeepsUserSwing();
  testQuantizedActivationKeepsUserSwing();
  testNonDefaultUserSwingSurvives();

  if (g_failures != 0) {
    std::fprintf(stderr, "GF2-I2 atlas swing ownership: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("GF2-I2 atlas swing ownership: PASS\n");
  return 0;
}
