#include "../scenes.h"
#include "../src/generation/migration/strong_rhythm_migration.h"
#include "../src/state/generation_request_state.h"

#include <atomic>
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
using GroovePuterRhythm::QuantizedGenerationResult;
using GroovePuterRhythm::StrongRhythmMigrationStatus;

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

GenreSettings technoRequest() {
  GenreSettings request{};
  request.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
  request.recipe = kBaseRecipeId;
  request.morphTarget = 0;
  request.morphAmount = 0;
  request.rhythmSelectionMode =
      static_cast<uint8_t>(GroovePuterRhythm::RhythmSelectionMode::Auto);
  request.rhythmArchetypeId = GroovePuterRhythm::kNoArchetypeId;
  return request;
}

int runRedContract() {
  resetHarnessState();
  MiniAcid engine;
  engine.setPlaying(false);
  const GenreSettings request = technoRequest();

  const QuantizedGenerationResult generation =
      GroovePuterRhythm::regenerateWithQuantizedCommit(
          engine, request, GrooveboxMode::Minimal, false, 100.0f);

  if (!g_capture.called) {
    std::fprintf(stderr,
                 "GF2-C2-V0R RED precondition failed: production migration was not invoked\n");
    return 2;
  }
  if (g_capture.result.status != StrongRhythmMigrationStatus::Applied) {
    std::fprintf(stderr,
                 "GF2-C2-V0R RED precondition failed: expected real migration Applied, got %u\n",
                 static_cast<unsigned>(g_capture.result.status));
    return 3;
  }
  if (generation != QuantizedGenerationResult::CommittedNow) {
    std::fprintf(stderr,
                 "GF2-C2-V0R RED precondition failed: expected CommittedNow, got %u\n",
                 static_cast<unsigned>(generation));
    return 4;
  }

  std::printf(
      "GF2-C2-V0R real path reached: migration=Applied generation=CommittedNow\n");
  std::fprintf(
      stderr,
      "GF2-C2-V0R RED: structured request/execution/result/provenance observation unavailable\n");
  return 1;
}
}  // namespace

int main() {
  return runRedContract();
}
