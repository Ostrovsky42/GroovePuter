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
// FAILURE CASES (F1 - F7) -- each case below is an executable runtime witness.
//
//   F1  occupied Melody descriptor        -> allocator skips slot (generate)
//   F2  occupied Pattern MaterialId       -> allocator skips slot (generate)
//   F3  no safe contiguous range          -> typed failure, nothing published
//   F4  reservation durable write fails   -> generate() fails closed
//   F5  target invalid before publication -> nothing visible, no ID reserved
//   F6  Undo                              -> see test_checkpoint_undo_...
//   F7  stale exact version               -> see test_checkpoint_generation_...
// ============================================================================

const auto kGuard = [](auto&& body) { body(); };

void configureChip(MiniAcid& engine) {
  configureCleanScene(engine);
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Chip);
  scene.genre.recipe = 0;
  engine.genreManager().setGenerativeMode(GenerativeMode::Chip);
  engine.genreManager().setRecipe(0);
}

// True when nothing generated is visible: physical Synth A/B/Drums empty,
// Song rows unreferenced, every Material descriptor canonical free.
bool nothingPublished(const Scene& scene) {
  for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
    if (!PhraseGenerator::localSlotIsEmpty(scene, slot)) return false;
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
      if (!GroovePuterMaterial::residentSlotIsFree(scene, voice, slot)) {
        return false;
      }
    }
  }
  for (int s = 0; s < 2; ++s) {
    for (const auto& position : scene.songs[s].positions) {
      for (const auto pattern : position.patterns) {
        if (pattern != -1) return false;
      }
    }
  }
  return true;
}

// The identity high-water is durable and monotonic: a probe allocation is the
// only public way to observe it. Returns the id handed out by the probe.
uint32_t probeHighWater() {
  const auto id = PatternPagingService::allocateMaterialId();
  assert(id.valid());
  return id.value;
}

// Other tests leave their own project directories behind, so pick the meta
// file that the immediately preceding probe touched (newest mtime).
std::filesystem::path findIdentityMeta() {
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

// F1 + F2: real allocator (generate()), not just the predicate. Slot 0 is
// physically empty but Synth B is Melody; slot 1 is physically empty but
// Synth A owns a valid MaterialId. Generation must land on slot 2 and leave
// both occupied descriptors untouched.
void test_f1_f2_allocator_skips_occupied_descriptors() {
  SceneStorageSdl storage;
  assert(PatternPagingService::setProjectName("d1a-f1f2"));
  assert(PatternPagingService::clearProjectPages());
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureChip(engine);
  Scene& scene = engine.sceneManager().currentScene();

  scene.materialSlots[1][0] = MaterialSlotDescriptor{MaterialKind::Melody, MaterialId{}};
  scene.materialSlots[0][1] = MaterialSlotDescriptor{MaterialKind::Pattern, MaterialId{999}};

  const auto result = GeneratedPhraseSong::generate(engine, 1, 0, kGuard);
  assert(result.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  assert(result.phrase.firstLocalSlot == 2);

  assert(scene.materialSlots[1][0] ==
         (MaterialSlotDescriptor{MaterialKind::Melody, MaterialId{}}));
  assert(PhraseGenerator::localSlotIsEmpty(scene, 0));
  assert(scene.materialSlots[0][1] ==
         (MaterialSlotDescriptor{MaterialKind::Pattern, MaterialId{999}}));
  assert(PhraseGenerator::localSlotIsEmpty(scene, 1));
  assert(!PhraseGenerator::localSlotIsEmpty(scene, 2));
  assert(scene.materialSlots[0][2].kind == MaterialKind::Pattern);
  assert(scene.materialSlots[0][2].id.valid());

  std::puts("D1-A F1/F2: generate() skips occupied descriptors: PASS");
}

// F3: every slot semantically occupied -> typed failure, zero publication,
// and no MaterialId reserved (failure happens in PREPARE, before reservation).
void test_f3_no_safe_range() {
  SceneStorageSdl storage;
  assert(PatternPagingService::setProjectName("d1a-f3"));
  assert(PatternPagingService::clearProjectPages());
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureChip(engine);
  Scene& scene = engine.sceneManager().currentScene();

  const uint32_t before = probeHighWater();
  for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) {
    scene.materialSlots[0][s].kind = MaterialKind::Melody;
  }
  const auto result = GeneratedPhraseSong::generate(engine, 4, 0, kGuard);
  assert(result.status == GeneratedPhraseSong::LifecycleStatus::Failed);
  assert(result.phrase.error ==
         PhraseGenerator::PhraseError::NoContiguousPatternSlots);
  for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s) {
    assert(scene.materialSlots[0][s] ==
           (MaterialSlotDescriptor{MaterialKind::Melody, MaterialId{}}));
    assert(PhraseGenerator::localSlotIsEmpty(scene, s));
  }
  assert(probeHighWater() == before + 1);  // no id consumed by the failure

  std::puts("D1-A F3: no safe contiguous range fails closed: PASS");
}

// F4: fault injection at the SDMock filesystem level (no production hook, no
// mock change). writeIdentityHighWater() first removes "<meta>.tmp"; a
// NON-EMPTY directory at that path makes SD.remove() fail, so the durable
// high-water write fails while the committed high-water stays readable.
void test_f4_reservation_io_failure() {
  SceneStorageSdl storage;
  assert(PatternPagingService::setProjectName("d1a-f4"));
  assert(PatternPagingService::clearProjectPages());
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureChip(engine);
  Scene& scene = engine.sceneManager().currentScene();

  const uint32_t before = probeHighWater();  // creates material_id.meta
  const std::filesystem::path meta = findIdentityMeta();
  assert(!meta.empty());
  const std::filesystem::path blocker = meta.string() + ".tmp";
  std::filesystem::create_directories(blocker / "blocked");

  // Direct API: fails closed with an invalid reservation.
  assert(!PatternPagingService::reserveMaterialIds(4).valid());
  assert(!PatternPagingService::allocateMaterialId().valid());

  // Through the real generate() route.
  const auto revisionBefore = GroovePuterUndo::undoOwner().committedRevision();
  assert(nothingPublished(scene));
  const auto result = GeneratedPhraseSong::generate(engine, 4, 0, kGuard);
  assert(result.status == GeneratedPhraseSong::LifecycleStatus::Failed);
  assert(nothingPublished(scene));  // no Pattern, no Song, no descriptor
  assert(GroovePuterUndo::undoOwner().committedRevision() == revisionBefore);
  assert(engine.songModeEnabled() == false);  // no transport/song side effects

  // Fault removed: identity service recovers, high-water did not advance.
  std::filesystem::remove_all(blocker);
  assert(probeHighWater() == before + 1);

  // And generation succeeds again with IDs beyond the previous high-water.
  const auto ok = GeneratedPhraseSong::generate(engine, 2, 0, kGuard);
  assert(ok.status == GeneratedPhraseSong::LifecycleStatus::CommittedNow);
  const int first = ok.phrase.firstLocalSlot;
  assert(scene.materialSlots[0][first].id.value > before + 1);

  std::puts("D1-A F4: reservation I/O failure fails closed: PASS");
}

// F5: the only pre-publication TargetChanged exits of generate() are
//   (a) the transport/song-slot check at the top, and
//   (b) preparedTargetStillCommitSafe() after PREPARE.
// PREPARE -> (b) -> reservation -> COMMIT run synchronously on the calling
// thread under the write lease, so no interleaving can be injected between
// (b) and COMMIT without a production hook; (b) is tested at the exact
// predicate boundary, (a) end-to-end, and the ordering (b < reservation <
// commitPrepared) is pinned in the source regressions.
void test_f5_target_invalid_before_publication() {
  SceneStorageSdl storage;
  assert(PatternPagingService::setProjectName("d1a-f5"));
  assert(PatternPagingService::clearProjectPages());
  MiniAcid engine{kSampleRate, &storage};
  engine.init();
  engine.setSongMode(false);
  configureChip(engine);
  Scene& scene = engine.sceneManager().currentScene();
  const uint32_t before = probeHighWater();

  // (a) end-to-end: playing while Song mode is off -> TargetChanged.
  engine.start();
  assert(engine.isPlaying());
  const auto result = GeneratedPhraseSong::generate(engine, 4, 0, kGuard);
  assert(result.status == GeneratedPhraseSong::LifecycleStatus::TargetChanged);
  engine.stop();
  assert(nothingPublished(scene));
  assert(probeHighWater() == before + 1);

  // (b) predicate boundary: PREPARE alone has no visible effect and consumes
  // no id; a Scene mutation after PREPARE invalidates the prepared target.
  GeneratedPhraseSong::PreparedPhraseArrangement prepared{};
  assert(GeneratedPhraseSong::prepare(engine, 4, 0, prepared));
  assert(nothingPublished(scene));
  assert(!prepared.synthAReservation.valid());
  assert(probeHighWater() == before + 2);
  assert(GeneratedPhraseSong::preparedTargetStillCommitSafe(engine, prepared));
  GroovePuterState::markSceneMutated();
  assert(!GeneratedPhraseSong::preparedTargetStillCommitSafe(engine, prepared));
  assert(nothingPublished(scene));

  std::puts("D1-A F5: invalid target -> nothing published, no id: PASS");
}

// B: post-success isolated main-loss recovery. After a fully successful
// reserveMaterialIds() only material_id.meta is lost/corrupted; ".bak" must
// hold the LATEST committed high-water so no returned id is ever reissued.
void test_post_success_main_loss_recovery() {
  for (int variant = 0; variant < 2; ++variant) {  // 0 = lost, 1 = corrupt
    assert(PatternPagingService::setProjectName("d1a-mainloss"));
    assert(PatternPagingService::clearProjectPages());
    const auto first = PatternPagingService::reserveMaterialIds(3);
    assert(first.valid());
    const auto last = PatternPagingService::reserveMaterialIds(4);  // success
    assert(last.valid());
    const uint32_t lastId = last.idAt(3).value;

    const std::string meta = std::filesystem::relative(
        findIdentityMeta(), std::filesystem::current_path()).string();
    assert(SD.exists((meta + ".bak").c_str()));
    assert(!SD.exists((meta + ".tmp").c_str()));
    if (variant == 0) {
      assert(SD.remove(meta.c_str()));
    } else {
      File f = SD.open(meta.c_str(), FILE_WRITE);
      assert(f);
      const uint8_t junk[] = {0xDE, 0xAD, 0xBE, 0xEF};
      assert(f.write(junk, sizeof(junk)) == sizeof(junk));
      f.close();  // size != MaterialIdentityMeta -> rejected by loader
    }

    const auto next = PatternPagingService::reserveMaterialIds(2);
    assert(next.valid());
    assert(next.first.value > lastId);
    assert(next.first.value == lastId + 1);  // latest high-water recovered
    assert(next.first.value > first.idAt(2).value);
  }
  std::puts("D1-A B: post-success main-loss recovery: PASS");
}

// SDMock regression for the host-only pubsetbuf(nullptr, 0) change:
// read / write / append / seek / rename / high-water metadata recovery.
void test_sdmock_unbuffered_semantics() {
  const char* path = "d1a_sdmock/file.bin";
  const uint8_t head[] = {'a', 'b', 'c'};
  const uint8_t tail[] = {'d', 'e', 'f'};
  SD.remove(path);

  { File f = SD.open(path, FILE_WRITE);  // creates
    assert(f);
    assert(f.write(head, 3) == 3);
    f.close(); }
  { File f = SD.open(path, FILE_WRITE);  // append keeps existing bytes
    assert(f);
    assert(f.write(tail, 3) == 3);
    f.flush();
    assert(f.size() == 6);  // visible immediately: no hidden buffering
    f.close(); }
  { File f = SD.open(path, FILE_READ);
    assert(f && f.size() == 6);
    uint8_t all[6] = {};
    assert(f.read(all, 6) == 6);
    assert(std::memcmp(all, "abcdef", 6) == 0);
    assert(f.seek(2));
    assert(f.read() == 'c');
    assert(f.position() == 3);
    assert(f.seek(5));
    assert(f.read() == 'f');
    assert(f.read() == -1);
    f.close(); }

  assert(SD.rename(path, "d1a_sdmock/renamed.bin"));
  assert(!SD.exists(path));
  assert(SD.exists("d1a_sdmock/renamed.bin"));
  { File f = SD.open("d1a_sdmock/renamed.bin", FILE_READ);
    assert(f && f.size() == 6);
    f.close(); }
  assert(SD.remove("d1a_sdmock/renamed.bin"));
  std::filesystem::remove_all("d1a_sdmock");

  // High-water recovery through the real identity service: the committed
  // value must survive loss of the main file (falls back to ".bak").
  assert(PatternPagingService::setProjectName("d1a-sdmock-hw"));
  assert(PatternPagingService::clearProjectPages());
  const uint32_t a = probeHighWater();
  const uint32_t b = probeHighWater();
  assert(b == a + 1);
  // SDMock resolves absolute paths against its root, so use a relative one.
  const std::string meta = std::filesystem::relative(
      findIdentityMeta(), std::filesystem::current_path()).string();
  assert(!meta.empty());
  assert(SD.exists(meta.c_str()));
  assert(SD.exists((meta + ".bak").c_str()));  // rename-based commit
  // Crash window of writeIdentityHighWater(): main was renamed to ".bak" but
  // the new file was not yet renamed into place. The committed value (b) must
  // be recovered from ".bak" and the next id must not reuse it.
  assert(SD.remove((meta + ".bak").c_str()));
  assert(SD.rename(meta.c_str(), (meta + ".bak").c_str()));
  assert(!SD.exists(meta.c_str()));
  assert(probeHighWater() == b + 1);

  std::puts("D1-A SDMock unbuffered semantics + high-water recovery: PASS");
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
  test_f1_f2_allocator_skips_occupied_descriptors();
  test_f3_no_safe_range();
  test_f4_reservation_io_failure();
  test_f5_target_invalid_before_publication();
  test_sdmock_unbuffered_semantics();
  test_post_success_main_loss_recovery();

  std::filesystem::remove_all("patterns");
  std::filesystem::remove_all("platform_sdl/patterns");
  std::filesystem::remove_all("projects");

  std::puts("0.9.14 D1-A material identity: ALL PASS");
  return 0;
}
