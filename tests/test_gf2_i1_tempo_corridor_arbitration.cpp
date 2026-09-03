// GF2-I1 — tempo / generation corridor arbitration.
//
// The generation corridor of the selected genre/recipe is the production tempo
// owner for MATERIALIZE+BPM. Atlas recipe metadata describes the source corpus
// pattern and must stay provenance only: it may never become a second runtime
// tempo writer once the generation request has already resolved a tempo.
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

using GroovePuterRhythm::GenerationProfileView;
using GroovePuterRhythm::QuantizedGenerationResult;
using GroovePuterRhythm::generationProfileFor;

// Canonical GF2-I1 regression fixture. The declared corridor and the Atlas
// source BPM deliberately disagree; the disagreement is the fixture and must
// not be removed by editing either owner.
constexpr uint8_t kMinimalSpaceRecipe = 11;
constexpr uint16_t kMinimalSpaceAtlasBpm = 116;
constexpr uint16_t kMinimalSpaceSuggestedBpm = 86;
constexpr float kInitialBpm = 120.0f;

int g_failures = 0;

void expectBpm(const char* label, float actual, float expected) {
  if (actual == expected) {
    std::printf("%-56s OK   (%.1f)\n", label, actual);
    return;
  }
  std::fprintf(stderr, "%-56s FAIL expected %.1f, got %.1f\n",
               label, expected, actual);
  ++g_failures;
}

void expectTrue(const char* label, bool condition) {
  if (condition) {
    std::printf("%-56s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-56s FAIL\n", label);
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
  ::g_forceMigrationFailure = false;
  ::g_retargetDuringMigration = nullptr;
}

GenreSettings minimalSpaceRequest() {
  GenreSettings requested{};
  requested.generativeMode = static_cast<uint8_t>(GenerativeMode::Reggae);
  requested.recipe = kMinimalSpaceRecipe;
  requested.regenerateOnApply = true;
  requested.applyTempoOnApply = true;
  return requested;
}

// GenrePage resolves the request tempo exactly once, from the corridor of the
// requested profile, before generation begins.
float resolvedRequestBpm(const GenreSettings& requested, float engineBpm) {
  const GenerationProfileView profile = generationProfileFor(requested);
  if (profile.corridor.suggestedBpm > 0)
    return static_cast<float>(profile.corridor.suggestedBpm);
  return engineBpm;
}

uint16_t atlasSourceBpm(uint8_t runtimeRecipeId) {
  SynthPattern synthA{};
  SynthPattern synthB{};
  DrumPatternSet drums{};
  AtlasRuntimeMetadata metadata{};
  if (!AtlasRuntime::applyRecipe(runtimeRecipeId, 0, synthA, synthB, drums,
                                 &metadata)) {
    return 0;
  }
  return metadata.bpm;
}

// ---------------------------------------------------------------------------
// Fixture integrity: the two disagreeing owners stay exactly as published.
// ---------------------------------------------------------------------------

void testCanonicalFixtureRemainsContradictory() {
  const GenerationProfileView profile =
      generationProfileFor(minimalSpaceRequest());
  expectTrue("fixture: Minimal Space corridor is 72..102",
             profile.corridor.bpmMin == 72 && profile.corridor.bpmMax == 102);
  expectTrue("fixture: Minimal Space suggested BPM is 86",
             profile.corridor.suggestedBpm == kMinimalSpaceSuggestedBpm);
  expectTrue("fixture: Minimal Space Atlas source BPM is 116",
             atlasSourceBpm(kMinimalSpaceRecipe) == kMinimalSpaceAtlasBpm);
  expectTrue("fixture: Atlas source BPM lies outside the corridor",
             kMinimalSpaceAtlasBpm > profile.corridor.bpmMax);
}

// ---------------------------------------------------------------------------
// A. STOPPED / immediate commit
// ---------------------------------------------------------------------------

void testStoppedCommitKeepsResolvedCorridorTempo() {
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(false);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);

  const GenreSettings requested = minimalSpaceRequest();
  const float requestedBpm = resolvedRequestBpm(requested, engine.bpm());
  expectBpm("STOPPED: request resolves to corridor tempo",
            requestedBpm, static_cast<float>(kMinimalSpaceSuggestedBpm));

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requested, GrooveboxMode::Minimal, true, requestedBpm);

  expectTrue("STOPPED: generation commits immediately",
             result == QuantizedGenerationResult::CommittedNow);
  expectBpm("STOPPED: engine tempo is the resolved corridor tempo",
            engine.bpm(), static_cast<float>(kMinimalSpaceSuggestedBpm));
  expectBpm("STOPPED: persistent Scene tempo matches the request",
            engine.sceneManager().getBpm(),
            static_cast<float>(kMinimalSpaceSuggestedBpm));
  expectTrue("STOPPED: Atlas source metadata stays 116",
             atlasSourceBpm(kMinimalSpaceRecipe) == kMinimalSpaceAtlasBpm);
}

// ---------------------------------------------------------------------------
// B. PLAYING / quantized next-bar commit
// ---------------------------------------------------------------------------

void testQuantizedActivationKeepsResolvedCorridorTempo() {
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(true);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);

  const GenreSettings requested = minimalSpaceRequest();
  const float requestedBpm = resolvedRequestBpm(requested, engine.bpm());

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requested, GrooveboxMode::Minimal, true, requestedBpm);

  expectTrue("PLAY: generation is published for the next bar",
             result == QuantizedGenerationResult::PendingNextBar);
  expectBpm("PLAY: live tempo is untouched before BAR_START",
            engine.bpm(), kInitialBpm);

  const int slot = g_publishedSlot.load(std::memory_order_acquire);
  expectTrue("PLAY: a candidate is published", slot >= 0 && slot < 2);
  if (slot >= 0 && slot < 2) {
    expectBpm("PLAY: prepared candidate carries the resolved tempo",
              g_slots[slot].bpm, static_cast<float>(kMinimalSpaceSuggestedBpm));
  }

  const bool activated =
      GroovePuterRhythm::commitQuantizedGenerationAtBarStart(
          engine.sceneManager());
  expectTrue("PLAY: BAR_START activates the candidate", activated);
  expectBpm("PLAY: activated tempo is the resolved corridor tempo",
            engine.bpm(), static_cast<float>(kMinimalSpaceSuggestedBpm));
  expectTrue("PLAY: Atlas source metadata stays 116",
             atlasSourceBpm(kMinimalSpaceRecipe) == kMinimalSpaceAtlasBpm);
}

// ---------------------------------------------------------------------------
// Negative contracts — I1 must not extend tempo application.
// ---------------------------------------------------------------------------

void testMaterializeWithoutTempoKeepsEngineBpm() {
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(false);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, minimalSpaceRequest(), GrooveboxMode::Minimal, false, 0.0f);

  expectTrue("MATERIALIZE: generation commits",
             result == QuantizedGenerationResult::CommittedNow);
  expectBpm("MATERIALIZE without BPM keeps the manual tempo",
            engine.bpm(), kInitialBpm);
  expectBpm("MATERIALIZE without BPM keeps the persistent tempo",
            engine.sceneManager().getBpm(), kInitialBpm);
}

void testMaterializeWithoutTempoKeepsEngineBpmWhilePlaying() {
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(true);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, minimalSpaceRequest(), GrooveboxMode::Minimal, false, 0.0f);
  expectTrue("MATERIALIZE (PLAY): candidate is published",
             result == QuantizedGenerationResult::PendingNextBar);
  GroovePuterRhythm::commitQuantizedGenerationAtBarStart(engine.sceneManager());
  expectBpm("MATERIALIZE (PLAY) without BPM keeps the manual tempo",
            engine.bpm(), kInitialBpm);
}

void testFailedGenerationDoesNotMutateTempo() {
  resetQuantizedState();
  ::g_forceMigrationFailure = true;

  MiniAcid engine;
  engine.setPlaying(false);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);

  const GenreSettings requested = minimalSpaceRequest();
  const float requestedBpm = resolvedRequestBpm(requested, engine.bpm());
  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requested, GrooveboxMode::Minimal, true, requestedBpm);

  expectTrue("FAILED: generation reports failure",
             result == QuantizedGenerationResult::Failed);
  expectBpm("FAILED generation leaves the live tempo untouched",
            engine.bpm(), kInitialBpm);
  expectBpm("FAILED generation leaves the persistent tempo untouched",
            engine.sceneManager().getBpm(), kInitialBpm);
}

void testBusyRequestDoesNotMutateTempo() {
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(true);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);

  // Hold both quantized slots so the next request cannot acquire a lease.
  const WriteLease held = acquireWriteLease();
  const int companion = acquireCompanionActivationSlot(held.slot);
  armActivationSlot(companion);

  const GenreSettings requested = minimalSpaceRequest();
  const float requestedBpm = resolvedRequestBpm(requested, engine.bpm());
  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requested, GrooveboxMode::Minimal, true, requestedBpm);

  expectTrue("BUSY: request is rejected",
             result == QuantizedGenerationResult::Failed);
  expectBpm("BUSY request leaves the live tempo untouched",
            engine.bpm(), kInitialBpm);
  abortArmedActivation(companion, GroovePuterRhythm::QuantizedGenerationStatus::CancelledExplicit);
  releaseWriteSlot(held.slot);
}

void testTargetChangedDoesNotMutateTempo() {
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(true);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);
  ::g_retargetDuringMigration = &engine.sceneManager();

  const GenreSettings requested = minimalSpaceRequest();
  const float requestedBpm = resolvedRequestBpm(requested, engine.bpm());
  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, requested, GrooveboxMode::Minimal, true, requestedBpm);

  expectTrue("TARGET CHANGED: candidate is discarded",
             result == QuantizedGenerationResult::Failed);
  expectBpm("TARGET CHANGED leaves the live tempo untouched",
            engine.bpm(), kInitialBpm);
  expectBpm("TARGET CHANGED leaves the persistent tempo untouched",
            engine.sceneManager().getBpm(), kInitialBpm);
}

void testSynthRerollDoesNotMutateTempo() {
  resetQuantizedState();

  MiniAcid engine;
  engine.setPlaying(false);
  engine.setBpm(kInitialBpm);
  engine.sceneManager().setBpm(kInitialBpm);
  engine.sceneManager().currentScene().genre = minimalSpaceRequest();

  const QuantizedGenerationResult result =
      GroovePuterRhythm::regenerateSynthWithQuantizedCommit(engine, 0);
  expectTrue("SYNTH REROLL: reroll commits",
             result == QuantizedGenerationResult::CommittedNow);
  expectBpm("SYNTH REROLL keeps the live tempo",
            engine.bpm(), kInitialBpm);
  expectBpm("SYNTH REROLL keeps the persistent tempo",
            engine.sceneManager().getBpm(), kInitialBpm);
}

// ---------------------------------------------------------------------------
// Corridor validity over every production generation profile.
// ---------------------------------------------------------------------------

void testEveryProductionCorridorIsWellFormed() {
  using GroovePuterRhythm::isValidGenerationProfile;
  bool ok = true;
  int profiles = 0;
  for (int modeIndex = 0; modeIndex < kGenerativeModeCount; ++modeIndex) {
    const auto genre = static_cast<GenerativeMode>(modeIndex);
    const uint8_t count = GroovePuterRhythm::availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < count; ++ordinal) {
      GenreRecipeId recipe = 0;
      if (!GroovePuterRhythm::availableRecipeAt(genre, ordinal, recipe)) {
        ok = false;
        continue;
      }
      GenreSettings settings{};
      settings.generativeMode = static_cast<uint8_t>(modeIndex);
      settings.recipe = static_cast<uint8_t>(recipe);
      const GenerationProfileView profile = generationProfileFor(settings);
      const auto& corridor = profile.corridor;
      ++profiles;
      if (!isValidGenerationProfile(profile) || corridor.bpmMin == 0 ||
          corridor.bpmMax < corridor.bpmMin ||
          corridor.suggestedBpm < corridor.bpmMin ||
          corridor.suggestedBpm > corridor.bpmMax) {
        std::fprintf(stderr,
                     "corridor invalid: genre=%d recipe=%u %u..%u suggested=%u\n",
                     modeIndex, static_cast<unsigned>(recipe),
                     static_cast<unsigned>(corridor.bpmMin),
                     static_cast<unsigned>(corridor.bpmMax),
                     static_cast<unsigned>(corridor.suggestedBpm));
        ok = false;
      }
    }
  }
  expectTrue("every production corridor is well formed", ok && profiles > 0);
  std::printf("  corridors audited: %d\n", profiles);
}

// ---------------------------------------------------------------------------
// Provenance table: Atlas source BPM stays readable and stays out of the
// production tempo decision for every Atlas-backed recipe.
// ---------------------------------------------------------------------------

void testAtlasProvenanceRemainsSeparateFromCorridor() {
  struct AtlasFixture {
    GenerativeMode genre;
    uint8_t recipe;
    uint16_t atlasBpm;
    uint16_t suggestedBpm;
  };
  constexpr AtlasFixture kFixtures[] = {
      {GenerativeMode::Acid, 6, 124, 124},
      {GenerativeMode::Acid, 7, 128, 136},
      {GenerativeMode::Broken, 8, 134, 132},
      {GenerativeMode::Broken, 9, 136, 134},
      {GenerativeMode::Reggae, 10, 120, 116},
      {GenerativeMode::Reggae, 11, kMinimalSpaceAtlasBpm, kMinimalSpaceSuggestedBpm},
  };

  bool ok = true;
  for (const AtlasFixture& fixture : kFixtures) {
    GenreSettings settings{};
    settings.generativeMode = static_cast<uint8_t>(fixture.genre);
    settings.recipe = fixture.recipe;
    const GenerationProfileView profile = generationProfileFor(settings);
    const uint16_t source = atlasSourceBpm(fixture.recipe);
    if (source != fixture.atlasBpm ||
        profile.corridor.suggestedBpm != fixture.suggestedBpm) {
      std::fprintf(stderr,
                   "atlas provenance drift: recipe=%u atlas=%u (expected %u) "
                   "suggested=%u (expected %u)\n",
                   static_cast<unsigned>(fixture.recipe),
                   static_cast<unsigned>(source),
                   static_cast<unsigned>(fixture.atlasBpm),
                   static_cast<unsigned>(profile.corridor.suggestedBpm),
                   static_cast<unsigned>(fixture.suggestedBpm));
      ok = false;
    }
  }
  expectTrue("Atlas provenance and corridor policy stay distinct", ok);
}

// Each Atlas-backed recipe must materialize at its own corridor tempo, not at
// the corpus tempo of its source pattern.
void testEveryAtlasRecipeMaterializesAtItsCorridorTempo() {
  struct AtlasFixture {
    GenerativeMode genre;
    uint8_t recipe;
    float suggestedBpm;
  };
  constexpr AtlasFixture kFixtures[] = {
      {GenerativeMode::Acid, 6, 124.0f},
      {GenerativeMode::Acid, 7, 136.0f},
      {GenerativeMode::Broken, 8, 132.0f},
      {GenerativeMode::Broken, 9, 134.0f},
      {GenerativeMode::Reggae, 10, 116.0f},
      {GenerativeMode::Reggae, 11, 86.0f},
  };

  for (const AtlasFixture& fixture : kFixtures) {
    resetQuantizedState();
    MiniAcid engine;
    engine.setPlaying(false);
    engine.setBpm(kInitialBpm);
    engine.sceneManager().setBpm(kInitialBpm);

    GenreSettings requested{};
    requested.generativeMode = static_cast<uint8_t>(fixture.genre);
    requested.recipe = fixture.recipe;
    requested.regenerateOnApply = true;
    requested.applyTempoOnApply = true;
    const float requestedBpm = resolvedRequestBpm(requested, engine.bpm());

    const QuantizedGenerationResult result =
        GroovePuterRhythm::regenerateWithQuantizedCommit(
            engine, requested, GrooveboxMode::Minimal, true, requestedBpm);
    if (result != QuantizedGenerationResult::CommittedNow) {
      std::fprintf(stderr, "recipe %u: generation did not commit\n",
                   static_cast<unsigned>(fixture.recipe));
      ++g_failures;
      continue;
    }
    if (engine.bpm() != fixture.suggestedBpm) {
      std::fprintf(stderr,
                   "recipe %u: committed BPM %.1f, expected corridor %.1f\n",
                   static_cast<unsigned>(fixture.recipe), engine.bpm(),
                   fixture.suggestedBpm);
      ++g_failures;
    }
  }
  std::printf("%-56s %s\n", "every Atlas recipe commits its corridor tempo",
              g_failures == 0 ? "OK" : "see failures above");
}

}  // namespace

int main() {
  testCanonicalFixtureRemainsContradictory();
  testStoppedCommitKeepsResolvedCorridorTempo();
  testQuantizedActivationKeepsResolvedCorridorTempo();
  testMaterializeWithoutTempoKeepsEngineBpm();
  testMaterializeWithoutTempoKeepsEngineBpmWhilePlaying();
  testFailedGenerationDoesNotMutateTempo();
  testBusyRequestDoesNotMutateTempo();
  testTargetChangedDoesNotMutateTempo();
  testSynthRerollDoesNotMutateTempo();
  testEveryProductionCorridorIsWellFormed();
  testAtlasProvenanceRemainsSeparateFromCorridor();
  testEveryAtlasRecipeMaterializesAtItsCorridorTempo();

  if (g_failures != 0) {
    std::fprintf(stderr, "GF2-I1 tempo arbitration: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("GF2-I1 tempo corridor arbitration: PASS\n");
  return 0;
}
