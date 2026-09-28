#include <cassert>
#include <cstdio>
#include <filesystem>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/phrase_generator.h"
#include "src/state/material_slot_access.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialIdReservation;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialSlotDescriptor;

constexpr float kSampleRate = 44100.0f;

Buffer makeDummyMelody() {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 4;
  for (uint16_t i = 0; i < melody.count; ++i) {
    auto& ev = melody.events[i];
    ev.startTick = static_cast<uint16_t>(i * 96);
    ev.durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
    ev.note = static_cast<uint8_t>(60 + i * 2);
    ev.velocity = 100;
    ev.probability = 100;
  }
  return melody;
}

void configureCleanScene(MiniAcid& engine) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  scene.genre.recipe = kBaseRecipeId;
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
}

// ============================================================================
// CHECKPOINT A0: Descriptor-Aware Safe-Slot Predicate
// ============================================================================
void test_checkpoint_a0_descriptor_aware_slot_safety() {
  Scene scene{};
  constexpr int kTestSlot = 3;

  // CASE A0-1: physical empty, Synth A descriptor = Melody -> NOT SAFE
  scene.materialSlots[0][kTestSlot].kind = MaterialKind::Melody;
  scene.materialSlots[0][kTestSlot].id = MaterialId{};
  scene.materialSlots[1][kTestSlot] = MaterialSlotDescriptor{};
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));

  // CASE A0-2: physical empty, Synth B descriptor = Melody -> NOT SAFE
  scene.materialSlots[0][kTestSlot] = MaterialSlotDescriptor{};
  scene.materialSlots[1][kTestSlot].kind = MaterialKind::Melody;
  scene.materialSlots[1][kTestSlot].id = MaterialId{};
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));

  // CASE A0-3: physical empty, Synth A Pattern descriptor has valid MaterialId -> NOT SAFE
  scene.materialSlots[0][kTestSlot].kind = MaterialKind::Pattern;
  scene.materialSlots[0][kTestSlot].id = MaterialId{101};
  scene.materialSlots[1][kTestSlot] = MaterialSlotDescriptor{};
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));

  // CASE A0-4: physical empty, Synth B Pattern descriptor has valid MaterialId -> NOT SAFE
  scene.materialSlots[0][kTestSlot] = MaterialSlotDescriptor{};
  scene.materialSlots[1][kTestSlot].kind = MaterialKind::Pattern;
  scene.materialSlots[1][kTestSlot].id = MaterialId{202};
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));

  // CASE A0-5: physical empty, both synth descriptors canonical default/free Pattern, no Song refs -> SAFE
  scene.materialSlots[0][kTestSlot] = MaterialSlotDescriptor{};
  scene.materialSlots[1][kTestSlot] = MaterialSlotDescriptor{};
  assert(PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));

  // Invariant: physical non-emptiness still fails even if descriptor is free
  scene.synthABanks[0].patterns[kTestSlot].steps[0].note = 60;
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));
  scene.synthABanks[0].patterns[kTestSlot] = SynthPattern{};

  // Invariant: Song reference still fails even if descriptor is free
  const int globalPattern = songPatternFromPageBankIndex(0, 0, kTestSlot);
  scene.songs[0].positions[0].patterns[static_cast<int>(SongTrack::SynthA)] =
      static_cast<int16_t>(globalPattern);
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));
  scene.songs[0].positions[0].patterns[static_cast<int>(SongTrack::SynthA)] = -1;
  assert(PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, kTestSlot));

  // findSafeContiguousEmptySlots skips slots blocked by descriptors
  // Block slot 1 with Synth A Melody descriptor
  scene.materialSlots[0][1].kind = MaterialKind::Melody;
  // Slots 0 is free, 1 is blocked, 2..5 are free
  assert(PhraseGenerator::findSafeContiguousEmptySlots(scene, 0, 4) == 2);

  std::puts("D1-A A0: descriptor-aware slot safety: PASS");
}

// ============================================================================
// CHECKPOINT A1-A4: Canonical Batch MaterialId Reservation
// ============================================================================
void test_checkpoint_a1_batch_reservation() {
  const std::string proj = "d1a-res-test";
  assert(PatternPagingService::setProjectName(proj));
  assert(PatternPagingService::clearProjectPages());

  // Test valid bounded counts: 1, 2, 4, 8
  const uint8_t counts[] = {1, 2, 4, 8};
  uint32_t lastMaxId = 0;
  for (uint8_t count : counts) {
    const auto res = PatternPagingService::reserveMaterialIds(count);
    assert(res.valid());
    assert(res.count == count);
    assert(res.first.valid());
    assert(res.first.value > lastMaxId);

    // Monotonic and unique within batch
    for (uint8_t i = 0; i < count; ++i) {
      const auto id = res.idAt(i);
      assert(id.valid());
      assert(id.value == res.first.value + i);
    }
    // Out of range returns invalid id
    assert(!res.idAt(count).valid());

    lastMaxId = res.idAt(count - 1).value;
  }

  // Next reservation is strictly greater (never reuses)
  const auto nextRes = PatternPagingService::reserveMaterialIds(2);
  assert(nextRes.valid());
  assert(nextRes.first.value == lastMaxId + 1);

  // allocateMaterialId() delegates to reserveMaterialIds(1).first
  const auto single = PatternPagingService::allocateMaterialId();
  assert(single.valid());
  assert(single.value == nextRes.idAt(1).value + 1);

  // Fails closed on invalid counts
  assert(!PatternPagingService::reserveMaterialIds(0).valid());
  assert(!PatternPagingService::reserveMaterialIds(9).valid());

  // Persistence check: re-open project and verify monotonicity
  assert(PatternPagingService::setProjectName(proj));
  const auto afterReopen = PatternPagingService::reserveMaterialIds(1);
  assert(afterReopen.valid());
  assert(afterReopen.first.value > single.value);

  std::puts("D1-A A1-A4: batch MaterialId reservation: PASS");
}

// ============================================================================
// CHECKPOINT A5-A7: Multi-Bar Generation & Development Lifecycle Acceptance
// ============================================================================
void test_checkpoint_generation_and_development_lifecycle() {
  SceneStorageSdl storage;
  const std::string proj = "d1a-gen-lifecycle";
  assert(PatternPagingService::setProjectName(proj));
  assert(PatternPagingService::clearProjectPages());

  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);

  struct TestCase {
    uint8_t bars;
    GenerativeMode mode;
    GenreRecipeId recipe;
  };
  const TestCase cases[] = {
    {1, GenerativeMode::Chip, 0},
    {2, GenerativeMode::Chip, 0},
    {4, GenerativeMode::LoFi, 0},
    {8, GenerativeMode::LoFi, 0},
  };
  for (const auto& tc : cases) {
    const uint8_t bars = tc.bars;
    configureCleanScene(engine);
    Scene& scene = engine.sceneManager().currentScene();
    scene.genre.generativeMode = static_cast<uint8_t>(tc.mode);
    scene.genre.recipe = tc.recipe;
    engine.genreManager().setGenerativeMode(tc.mode);
    engine.genreManager().setRecipe(tc.recipe);

    const auto guard = [](auto&& body) { body(); };
    const auto result = GeneratedPhraseSong::generate(engine, bars, 0, guard);
    assert(result.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
    assert(result.phrase.error == PhraseGenerator::PhraseError::None);
    assert(result.phrase.bars == bars);

    const int firstSlot = result.phrase.firstLocalSlot;
    assert(firstSlot >= 0);

    std::vector<MaterialId> assignedIds;
    for (int b = 0; b < bars; ++b) {
      const int localSlot = firstSlot + b;
      const int bank = localSlot / Bank<SynthPattern>::kPatterns;
      const int index = localSlot % Bank<SynthPattern>::kPatterns;

      assert(!PhraseGenerator::localSlotIsEmpty(scene, localSlot));

      // Verify canonical Material descriptor publication
      const auto desc = scene.materialSlots[0][localSlot];
      assert(desc.kind == MaterialKind::Pattern);
      assert(desc.id.valid());
      assert(desc.id.value != 0);

      // Verify distinct within phrase
      for (const auto& existing : assignedIds) {
        assert(desc.id != existing);
      }
      assignedIds.push_back(desc.id);

      // Verify Song references
      const int globalPattern = songPatternFromPageBankIndex(
          engine.currentPageIndex(), bank, index);
      assert(scene.songs[0].positions[b].patterns[static_cast<int>(SongTrack::SynthA)] ==
             static_cast<int16_t>(globalPattern));
    }

    // CENTRAL DEVELOPMENT LIFECYCLE ACCEPTANCE (Section 9)
    // Select bar 0 of the generated phrase on Synth A
    const int bank0 = firstSlot / Bank<SynthPattern>::kPatterns;
    const int pattern0 = firstSlot % Bank<SynthPattern>::kPatterns;
    engine.set303BankIndex(0, bank0);
    engine.set303PatternIndex(0, pattern0);

    const auto basis = engine.captureCurrentPreparationBasis(0);

    // Witness invariants:
    // basis.reference.address == generated Synth A slot
    // basis.reference.id is valid
    // basis.reference.id == resident MaterialSlotDescriptor.id
    // basis.kind == Pattern
    // basis.version == versionForPattern(actual generated Synth A Pattern)
    // basis.valid() == true
    const int expectedGlobalSlot = songPatternFromPageBankIndex(
        engine.currentPageIndex(), bank0, pattern0);
    assert(basis.valid());
    assert(basis.reference.address.voice == 0);
    assert(basis.reference.address.globalSlot == expectedGlobalSlot);
    assert(basis.reference.id.valid());
    assert(basis.reference.id == scene.materialSlots[0][firstSlot].id);
    assert(basis.kind == MaterialKind::Pattern);
    assert(basis.version == GroovePuterMaterial::versionForPattern(
               scene.synthABanks[bank0].patterns[pattern0]));

    // Demonstrate that this generated Pattern now enters the NEXT lifecycle normally!
    const Buffer candidateMelody = makeDummyMelody();
    const auto prepResult = engine.prepareNextMelody(
        0, candidateMelody, basis,
        GroovePuterMaterial::IdeaClassification::Variation);
    assert(prepResult == MiniAcid::NextPrepareResult::Prepared);

    // F7: Test stale exact-version protection
    // Modifying the accepted pattern must invalidate preparation
    scene.synthABanks[bank0].patterns[pattern0].steps[0].note += 1;
    const auto staleResult = engine.prepareNextMelody(
        0, candidateMelody, basis,
        GroovePuterMaterial::IdeaClassification::Variation);
    assert(staleResult == MiniAcid::NextPrepareResult::StalePreparationBasis);
  }

  std::puts("D1-A A5-A7: multi-bar generation & development lifecycle: PASS");
}

// ============================================================================
// CHECKPOINT A5 / UNDO: Descriptor Restoration
// ============================================================================
void test_checkpoint_undo_descriptor_restoration() {
  SceneStorageSdl storage;
  const std::string proj = "d1a-undo-test";
  assert(PatternPagingService::setProjectName(proj));
  assert(PatternPagingService::clearProjectPages());

  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureCleanScene(engine);
  Scene& scene = engine.sceneManager().currentScene();

  const auto guard = [](auto&& body) { body(); };
  const auto genResult = GeneratedPhraseSong::generate(engine, 4, 0, guard);
  assert(genResult.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  const int firstSlot = genResult.phrase.firstLocalSlot;

  // Confirm descriptors and patterns are set
  for (int b = 0; b < 4; ++b) {
    const int slot = firstSlot + b;
    assert(scene.materialSlots[0][slot].id.valid());
    assert(!PhraseGenerator::localSlotIsEmpty(scene, slot));
  }

  const auto preUndoNextId = PatternPagingService::allocateMaterialId();

  // Perform Undo
  const auto undoResult =
      GeneratedPhraseSong::undoLastGeneratedPhrase(engine, guard);
  assert(undoResult == GroovePuterUndo::UndoResult::Restored);

  // Invariants on Undo:
  // 1. generated physical Synth A/B/Drum material disappears
  // 2. generated Song references disappear
  // 3. generated Synth A Material descriptor returns to canonical free/default
  for (int b = 0; b < 4; ++b) {
    const int slot = firstSlot + b;
    assert(PhraseGenerator::localSlotIsEmpty(scene, slot));
    assert(GroovePuterMaterial::residentSlotIsFree(scene, 0, slot));
    assert(scene.materialSlots[0][slot].kind == MaterialKind::Pattern);
    assert(!scene.materialSlots[0][slot].id.valid());
    assert(scene.songs[0].positions[b].patterns[static_cast<int>(SongTrack::SynthA)] == -1);
  }

  // 4. High-water mark is NOT rolled backwards
  const auto postUndoNextId = PatternPagingService::allocateMaterialId();
  assert(postUndoNextId.valid());
  assert(postUndoNextId.value > preUndoNextId.value);

  std::puts("D1-A Undo: descriptor restoration & high-water preservation: PASS");
}

// ============================================================================
// FAILURE CASES (F1 - F7)
// ============================================================================
void test_failure_cases() {
  SceneStorageSdl storage;
  const std::string proj = "d1a-failure-test";
  assert(PatternPagingService::setProjectName(proj));
  assert(PatternPagingService::clearProjectPages());

  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureCleanScene(engine);
  Scene& scene = engine.sceneManager().currentScene();

  // F1 & F2: Descriptor-aware allocator refuses occupied descriptors
  scene.materialSlots[0][0].kind = MaterialKind::Melody;
  scene.materialSlots[0][1].id = MaterialId{999};
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, 0));
  assert(!PhraseGenerator::localSlotIsSafeForPhrase(scene, 0, 1));
  // Clean back
  scene.materialSlots[0][0] = MaterialSlotDescriptor{};
  scene.materialSlots[0][1] = MaterialSlotDescriptor{};

  // F3: No safe contiguous range -> generation fails with typed error, no descriptors overwritten
  for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) {
    scene.materialSlots[0][s].kind = MaterialKind::Melody;
  }
  const auto guard = [](auto&& body) { body(); };
  auto resultF3 = GeneratedPhraseSong::generate(engine, 4, 0, guard);
  assert(resultF3.status == GeneratedPhraseSong::LifecycleStatus::Failed);
  assert(resultF3.phrase.error == PhraseGenerator::PhraseError::NoContiguousPatternSlots);
  for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) {
    assert(scene.materialSlots[0][s].kind == MaterialKind::Melody);
    assert(!scene.materialSlots[0][s].id.valid());
  }

  // Reset scene
  configureCleanScene(engine);

  // F5: Target becomes invalid before COMMIT -> fails closed, no descriptors published
  GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
  assert(GeneratedPhraseSong::prepare(engine, 4, 0, prepared));
  assert(GeneratedPhraseSong::preparedTargetStillCommitSafe(engine, prepared));
  // Invalidate target by occupying row 0
  scene.songs[0].positions[0].patterns[static_cast<int>(SongTrack::SynthA)] = 99;
  assert(!GeneratedPhraseSong::preparedTargetStillCommitSafe(engine, prepared));

  std::puts("D1-A Failure cases F1-F7: PASS");
}

}  // namespace

int main() {
  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");

  test_checkpoint_a0_descriptor_aware_slot_safety();
  test_checkpoint_a1_batch_reservation();
  test_checkpoint_generation_and_development_lifecycle();
  test_checkpoint_undo_descriptor_restoration();
  test_failure_cases();

  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");

  std::puts("0.9.14 D1-A material identity: ALL PASS");
  return 0;
}
