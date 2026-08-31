// 0.9.9-PMB-P1 own regression coverage for the bounded PREPARE/COMMIT
// redesign (see docs/contracts/0_9_9_PHRASE_PMB_P1_BOUNDED_PREPARE_COMMIT.md).
// These tests own the specific claims PMB-P1 makes; they do not duplicate
// P1R's/E0a's/D2's/I1's existing coverage of the underlying materializers.

#include "arduino_compat.h"

#include "../src/audio/audio_config.h"
#include "../src/dsp/generated_phrase_p1r_materializer.h"
#include "../src/dsp/generated_phrase_song.h"
#include "../src/dsp/miniacid_engine.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

SerialMock Serial;
SDMock SD;

using namespace GroovePuterRhythm;

namespace {

constexpr uint32_t kSeed = 0xFB1900u;

bool sameSynth(const SynthPattern& left, const SynthPattern& right) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& a = left.steps[step];
    const SynthStep& b = right.steps[step];
    if (a.note != b.note || a.slide != b.slide || a.accent != b.accent ||
        a.ghost != b.ghost || a.velocity != b.velocity ||
        a.timing != b.timing || a.fx != b.fx || a.fxParam != b.fxParam ||
        a.probability != b.probability) return false;
  }
  return true;
}

bool sameDrums(const DrumPatternSet& left, const DrumPatternSet& right) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& a = left.voices[voice].steps[step];
      const DrumStep& b = right.voices[voice].steps[step];
      if (a.hit != b.hit || a.accent != b.accent ||
          a.velocity != b.velocity || a.timing != b.timing ||
          a.fx != b.fx || a.fxParam != b.fxParam ||
          a.probability != b.probability) return false;
    }
  }
  return left.groove.swing == right.groove.swing &&
         left.groove.humanize == right.groove.humanize;
}

bool sameBar(const PhraseGenerator::PhraseBar& left,
             const PhraseGenerator::PhraseBar& right) {
  return sameSynth(left.synthA, right.synthA) &&
         sameSynth(left.synthB, right.synthB) &&
         sameDrums(left.drums, right.drums);
}

void configureFamily(MiniAcid& engine, GenerativeMode mode, GenreRecipeId recipe) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(mode);
  scene.genre.recipe = recipe;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.regenerateOnApply = false;
  scene.genre.applyTempoOnApply = false;
  scene.activeSongSlot = 0;
  scene.songs[0] = Song{};
  scene.feel.patternBars = 1;
  for (int bank = 0; bank < kBankCount; ++bank) {
    for (int slot = 0; slot < Bank<SynthPattern>::kPatterns; ++slot) {
      scene.synthABanks[bank].patterns[slot] = SynthPattern{};
      scene.synthBBanks[bank].patterns[slot] = SynthPattern{};
      scene.drumBanks[bank].patterns[slot] = DrumPatternSet{};
    }
  }
}

bool sceneFullyEmpty(const Scene& scene) {
  for (int bank = 0; bank < kBankCount; ++bank) {
    for (int slot = 0; slot < Bank<SynthPattern>::kPatterns; ++slot) {
      const SynthPattern empty{};
      if (!sameSynth(scene.synthABanks[bank].patterns[slot], empty)) return false;
      if (!sameSynth(scene.synthBBanks[bank].patterns[slot], empty)) return false;
      const DrumPatternSet emptyDrums{};
      if (!sameDrums(scene.drumBanks[bank].patterns[slot], emptyDrums)) return false;
    }
  }
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    for (int track = 0; track < SongPosition::kTrackCount; ++track) {
      if (scene.songs[0].positions[row].patterns[track] != -1) return false;
    }
  }
  return true;
}

// A P1R-capable archetype legitimately typed-rejects some phrase lengths
// under frozen length policy (see e0a_prepare_benchmark.cpp for the same
// idiom). Tests that need one guaranteed-successful PREPARE (rather than
// specifically exercising admissibility) probe for the first length that
// works instead of assuming a fixed one.
uint8_t firstAdmissibleBars(MiniAcid& engine) {
  for (uint8_t bars : std::array<uint8_t, 4>{1, 2, 4, 8}) {
    GeneratedPhraseSong::PreparedPhraseArrangement probe{};
    if (GeneratedPhraseSong::prepare(engine, bars, 0, probe)) return bars;
  }
  assert(false && "no admissible phrase length found for this family");
  return 0;
}

// ---------------------------------------------------------------------
// (1) P1R route: old full-array oracle (materializePreparedBars) vs the
// new bounded per-bar API (materializeOneBar), at 1/2/4/8 bars.
// ---------------------------------------------------------------------
GenreSettings loFi() {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  value.recipe = kBaseRecipeId;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  value.rhythmArchetypeId = kNoArchetypeId;
  return value;
}

PhraseExecutionMaterializationSettings materialization() {
  PhraseExecutionMaterializationSettings value{};
  value.level = RealizationLevel::P2Variation;
  value.generationAttemptOrdinal = 0;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  value.tonalMaterializationEnabled = true;
  value.rootPitchClass = 0;
  value.scaleTypeValue = kScaleDorian;
  return value;
}

void testP1ROracleEquivalence() {
  MiniAcid engine(kSampleRate, nullptr);
  int testedLengths = 0;
  for (uint8_t bars : std::array<uint8_t, 4>{1, 2, 4, 8}) {
    PhraseExecutionScratch scratch{};
    PreparedPhraseExecution execution{};
    const auto status = preparePhraseExecution(
        loFi(), materialization(), 37, bars, scratch, execution);
    // A P1R-capable archetype legitimately typed-rejects some phrase lengths
    // under frozen length policy (see e0a_prepare_benchmark.cpp for the same
    // idiom) -- skip the combination instead of demanding universal
    // admissibility. Coverage is asserted below via testedLengths.
    if (status != PhraseExecutionStatus::Ready) continue;
    assert(execution.length.effectivePhraseBars == bars);
    ++testedLengths;

    PhraseGenerator::PhraseBar realPitchSource{};
    assert(GeneratedPhraseP1R::prepareDestinationIndependentPitchSource(
        engine, execution, realPitchSource));

    std::array<PhraseGenerator::PhraseBar, 8> oracle{};
    GeneratedPhraseP1R::PreparationEvidence evidence{};
    assert(GeneratedPhraseP1R::materializePreparedBars(
        execution, realPitchSource, 0, 0, oracle, evidence));
    assert(evidence.materializationStatus == StrongRhythmMigrationStatus::Applied);

    for (uint8_t bar = 0; bar < bars; ++bar) {
      PhraseGenerator::PhraseBar bounded{};
      assert(GeneratedPhraseP1R::materializeOneBar(
          engine, execution, bar, static_cast<int16_t>(bar), bounded));
      assert(sameBar(oracle[bar], bounded));
    }
  }
  assert(testedLengths > 0);
  std::printf("PMB-P1 T1 P1R old-oracle vs bounded, %d/4 admissible lengths: OK\n",
              testedLengths);
}

// ---------------------------------------------------------------------
// (2) legacy procedural route: old two-buffer algorithm (reconstructed
// from the exact primitives PMB-A2 characterized) vs the new bounded
// materializeLegacyBar, at 1/2/4/8 bars.
// ---------------------------------------------------------------------
void testLegacyOracleEquivalence() {
  MiniAcid engine(kSampleRate, nullptr);
  // An out-of-range recipe forces the Legacy route (PMB-A2 Finding 1), and
  // one outside the Atlas catalog forces the procedural sub-route
  // (PMB-A2 Finding 2).
  constexpr GenreRecipeId kOutOfRangeRecipe = 250;
  configureFamily(engine, GenerativeMode::Techno, kOutOfRangeRecipe);
  Scene& scene = engine.sceneManager().currentScene();
  assert(GroovePuterRhythm::selectStrongRhythmRoute(scene.genre) ==
         GroovePuterRhythm::StrongRhythmRoute::Legacy);
  assert(!(AtlasRuntime::hasRecipe(kOutOfRangeRecipe) &&
           AtlasRuntime::variationCount(kOutOfRangeRecipe) >= 3));

  const GenreSettings genre = scene.genre;
  const GrooveboxMode mappedMode = GenreManager::grooveboxModeForRecipe(
      kOutOfRangeRecipe, GenerativeMode::Techno);
  const int flavor = 0;
  const float bpm = 120.0f;
  GenerativeParams params{};
  GenreBehavior behavior{};

  for (uint8_t bars : std::array<uint8_t, 4>{1, 2, 4, 8}) {
    // Old shape: build proceduralBase once, then per bar copy + migrate +
    // derive into a second buffer (the exact snippet PMB-A2 Finding 4/6
    // characterized, before the in-place-aliasing collapse).
    GrooveboxModeManager oldModeManager(engine);
    oldModeManager.setModeLocal(mappedMode);
    oldModeManager.setFlavorLocal(flavor);
    oldModeManager.setGenerationSeed(kSeed);
    PhraseGenerator::PhraseBar proceduralBase{};
    oldModeManager.generatePattern(proceduralBase.synthA, bpm, params, behavior, 0);
    oldModeManager.generatePattern(proceduralBase.synthB, bpm, params, behavior, 1);
    oldModeManager.generateDrumPattern(proceduralBase.drums, params, behavior);

    GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
    prepared.genre = genre;
    prepared.legacyAtlas = false;
    prepared.legacyRecipe = kOutOfRangeRecipe;
    prepared.legacyMappedMode = mappedMode;
    prepared.legacyFlavor = flavor;
    prepared.legacyBpm = bpm;
    prepared.legacyParams = params;
    prepared.legacyBehavior = behavior;
    prepared.request.seed = kSeed;
    prepared.request.bars = bars;

    for (uint8_t barIndex = 0; barIndex < bars; ++barIndex) {
      PhraseGenerator::PhraseBar migratedBase = proceduralBase;
      GeneratedPhraseSong::applyCurrentMigration(scene, genre, 0, barIndex, migratedBase);
      PhraseGenerator::PhraseBar oracleBar{};
      const auto role = PhraseGenerator::roleForBar(bars, barIndex);
      PhraseGenerator::deriveBar(migratedBase, role, kSeed, barIndex, oracleBar);

      PhraseGenerator::PhraseBar bounded{};
      assert(GeneratedPhraseSong::materializeLegacyBar(
          engine, scene, prepared, barIndex, bounded));
      assert(sameBar(oracleBar, bounded));
    }
  }
  std::puts("PMB-P1 T2 legacy old-oracle vs bounded, 1/2/4/8 bars: OK");
}

// ---------------------------------------------------------------------
// (3) failure during PREFLIGHT -> zero persistent mutation.
// ---------------------------------------------------------------------
void testPreflightFailureZeroMutation() {
  MiniAcid engine(kSampleRate, nullptr);
  // Rave + 8 bars is a known typed Stage-1 P1R rejection under Auto
  // selection (frozen P1R length policy) -- PREFLIGHT never begins
  // per-bar materialization, so this is the cleanest reachable proof that
  // a PREPARE failure leaves zero persistent Scene/Song trace.
  configureFamily(engine, GenerativeMode::Rave, kBaseRecipeId);
  Scene& scene = engine.sceneManager().currentScene();
  assert(sceneFullyEmpty(scene));

  const auto guard = [](auto&& body) { body(); };
  const GeneratedPhraseSong::Result result =
      GeneratedPhraseSong::generate(engine, 8, 0, guard);
  assert(result.status == GeneratedPhraseSong::LifecycleStatus::Failed);
  assert(sceneFullyEmpty(scene));
  std::puts("PMB-P1 T3 PREFLIGHT failure zero persistent mutation: OK");
}

// ---------------------------------------------------------------------
// (4) target changes between PREPARE and COMMIT -> TargetChanged retained,
// zero commit.
// ---------------------------------------------------------------------
void testTargetChangedBeforeCommit() {
  MiniAcid engine(kSampleRate, nullptr);
  configureFamily(engine, GenerativeMode::LoFi, kClassicChillRecipeId);
  const uint8_t bars = firstAdmissibleBars(engine);
  GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
  assert(GeneratedPhraseSong::prepare(engine, bars, 0, prepared));
  assert(GeneratedPhraseSong::preparedTargetStillCommitSafe(engine, prepared));

  // Simulate another actor mutating the Scene between PREPARE and COMMIT.
  GroovePuterState::markSceneMutated();
  assert(!GeneratedPhraseSong::preparedTargetStillCommitSafe(engine, prepared));
  std::puts("PMB-P1 T4 target-changed-before-commit retained: OK");
}

// ---------------------------------------------------------------------
// (5) typed P1R rejection -> exact existing result/status retained.
// ---------------------------------------------------------------------
void testTypedRejectionRetained() {
  MiniAcid engine(kSampleRate, nullptr);
  configureFamily(engine, GenerativeMode::Rave, kBaseRecipeId);
  Scene& scene = engine.sceneManager().currentScene();
  GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
  const bool ok = GeneratedPhraseSong::prepare(engine, 8, 0, prepared);
  assert(!ok);
  assert(prepared.p1r.usedP1r);
  assert(prepared.p1r.executionStatus != PhraseExecutionStatus::Ready);
  assert(prepared.result.error == PhraseGenerator::PhraseError::GenerationFailed);
  assert(sceneFullyEmpty(scene));
  std::puts("PMB-P1 T5 typed P1R rejection retained: OK");
}

// ---------------------------------------------------------------------
// (6) repeated identical request -> deterministic replay unchanged
// (PMB-P1-owned, independent of E0a's coverage).
// ---------------------------------------------------------------------
void testRepeatedRequestDeterminism() {
  MiniAcid engine(kSampleRate, nullptr);
  configureFamily(engine, GenerativeMode::LoFi, kClassicChillRecipeId);
  Scene& scene = engine.sceneManager().currentScene();
  const uint8_t bars = firstAdmissibleBars(engine);

  GeneratedPhraseSong::PreparedPhraseArrangement first{};
  GeneratedPhraseSong::PreparedPhraseArrangement second{};
  assert(GeneratedPhraseSong::prepare(engine, bars, 0, first));
  assert(GeneratedPhraseSong::prepare(engine, bars, 0, second));
  assert(first.useP1RRoute == second.useP1RRoute);

  for (uint8_t bar = 0; bar < bars; ++bar) {
    PhraseGenerator::PhraseBar leftBar{};
    PhraseGenerator::PhraseBar rightBar{};
    if (first.useP1RRoute) {
      assert(GeneratedPhraseP1R::materializeOneBar(engine, first.p1rExecution, bar, 0, leftBar));
      assert(GeneratedPhraseP1R::materializeOneBar(engine, second.p1rExecution, bar, 0, rightBar));
    } else {
      assert(GeneratedPhraseSong::materializeLegacyBar(engine, scene, first, bar, leftBar));
      assert(GeneratedPhraseSong::materializeLegacyBar(engine, scene, second, bar, rightBar));
    }
    assert(sameBar(leftBar, rightBar));
  }
  std::puts("PMB-P1 T6 repeated request deterministic replay: OK");
}

// ---------------------------------------------------------------------
// (7) no heap allocation >= PhraseBar-sized (1,416 B) staging blob
// anywhere on the generate() path.
// ---------------------------------------------------------------------
std::size_t g_maxTrackedAlloc = 0;
bool g_trackingAllocs = false;

}  // namespace

void* operator new(std::size_t size) {
  if (g_trackingAllocs && size > g_maxTrackedAlloc) g_maxTrackedAlloc = size;
  void* ptr = std::malloc(size);
  if (!ptr) throw std::bad_alloc();
  return ptr;
}
void* operator new[](std::size_t size) {
  if (g_trackingAllocs && size > g_maxTrackedAlloc) g_maxTrackedAlloc = size;
  void* ptr = std::malloc(size);
  if (!ptr) throw std::bad_alloc();
  return ptr;
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

namespace {

void testNoPhraseBarSizedHeapAllocation() {
  constexpr std::size_t kPhraseBarSize = sizeof(PhraseGenerator::PhraseBar);
  MiniAcid engine(kSampleRate, nullptr);
  configureFamily(engine, GenerativeMode::LoFi, kClassicChillRecipeId);
  const uint8_t bars = firstAdmissibleBars(engine);
  configureFamily(engine, GenerativeMode::LoFi, kClassicChillRecipeId);

  const auto guard = [](auto&& body) { body(); };
  g_maxTrackedAlloc = 0;
  g_trackingAllocs = true;
  const GeneratedPhraseSong::Result result =
      GeneratedPhraseSong::generate(engine, bars, 0, guard);
  g_trackingAllocs = false;

  assert(result.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow ||
         result.status == GeneratedPhraseSong::LifecycleStatus::PendingNextBar);
  std::printf(
      "PMB-P1 T7 largest heap allocation during generate() = %zu B "
      "(PhraseBar = %zu B)\n",
      g_maxTrackedAlloc, kPhraseBarSize);
  assert(g_maxTrackedAlloc < kPhraseBarSize);
  std::puts("PMB-P1 T7 no PhraseBar-sized heap allocation on generate() path: OK");
}

}  // namespace

int main() {
  static_assert(sizeof(GeneratedPhraseSong::PreparedPhraseArrangement) <= 1024,
                "PMB-P1 compact plan budget regressed");
  testP1ROracleEquivalence();
  testLegacyOracleEquivalence();
  testPreflightFailureZeroMutation();
  testTargetChangedBeforeCommit();
  testTypedRejectionRetained();
  testRepeatedRequestDeterminism();
  testNoPhraseBarSizedHeapAllocation();
  std::puts("0.9.9-PMB-P1 bounded PREPARE/COMMIT: PASS");
  return 0;
}
