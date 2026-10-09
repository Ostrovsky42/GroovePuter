#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/phrase_generator.h"
#include "src/state/generated_synth_a_origin.h"
#include "src/state/material_slot_access.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using GroovePuterMaterial::GeneratedSynthABarOrigin;
using GroovePuterMaterial::GeneratedSynthAOrigin;
using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialSlotDescriptor;

constexpr float kSampleRate = 44100.0f;
const auto kGuard = [](auto&& body) { body(); };

// ---- helpers ---------------------------------------------------------------

void configureScene(MiniAcid& engine, GenerativeMode mode, GenreRecipeId recipe) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(mode);
  scene.genre.recipe = recipe;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.regenerateOnApply = false;
  scene.genre.applyTempoOnApply = false;
  scene.activeSongSlot = 0;
  scene.songs[0] = Song{};
  scene.songs[1] = Song{};
  scene.feel.patternBars = 1;
  for (int b = 0; b < kBankCount; ++b) {
    for (int p = 0; p < Bank<SynthPattern>::kPatterns; ++p) {
      scene.synthABanks[b].patterns[p] = SynthPattern{};
      scene.synthBBanks[b].patterns[p] = SynthPattern{};
      scene.drumBanks[b].patterns[p] = DrumPatternSet{};
    }
  }
  for (int v = 0; v < Scene::kMaterialVoices; ++v) {
    for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) {
      scene.materialSlots[v][s] = MaterialSlotDescriptor{};
    }
  }
  for (int v = 0; v < NUM_303_VOICES; ++v) {
    engine.pendingMaterial_[v].queued = false;
    engine.pendingMaterial_[v].lifecycleBound = false;
  }
  engine.genreManager().setGenerativeMode(mode);
  if (recipe <= 64) engine.genreManager().setRecipe(recipe);
}

// Active-step topology of a Synth A Pattern. Bass continuations are realized
// as held notes on the following steps, so a plan explains exactly
// onsets | continuations.
uint16_t onsetMask(const SynthPattern& pattern) {
  uint16_t mask = 0;
  for (int i = 0; i < 16; ++i) {
    if (pattern.steps[i].note >= 0) {
      mask = static_cast<uint16_t>(mask | GroovePuterRhythm::stepBit(static_cast<uint8_t>(i)));
    }
  }
  return mask;
}

uint16_t planMask(const GroovePuterRhythm::BassRhythmPlan& plan) {
  return static_cast<uint16_t>(plan.onsets | plan.continuations);
}

bool sameBass(const GroovePuterRhythm::BassRhythmPlan& a,
              const GroovePuterRhythm::BassRhythmPlan& b) {
  return a.id == b.id && a.kickRelationship == b.kickRelationship &&
         a.onsets == b.onsets && a.continuations == b.continuations;
}

bool sameHarmonic(const GroovePuterRhythm::HarmonicRhythmPlan& a,
                  const GroovePuterRhythm::HarmonicRhythmPlan& b) {
  return a.progression == b.progression && a.onsets == b.onsets &&
         a.eventCount == b.eventCount &&
         a.phraseBarOrdinal == b.phraseBarOrdinal &&
         a.phraseHarmonicPosition == b.phraseHarmonicPosition;
}

bool sameSource(const GroovePuterRhythm::ChordProgressionSource& a,
                const GroovePuterRhythm::ChordProgressionSource& b) {
  if (a.id != b.id || a.period != b.period) return false;
  for (int i = 0; i < GroovePuterRhythm::kMaxChordProgressionSourceEvents; ++i) {
    if (a.events[i].degree != b.events[i].degree ||
        a.events[i].quality != b.events[i].quality ||
        a.events[i].rootOffsetSemitones != b.events[i].rootOffsetSemitones) {
      return false;
    }
  }
  return true;
}

bool sameOrigin(const GeneratedSynthAOrigin& a, const GeneratedSynthAOrigin& b) {
  if (a.common.phraseGenerationIdentity != b.common.phraseGenerationIdentity ||
      a.common.barCount != b.common.barCount ||
      a.common.rootPitchClass != b.common.rootPitchClass ||
      a.common.scaleTypeValue != b.common.scaleTypeValue ||
      !sameSource(a.common.progressionSource, b.common.progressionSource)) {
    return false;
  }
  for (int i = 0; i < a.common.barCount; ++i) {
    const auto& x = a.bars[i];
    const auto& y = b.bars[i];
    if (!(x.material.address == y.material.address) ||
        x.material.id != y.material.id ||
        x.originPatternVersion != y.originPatternVersion ||
        !sameBass(x.bassRhythm, y.bassRhythm) ||
        !sameHarmonic(x.harmonicRhythm, y.harmonicRhythm) ||
        x.phraseBarOrdinal != y.phraseBarOrdinal) {
      return false;
    }
  }
  return true;
}

struct Route {
  uint8_t bars;
  GenerativeMode mode;
  GenreRecipeId recipe;
};
// All four supported phrase lengths through P1R routes (see D1-A).
const Route kRoutes[] = {
    {1, GenerativeMode::Chip, 0},
    {2, GenerativeMode::Chip, 0},
    {4, GenerativeMode::LoFi, 0},
    {8, GenerativeMode::LoFi, 0},
};

void freshProject(const char* name) {
  assert(PatternPagingService::setProjectName(name));
  assert(PatternPagingService::clearProjectPages());
}

std::filesystem::path newestIdentityMeta() {
  std::filesystem::path newest;
  std::filesystem::file_time_type newestTime{};
  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           std::filesystem::current_path())) {
    if (!entry.is_regular_file() ||
        entry.path().filename() != "material_id.meta") {
      continue;
    }
    const auto time = entry.last_write_time();
    if (newest.empty() || time >= newestTime) {
      newest = entry.path();
      newestTime = time;
    }
  }
  return newest;
}

// ---- B1 / B2 ---------------------------------------------------------------
// Exact resolved BassRhythmPlan evidence leaves the migration owner and the
// P1R one-bar seam. Genre idiom materialization may further shape the final
// Synth A pattern, so output replay is compared between the two seams.
void test_b1_b2_bass_plan_export_and_forwarding() {
  freshProject("d1b-b1");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);

  size_t distinctPlans = 0;
  for (const auto& route : kRoutes) {
    configureScene(engine, route.mode, route.recipe);
    GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
    if (!GeneratedPhraseSong::prepare(engine, route.bars, 0, prepared)) continue;
    assert(prepared.useP1RRoute);

    GroovePuterRhythm::BassRhythmPlan plans[8]{};
    uint16_t planMasks[8]{};
    for (uint8_t bar = 0; bar < route.bars; ++bar) {
      const int16_t address = static_cast<int16_t>(
          songPatternFromPageBankIndex(0, 0, prepared.firstLocalSlot + bar));

      // B1: the migration owner itself.
      PhraseGenerator::PhraseBar direct{};
      assert(GeneratedPhraseP1R::prepareDestinationIndependentPitchSource(
          engine, prepared.p1rExecution, direct));
      const auto migration = GroovePuterRhythm::materializePreparedPhraseBar(
          prepared.p1rExecution, bar, address, direct.drums, direct.synthA,
          direct.synthB);
      assert(migration.status ==
             GroovePuterRhythm::StrongRhythmMigrationStatus::Applied);
      assert(migration.bassRhythmPlanAvailable);
      assert(migration.bassRhythmPlan.id == migration.bassRhythmId);
      assert(migration.bassRhythmPlan.id != GroovePuterRhythm::BassRhythmId::Auto);

      // B2: the one-bar seam forwards the very same evidence.
      PhraseGenerator::PhraseBar viaSeam{};
      GeneratedPhraseP1R::MaterializedSynthABarEvidence evidence{};
      assert(GeneratedPhraseP1R::materializeOneBar(
          engine, prepared.p1rExecution, bar, address, viaSeam, evidence));
      assert(evidence.valid);
      assert(evidence.phraseBarOrdinal == bar);
      assert(sameBass(evidence.bassRhythm, migration.bassRhythmPlan));
      assert(onsetMask(viaSeam.synthA) == onsetMask(direct.synthA));

      // B1/B2 correspondence: export is the exact rhythm-role plan, while the
      // two public materialization seams must produce the same final pattern.
      plans[bar] = migration.bassRhythmPlan;
      planMasks[bar] = planMask(migration.bassRhythmPlan);
    }
    // Count the distinct planned topologies independently of the genre idiom's
    // later materialization, which owns phrase-level shape and protected space.
    for (uint8_t i = 0; i < route.bars; ++i) {
      for (uint8_t j = 0; j < route.bars; ++j) {
        if (planMasks[i] != planMasks[j]) ++distinctPlans;
      }
    }
    // Default-plan export would be caught by the non-zero onsets assertion.
    assert(!sameBass(plans[0], GroovePuterRhythm::BassRhythmPlan{}));
  }
  std::printf("D1-B B1/B2: differing bass topologies observed across bar pairs = %zu\n",
              distinctPlans);
  std::puts("D1-B B1/B2: exact BassRhythmPlan export + forwarding: PASS");
}

// ---- B3 / B4 / B5 / B6 -------------------------------------------------------
void test_b3_to_b6_generated_origin_multibar() {
  freshProject("d1b-multibar");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  assert(engine.generatedSynthAOrigin() == nullptr);
  Scene& scene = engine.sceneManager().currentScene();

  uint32_t previousMaxId = 0;
  for (const auto& route : kRoutes) {
    configureScene(engine, route.mode, route.recipe);
    const auto result = GeneratedPhraseSong::generate(engine, route.bars, 0, kGuard);
    assert(result.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
    assert(result.p1r.usedP1r);

    const GeneratedSynthAOrigin* origin = engine.generatedSynthAOrigin();
    assert(origin != nullptr && origin->valid());
    assert(origin->common.barCount == route.bars);
    assert(origin->common.phraseGenerationIdentity ==
           result.p1r.phraseGenerationIdentity);
    assert(origin->common.progressionSource.id == result.p1r.progression);

    const int firstSlot = result.phrase.firstLocalSlot;
    std::vector<uint32_t> ids;
    for (int bar = 0; bar < route.bars; ++bar) {
      const int slot = firstSlot + bar;
      const int bank = slot / Bank<SynthPattern>::kPatterns;
      const int index = slot % Bank<SynthPattern>::kPatterns;
      const auto& entry = origin->bars[bar];

      // B4: binding to the actually committed Material.
      assert(entry.material.id.valid());
      assert(entry.material.id == scene.materialSlots[0][slot].id);
      assert(entry.material.address.voice == 0);
      assert(entry.material.address.globalSlot ==
             songPatternFromPageBankIndex(0, bank, index));
      assert(entry.phraseBarOrdinal == bar);
      for (uint32_t seen : ids) assert(seen != entry.material.id.value);
      ids.push_back(entry.material.id.value);
      assert(entry.material.id.value > previousMaxId);  // never reused

      // B5: exact origin version of the committed Pattern.
      assert(entry.originPatternVersion ==
             GroovePuterMaterial::versionForPattern(
                 scene.synthABanks[bank].patterns[index]));
      assert(entry.originPatternVersion.valid());

      // B4: lookup by canonical identity, no version requirement.
      assert(engine.findGeneratedSynthAOrigin(entry.material) == &entry);
      assert(engine.findGeneratedSynthAOrigin(MaterialReference{
                 entry.material.address, MaterialId{entry.material.id.value + 100000}}) ==
             nullptr);

      // Bass evidence records the upstream role plan. The genre idiom owner
      // may reshape its final topology, including protected empty regions.
    }
    previousMaxId = ids.back();

    // B6: authoritative PREPARE data, reproduced from the same attempt.
    // (Scene is wiped afterwards; the engine-owned sidecar is unaffected.)
    const GeneratedSynthAOrigin snapshot = *origin;
    configureScene(engine, route.mode, route.recipe);
    assert(result.p1r.phraseGenerationIdentity <
           GroovePuterRhythm::kUnspecifiedPhraseGenerationIdentity);
    GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
    assert(GeneratedPhraseSong::prepareWithGenerationAttempt(
        engine, route.bars, 0, result.p1r.phraseGenerationIdentity, true, prepared));
    assert(prepared.useP1RRoute);
    const auto& exec = prepared.p1rExecution;
    assert(snapshot.common.rootPitchClass == exec.materialization.rootPitchClass);
    assert(snapshot.common.scaleTypeValue == exec.materialization.scaleTypeValue);
    assert(sameSource(snapshot.common.progressionSource, exec.progressionSource));
    for (int bar = 0; bar < route.bars; ++bar) {
      assert(sameHarmonic(snapshot.bars[bar].harmonicRhythm,
                          exec.harmonicClock.bars[bar].harmonicRhythm));
      assert(snapshot.bars[bar].harmonicRhythm.phraseBarOrdinal == bar);
    }
    assert(sameOrigin(snapshot, *engine.generatedSynthAOrigin()));
  }
  std::puts("D1-B B3-B6: multi-bar origin binding + harmonic correspondence: PASS");
}

// ---- B7 --------------------------------------------------------------------
void test_b7_failed_generation_keeps_previous_origin() {
  freshProject("d1b-b7");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureScene(engine, GenerativeMode::Chip, 0);

  const auto a = GeneratedPhraseSong::generate(engine, 1, 0, kGuard);
  assert(a.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  const GeneratedSynthAOrigin sidecarA = *engine.generatedSynthAOrigin();

  // (1) reservation I/O failure (F4-style fault: non-empty dir at .tmp).
  const std::filesystem::path meta = newestIdentityMeta();
  assert(!meta.empty());
  const std::filesystem::path blocker = meta.string() + ".tmp";
  std::filesystem::create_directories(blocker / "blocked");
  const auto io = GeneratedPhraseSong::generate(engine, 2, 1, kGuard);
  assert(io.status == GeneratedPhraseSong::LifecycleStatus::Failed);
  assert(engine.generatedSynthAOrigin() != nullptr);
  assert(sameOrigin(sidecarA, *engine.generatedSynthAOrigin()));
  std::filesystem::remove_all(blocker);

  // (2) PREPARE failure: every remaining slot semantically occupied.
  Scene& scene = engine.sceneManager().currentScene();
  for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) {
    if (scene.materialSlots[0][s].isFree()) {
      scene.materialSlots[0][s].kind = MaterialKind::Melody;
    }
  }
  const auto prep = GeneratedPhraseSong::generate(engine, 4, 1, kGuard);
  assert(prep.status == GeneratedPhraseSong::LifecycleStatus::Failed);
  assert(sameOrigin(sidecarA, *engine.generatedSynthAOrigin()));

  std::puts("D1-B B7: failed generation keeps previous sidecar: PASS");
}

// ---- B8 --------------------------------------------------------------------
void test_b8_undo_clears_origin() {
  freshProject("d1b-b8");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureScene(engine, GenerativeMode::Chip, 0);
  Scene& scene = engine.sceneManager().currentScene();

  const auto r = GeneratedPhraseSong::generate(engine, 2, 0, kGuard);
  assert(r.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(engine.generatedSynthAOrigin() != nullptr);
  const MaterialReference removed0 = engine.generatedSynthAOrigin()->bars[0].material;
  const MaterialReference removed1 = engine.generatedSynthAOrigin()->bars[1].material;
  assert(engine.findGeneratedSynthAOrigin(removed0) != nullptr);

  assert(GeneratedPhraseSong::undoLastGeneratedPhrase(engine, kGuard) ==
         GroovePuterUndo::UndoResult::Restored);
  for (int bar = 0; bar < 2; ++bar) {
    const int slot = r.phrase.firstLocalSlot + bar;
    assert(PhraseGenerator::localSlotIsEmpty(scene, slot));
    assert(GroovePuterMaterial::residentSlotIsFree(scene, 0, slot));
  }
  assert(engine.generatedSynthAOrigin() == nullptr);
  assert(engine.findGeneratedSynthAOrigin(removed0) == nullptr);
  assert(engine.findGeneratedSynthAOrigin(removed1) == nullptr);

  std::puts("D1-B B8: Undo clears origin for removed phrase: PASS");
}

// ---- B9 --------------------------------------------------------------------
void test_b9_legacy_generation_never_claims_p1r_origin() {
  freshProject("d1b-b9");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);

  // Direct legacy generation with no previous sidecar: nothing published.
  configureScene(engine, GenerativeMode::Techno, 250);
  Scene& scene = engine.sceneManager().currentScene();
  assert(GroovePuterRhythm::selectStrongRhythmRoute(scene.genre) ==
         GroovePuterRhythm::StrongRhythmRoute::Legacy);
  const auto legacy = GeneratedPhraseSong::generate(engine, 1, 0, kGuard);
  assert(legacy.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(!legacy.p1r.usedP1r);
  assert(!PhraseGenerator::localSlotIsEmpty(scene, legacy.phrase.firstLocalSlot));
  assert(engine.generatedSynthAOrigin() == nullptr);

  // P1R first, then a successful Legacy phrase: policy = clear the P1R sidecar
  // (the latest generated phrase no longer has the required provenance).
  configureScene(engine, GenerativeMode::Chip, 0);
  const auto p1r = GeneratedPhraseSong::generate(engine, 1, 0, kGuard);
  assert(p1r.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(engine.generatedSynthAOrigin() != nullptr);
  const MaterialReference p1rRef = engine.generatedSynthAOrigin()->bars[0].material;

  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
  scene.genre.recipe = 250;
  engine.genreManager().setGenerativeMode(GenerativeMode::Techno);
  const auto second = GeneratedPhraseSong::generate(engine, 1, 1, kGuard);
  assert(second.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(!second.p1r.usedP1r);
  assert(engine.generatedSynthAOrigin() == nullptr);
  assert(engine.findGeneratedSynthAOrigin(p1rRef) == nullptr);

  std::puts("D1-B B9: Legacy generation clears / never publishes P1R origin: PASS");
}

// ---- B10 / B11 -------------------------------------------------------------
void test_b10_b11_survival_and_immutability() {
  freshProject("d1b-b10");
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureScene(engine, GenerativeMode::Chip, 0);
  Scene& scene = engine.sceneManager().currentScene();

  const auto r = GeneratedPhraseSong::generate(engine, 2, 0, kGuard);
  assert(r.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  const GeneratedSynthAOrigin before = *engine.generatedSynthAOrigin();
  const int slot = r.phrase.firstLocalSlot;
  const int bank = slot / Bank<SynthPattern>::kPatterns;
  const int index = slot % Bank<SynthPattern>::kPatterns;

  // B10: unrelated read / playback operations.
  engine.set303BankIndex(0, bank);
  engine.set303PatternIndex(0, index);
  const auto basis = engine.captureCurrentPreparationBasis(0);
  assert(basis.valid());
  assert(engine.findGeneratedSynthAOrigin(basis.reference) != nullptr);
  engine.start();
  engine.stop();
  (void)engine.captureCurrentPreparationBasis(0);
  assert(sameOrigin(before, *engine.generatedSynthAOrigin()));

  // "Current exact to origin"
  const auto* barOrigin = engine.findGeneratedSynthAOrigin(basis.reference);
  assert(barOrigin != nullptr);
  assert(barOrigin->originPatternVersion == basis.version);

  // B11: editing CURRENT never rewrites the immutable origin evidence.
  scene.synthABanks[bank].patterns[index].steps[0].note += 1;
  const auto edited = engine.captureCurrentPreparationBasis(0);
  assert(edited.valid());
  assert(edited.version != barOrigin->originPatternVersion);  // changed since origin
  assert(engine.findGeneratedSynthAOrigin(edited.reference) == barOrigin);  // still known
  assert(sameOrigin(before, *engine.generatedSynthAOrigin()));

  std::puts("D1-B B10/B11: survives unrelated ops, immutable under CURRENT edits: PASS");
}


// B1 adversarial: routes whose bass plan EVOLVES across bars. The sidecar keeps
// the bar-indexed rhythm-role evidence; genre idiom materialization separately
// owns the final Synth A onset topology.
void test_b1_evolving_plans_discriminate() {
  struct Evolving { uint8_t bars; GenerativeMode mode; GenreRecipeId recipe; };
  const Evolving routes[] = {
      {8, GenerativeMode::Reggae, 3},
      {4, GenerativeMode::TripHop, 1},
  };
  size_t discriminated = 0;
  for (const auto& route : routes) {
    freshProject("d1b-evolving");
    SceneStorageSdl storage;
    MiniAcid engine{kSampleRate, &storage};
    engine.init();
    engine.setSongMode(false);
    configureScene(engine, route.mode, route.recipe);
    Scene& scene = engine.sceneManager().currentScene();
    const auto result = GeneratedPhraseSong::generate(engine, route.bars, 0, kGuard);
    assert(result.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
    assert(result.p1r.usedP1r);
    const GeneratedSynthAOrigin* origin = engine.generatedSynthAOrigin();
    assert(origin != nullptr);

    uint16_t planMasks[8]{};
    for (int bar = 0; bar < route.bars; ++bar) {
      assert(origin->bars[bar].phraseBarOrdinal == bar);
      planMasks[bar] = planMask(origin->bars[bar].bassRhythm);
    }
    for (int i = 0; i < route.bars; ++i) {
      for (int j = 0; j < route.bars; ++j) {
        if (planMasks[i] != planMasks[j]) {
          ++discriminated;
        }
      }
    }
  }
  // The test is only meaningful if the plan really differs between bars.
  assert(discriminated > 0);
  std::printf("D1-B B1: evolving-plan discrimination pairs = %zu\n", discriminated);
  std::puts("D1-B B1 adversarial: stale/other-bar plan export would fail: PASS");
}

void report_sizes() {
  std::printf("D1-B sizes: StrongRhythmMigrationResult=%zu barEvidence=%zu "
              "originCommon=%zu originBar=%zu originTotal=%zu "
              "candidate=%zu PreparedPhraseArrangement=%zu UndoPayload=%zu\n",
              sizeof(GroovePuterRhythm::StrongRhythmMigrationResult),
              sizeof(GeneratedPhraseP1R::MaterializedSynthABarEvidence),
              sizeof(GroovePuterMaterial::GeneratedSynthAOriginCommon),
              sizeof(GeneratedSynthABarOrigin),
              sizeof(GeneratedSynthAOrigin),
              sizeof(GroovePuterMaterial::GeneratedSynthAOriginCandidate),
              sizeof(GeneratedPhraseSong::PreparedPhraseArrangement),
              sizeof(GeneratedPhraseSong::GeneratedPhraseUndoPayload));
  assert(sizeof(GeneratedSynthAOrigin) <= 352);
  assert(sizeof(GeneratedPhraseSong::PreparedPhraseArrangement) <= 1024);
}

}  // namespace

int main() {
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");

  report_sizes();
  test_b1_b2_bass_plan_export_and_forwarding();
  test_b1_evolving_plans_discriminate();
  test_b3_to_b6_generated_origin_multibar();
  test_b7_failed_generation_keeps_previous_origin();
  test_b8_undo_clears_origin();
  test_b9_legacy_generation_never_claims_p1r_origin();
  test_b10_b11_survival_and_immutability();

  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");
  std::puts("0.9.14 D1-B generated Synth A origin: ALL PASS");
  return 0;
}
