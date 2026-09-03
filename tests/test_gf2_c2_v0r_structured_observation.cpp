#include "../scenes.h"
#include "../src/generation/migration/strong_rhythm_migration.h"
#include "../src/state/generation_request_state.h"
#include "support/gf2_generation_observation.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

class TestSceneManager {
 public:
  int currentPageIndex() const { return 0; }
  int getCurrentBankIndex(int) const { return 0; }
  int getCurrentSynthPatternIndex(int) const { return 0; }
  int getCurrentDrumPatternIndex() const { return 0; }

  Scene& currentScene() { return scene_; }
  const Scene& currentScene() const { return scene_; }

 private:
  Scene scene_{};
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

namespace {
struct MigrationCapture {
  bool called = false;
  GenreSettings request{};
  GroovePuterRhythm::StrongRhythmMigrationContext context{};
  GroovePuterRhythm::StrongRhythmMigrationResult result{};
};

MigrationCapture g_capture{};
}  // namespace

namespace GroovePuterRhythm {
inline StrongRhythmRoute c2SelectStrongRhythmRoute(const GenreSettings& settings) {
  return selectStrongRhythmRoute(settings);
}

inline StrongRhythmMigrationResult c2MigrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      settings, context, drums, synthA, synthB);
  ::g_capture.called = true;
  ::g_capture.request = settings;
  ::g_capture.context = context;
  ::g_capture.result = result;
  return result;
}

inline StrongRhythmMigrationResult c2MigrateStrongRhythmSynths(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB) {
  return migrateStrongRhythmSynths(settings, context, drums, synthA, synthB);
}
}  // namespace GroovePuterRhythm

#define GROOVEPUTER_DSP_ATLAS_RUNTIME_H
#define MINIACID_ENGINE_H
#define GROOVEPUTER_DSP_MODE_MANAGER_H
#define SceneManager TestSceneManager
#define selectStrongRhythmRoute c2SelectStrongRhythmRoute
#define migrateStrongRhythmMaterial c2MigrateStrongRhythmMaterial
#define migrateStrongRhythmSynths c2MigrateStrongRhythmSynths
#include "../src/generation/migration/quantized_generation_commit_impl.h"
#undef migrateStrongRhythmSynths
#undef migrateStrongRhythmMaterial
#undef selectStrongRhythmRoute
#undef SceneManager

namespace {
using GroovePuterRhythm::GF2Measurement::GenerationObservation;
using GroovePuterRhythm::GF2Measurement::MaterialProvenance;
using GroovePuterRhythm::GF2Measurement::equivalent;
using GroovePuterRhythm::GF2Measurement::materialFingerprint;
using GroovePuterRhythm::GF2Measurement::observeGeneration;
using GroovePuterRhythm::GF2Measurement::toJson;
using GroovePuterRhythm::QuantizedGenerationResult;
using GroovePuterRhythm::StrongRhythmMigrationStatus;

struct CaseResult {
  QuantizedGenerationResult generation = QuantizedGenerationResult::Failed;
  GenerationObservation observation{};
};

void resetHarnessState() {
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;
  g_capture = {};
  g_publishedSlot.store(-1, std::memory_order_release);
  for (int slot = 0; slot < 2; ++slot) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    g_slots[slot] = PendingGeneration{};
  }
  g_status.store(
      static_cast<uint8_t>(GroovePuterRhythm::QuantizedGenerationStatus::Idle),
      std::memory_order_release);
  g_commitSerial.store(0, std::memory_order_release);
  GroovePuterState::resetGenerationAttemptState();
}

GenreSettings requestFor(GenerativeMode mode) {
  GenreSettings request{};
  request.generativeMode = static_cast<uint8_t>(mode);
  request.recipe = kBaseRecipeId;
  request.morphTarget = 0;
  request.morphAmount = 0;
  request.rhythmSelectionMode =
      static_cast<uint8_t>(GroovePuterRhythm::RhythmSelectionMode::Auto);
  request.rhythmArchetypeId = GroovePuterRhythm::kNoArchetypeId;
  return request;
}

GenreSettings unsupportedRequest() {
  GenreSettings request = requestFor(GenerativeMode::Techno);
  request.generativeMode = static_cast<uint8_t>(kGenerativeModeCount);
  return request;
}

uint32_t liveMaterialFingerprint(const MiniAcid& engine) {
  const Scene& scene = engine.sceneManager().currentScene();
  return materialFingerprint(
      scene.drumBanks[0].patterns[0],
      scene.synthABanks[0].patterns[0],
      scene.synthBBanks[0].patterns[0]);
}

bool generationAccepted(QuantizedGenerationResult result) {
  return result == QuantizedGenerationResult::CommittedNow ||
         result == QuantizedGenerationResult::PendingNextBar;
}

CaseResult runCase(bool playing, const GenreSettings& request) {
  resetHarnessState();
  MiniAcid engine;
  engine.setPlaying(playing);
  const uint32_t before = liveMaterialFingerprint(engine);
  const QuantizedGenerationResult generation =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, request, GrooveboxMode::Minimal, false, 100.0f);
  const uint32_t after = liveMaterialFingerprint(engine);

  if (!g_capture.called) {
    std::fprintf(stderr,
                 "GF2-C2-V0R: production migration was not invoked\n");
    return {};
  }

  CaseResult result{};
  result.generation = generation;
  result.observation = observeGeneration(
      g_capture.request,
      g_capture.context,
      g_capture.result,
      static_cast<uint8_t>(generation),
      generationAccepted(generation),
      before,
      after);
  return result;
}

bool require(bool condition, const char* message) {
  if (condition) return true;
  std::fprintf(stderr, "GF2-C2-V0R FAIL: %s\n", message);
  return false;
}

bool resultIdentityDiffers(const GenerationObservation& left,
                           const GenerationObservation& right) {
  return left.migrationRoute != right.migrationRoute ||
         left.migrationArchetype != right.migrationArchetype ||
         left.bassRhythmId != right.bassRhythmId ||
         left.chordRhythmId != right.chordRhythmId ||
         left.progressionId != right.progressionId ||
         left.melodicRhythmId != right.melodicRhythmId ||
         left.motifShapeId != right.motifShapeId ||
         left.synthBRole != right.synthBRole ||
         left.phraseLaw != right.phraseLaw ||
         left.effectiveMaterialFingerprint != right.effectiveMaterialFingerprint;
}

int runContract() {
  bool ok = true;

  // Case A: same production request/context/state in two fresh runs.
  const CaseResult a1 = runCase(false, requestFor(GenerativeMode::Techno));
  const CaseResult a2 = runCase(false, requestFor(GenerativeMode::Techno));
  ok &= require(
      a1.generation == QuantizedGenerationResult::CommittedNow &&
          a2.generation == QuantizedGenerationResult::CommittedNow,
      "Case A must reach successful quantized execution twice");
  ok &= require(equivalent(a1.observation, a2.observation),
                "Case A observations must be equivalent");
  std::puts(toJson("A", a1.observation).c_str());
  if (ok) std::puts("GF2-C2-V0R CASE A deterministic observation: PASS");

  // Case B: real migration Applied and the requested result becomes effective.
  const CaseResult b = a1;
  const bool caseB =
      b.observation.migrationStatus ==
          static_cast<uint8_t>(StrongRhythmMigrationStatus::Applied) &&
      b.generation == QuantizedGenerationResult::CommittedNow &&
      b.observation.generationAccepted &&
      b.observation.requestedResultEffective &&
      b.observation.provenance == MaterialProvenance::RequestedOperationAccepted &&
      b.observation.effectiveMaterialFingerprint !=
          b.observation.previousMaterialFingerprint;
  ok &= require(caseB,
                "Case B must attribute effective material to an Applied request");
  std::puts(toJson("B", b.observation).c_str());
  if (caseB) std::puts("GF2-C2-V0R CASE B successful migration observation: PASS");

  // Case C: the same production owner sees a real non-Applied migration.
  // PLAY is intentional: failed candidate preparation must leave live material
  // untouched, making retained previous material directly observable.
  const CaseResult c = runCase(true, unsupportedRequest());
  const bool caseC =
      c.observation.migrationStatus !=
          static_cast<uint8_t>(StrongRhythmMigrationStatus::Applied) &&
      c.generation == QuantizedGenerationResult::Failed &&
      !c.observation.generationAccepted &&
      !c.observation.requestedResultEffective &&
      c.observation.provenance == MaterialProvenance::PreviousMaterialRetained &&
      c.observation.effectiveMaterialFingerprint ==
          c.observation.previousMaterialFingerprint;
  ok &= require(caseC,
                "Case C must expose retained previous material after non-Applied migration");
  std::puts(toJson("C", c.observation).c_str());
  if (caseC) std::puts("GF2-C2-V0R CASE C failed migration provenance: PASS");

  // Case D: changing a semantic request must change execution/result evidence,
  // not merely echo a different request field.
  const CaseResult d = runCase(false, requestFor(GenerativeMode::Acid));
  const bool caseD =
      d.observation.requestedMode != b.observation.requestedMode &&
      d.observation.migrationStatus ==
          static_cast<uint8_t>(StrongRhythmMigrationStatus::Applied) &&
      d.generation == QuantizedGenerationResult::CommittedNow &&
      resultIdentityDiffers(b.observation, d.observation);
  ok &= require(caseD,
                "Case D semantic input change must alter execution/result evidence");
  std::puts(toJson("D", d.observation).c_str());
  if (caseD) std::puts("GF2-C2-V0R CASE D meaningful input sensitivity: PASS");

  return ok ? 0 : 1;
}
}  // namespace

int main() {
  return runContract();
}
