#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <atomic>
#include <thread>
#include "src/audio/audio_mutation_gate.h"

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/audio/pattern_paging.h"
#include "src/state/melody_promotion.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::PublicationSlot;
using GroovePuterMaterial::MaterialPublicationRecord;

static std::filesystem::path s_origCwd;

void setupTestDirectory(const std::string& name) {
  if (s_origCwd.empty()) {
    s_origCwd = std::filesystem::current_path();
  }
  const auto root = std::filesystem::temp_directory_path() / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
}

void test_pattern_accept_and_cold_boot() {
  std::puts("--- TEST 1: Pattern ACCEPT and Cold Boot Recovery ---");
  setupTestDirectory("gp_test_pattern_accept");

  const std::string proj = "pattern_proj";
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);

  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  MaterialId acceptedId{};
  int8_t acceptedNote = -1;
  const int8_t targetNote = 72;
  {
    // Session 1: Edit pattern and ACCEPT
    MiniAcid engine{44100.0f, &storage};
    engine.init();

    // Initial check
    assert(engine.currentPageIndex() == 0);
    assert(engine.current303BankIndex(0) == 0);
    assert(engine.display303LocalPatternIndex(0) == 0);

    // Test Case A: Clean accept on untouched pattern returns AlreadyClean
    assert(engine.workingMaterial_[0].empty());
    assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::AlreadyClean);

    Scene& scene = engine.sceneManager().currentScene();
    if (!scene.materialSlots[0][0].id.valid()) {
      scene.materialSlots[0][0].id = PatternPagingService::allocateMaterialId();
      scene.materialSlots[0][0].kind = MaterialKind::Pattern;
    }

    // Prepare a modified working pattern
    SynthPattern workingPattern = scene.synthABanks[0].patterns[0];
    acceptedNote = (workingPattern.steps[0].note == targetNote) ? 75 : targetNote;
    workingPattern.steps[0].note = acceptedNote;
    workingPattern.steps[0].accent = true;
    workingPattern.steps[0].slide = true;

    MaterialReference ref{{0, 0}, scene.materialSlots[0][0].id};
    engine.workingMaterial_[0].storePattern(workingPattern, ref);
    assert(engine.hasModifiedWorking303Pattern(0));

    // ACCEPT
    const auto result = engine.acceptMaterialWorking(0);
    assert(result == MiniAcid::AcceptResult::Accepted);

    // Verify RAM state after ACCEPT
    assert(!engine.hasModifiedWorking303Pattern(0));
    assert(engine.workingMaterial_[0].empty());
    assert(engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note == acceptedNote);
    assert(engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].accent == true);
    assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern);

    acceptedId = engine.sceneManager().currentScene().materialSlots[0][0].id;
    assert(acceptedId.valid());

    // Save scene metadata so cold boot loads scene pointing to project
    engine.saveSceneToStorage();
    std::puts("  Session 1 ACCEPT complete, destroying engine (power off simulation)...");
  }

  {
    // Session 2: Cold boot restart
    std::puts("  Session 2 Cold Boot starting...");
    PatternPagingService::setProjectName(proj);
    MiniAcid engine2{44100.0f, &storage};
    engine2.init();

    // Verify exact accepted pattern restored
    const Scene& scene2 = engine2.sceneManager().currentScene();
    assert(scene2.materialSlots[0][0].id == acceptedId);
    assert(scene2.materialSlots[0][0].kind == MaterialKind::Pattern);
    assert(scene2.synthABanks[0].patterns[0].steps[0].note == acceptedNote);
    assert(scene2.synthABanks[0].patterns[0].steps[0].accent == true);
    assert(scene2.synthABanks[0].patterns[0].steps[0].slide == true);

    // Verify sounding runtime bank restored
    assert(engine2.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern);

    // Working material must be clean/empty
    assert(!engine2.hasModifiedWorking303Pattern(0));
    assert(engine2.workingMaterial_[0].empty());

    std::puts("  Cold boot restored exact accepted Pattern: PASS");
  }
}

void test_melody_accept_and_cold_boot() {
  std::puts("--- TEST 2: Melody ACCEPT and Cold Boot Recovery ---");
  setupTestDirectory("gp_test_melody_accept");

  const std::string proj = "melody_proj";
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);

  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  MaterialId melodyId{};
  PhraseRuntime::RuntimeSynthEventBuffer expectedMelody{};
  expectedMelody.count = 4;
  expectedMelody.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  for (uint16_t i = 0; i < 4; ++i) {
    expectedMelody.events[i].startTick = i * 48;
    expectedMelody.events[i].durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
    expectedMelody.events[i].note = static_cast<uint8_t>(50 + i * 3);
    expectedMelody.events[i].velocity = 110;
    expectedMelody.events[i].probability = 100;
  }

  {
    // Session 1: Create 2-bar melody in Working and ACCEPT
    MiniAcid engine{44100.0f, &storage};
    engine.init();

    Scene& scene = engine.sceneManager().currentScene();
    assert(!scene.materialSlots[0][0].id.valid());

    engine.workingMaterial_[0].storeMelody(expectedMelody);
    assert(engine.workingMaterial_[0].holdsMelody());

    const auto result = engine.acceptMaterialWorking(0);
    assert(result == MiniAcid::AcceptResult::Accepted);

    // Verify RAM state after ACCEPT
    assert(engine.workingMaterial_[0].holdsMelody());
    assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);
    assert(engine.sceneManager().currentScene().materialSlots[0][0].kind == MaterialKind::Melody);

    melodyId = engine.sceneManager().currentScene().materialSlots[0][0].id;
    assert(melodyId.valid());

    // Save scene metadata
    engine.saveSceneToStorage();
    std::puts("  Session 1 Melody ACCEPT complete, destroying engine...");
  }

  {
    // Session 2: Cold boot restart
    std::puts("  Session 2 Cold Boot starting for Melody...");
    PatternPagingService::setProjectName(proj);
    MiniAcid engine2{44100.0f, &storage};
    engine2.init();

    // Verify exact accepted melody restored
    const Scene& scene2 = engine2.sceneManager().currentScene();
    assert(scene2.materialSlots[0][0].id == melodyId);
    assert(scene2.materialSlots[0][0].kind == MaterialKind::Melody);
    assert(engine2.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);

    assert(engine2.workingMaterial_[0].holdsMelody());
    const auto& actualMelody = engine2.workingMaterial_[0].melody();
    assert(actualMelody.count == expectedMelody.count);
    assert(actualMelody.lengthTicks == expectedMelody.lengthTicks);
    for (uint16_t i = 0; i < expectedMelody.count; ++i) {
      assert(actualMelody.events[i].startTick == expectedMelody.events[i].startTick);
      assert(actualMelody.events[i].durationSubticks == expectedMelody.events[i].durationSubticks);
      assert(actualMelody.events[i].note == expectedMelody.events[i].note);
      assert(actualMelody.events[i].velocity == expectedMelody.events[i].velocity);
    }

    std::puts("  Cold boot restored exact accepted Melody: PASS");
  }
}

void test_fault_matrix_power_loss() {
  std::puts("--- TEST 3: Fault Matrix (T0..T4) Power Loss Durability ---");
  setupTestDirectory("gp_test_fault_matrix");

  const std::string proj = "fault_proj";
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);

  Scene sceneA{};
  sceneA.synthABanks[0].patterns[0].steps[0].note = 40;
  const MaterialId idA = PatternPagingService::allocateMaterialId();
  sceneA.materialSlots[0][0].id = idA;
  sceneA.materialSlots[0][0].kind = MaterialKind::Pattern;

  // T0: Initial write of Slot A (Gen 1)
  assert(PatternPagingService::commitPageCandidate(
      0, sceneA, 0, 0, 0,
      &sceneA.synthABanks[0].patterns[0],
      MaterialKind::Pattern, idA));

  Scene loaded{};
  assert(PatternPagingService::loadPage(0, loaded));
  assert(loaded.synthABanks[0].patterns[0].steps[0].note == 40);
  assert(PatternPagingService::activePublicationSlot(0) == PublicationSlot::SlotB ||
         PatternPagingService::activePublicationSlot(0) == PublicationSlot::SlotA);
  const auto initialSlot = PatternPagingService::activePublicationSlot(0);
  std::puts("  T0 Initial commit validated");

  // T1: Simulate power loss / crash during write of next candidate slot (partial write / corrupted CRC)
  const auto nextSlot = (initialSlot == PublicationSlot::SlotA) ? PublicationSlot::SlotB : PublicationSlot::SlotA;
  const std::string corruptedPath = PatternPagingService::slotPathForProject(proj, 0, nextSlot);
  {
    File f = SD.open(corruptedPath.c_str(), FILE_WRITE);
    assert(f);
    const char garbage[] = "CORRUPT_INCOMPLETE_PAGE_DATA_DURING_POWER_LOSS";
    f.write(reinterpret_cast<const uint8_t*>(garbage), sizeof(garbage));
    f.close();
  }

  // Reload page: loader must reject corrupted candidate and recover initial valid slot!
  Scene recoveredT1{};
  assert(PatternPagingService::loadPage(0, recoveredT1));
  assert(recoveredT1.synthABanks[0].patterns[0].steps[0].note == 40);
  assert(recoveredT1.materialSlots[0][0].id == idA);
  std::puts("  T1 Corrupt write rejected, previous slot restored: PASS");

  // T2: Simulate crash after candidate slot write, but BEFORE NVS publication record commit
  // Write valid candidate to nextSlot with generation 2
  SynthPattern candidatePat = sceneA.synthABanks[0].patterns[0];
  candidatePat.steps[0].note = 45;
  const MaterialId idB = PatternPagingService::allocateMaterialId();

  // Commit candidate normally to produce Gen 2
  assert(PatternPagingService::commitPageCandidate(
      0, sceneA, 0, 0, 0,
      &candidatePat, MaterialKind::Pattern, idB));

  // Now manually rollback NVS publication record to point to initialSlot (Gen 1)
  // to simulate that NVS write never happened!
  MaterialPublicationRecord oldRec{};
  oldRec.magic = GroovePuterMaterial::kPublicationMagic;
  oldRec.schema = GroovePuterMaterial::kPublicationSchema;
  oldRec.slot = static_cast<uint8_t>(initialSlot);
  oldRec.pageIndex = 0;
  oldRec.storageGeneration = 1;
  oldRec.checksum = GroovePuterMaterial::checksumPublicationRecord(oldRec);
  assert(GroovePuterPlatform::saveMaterialPublication(proj, 0, oldRec));

  // Reload: NVS record is authoritative, so it must load initialSlot (note 40)!
  Scene recoveredT2{};
  assert(PatternPagingService::loadPage(0, recoveredT2));
  assert(recoveredT2.synthABanks[0].patterns[0].steps[0].note == 40);
  assert(recoveredT2.materialSlots[0][0].id == idA);
  std::puts("  T2 Power loss before NVS commit fails-closed to published slot: PASS");

  // T3: Re-commit candidate: NVS publication record updated to Gen 2
  assert(PatternPagingService::commitPageCandidate(
      0, sceneA, 0, 0, 0,
      &candidatePat, MaterialKind::Pattern, idB));
  Scene recoveredT3{};
  assert(PatternPagingService::loadPage(0, recoveredT3));
  assert(recoveredT3.synthABanks[0].patterns[0].steps[0].note == 45);
  assert(recoveredT3.materialSlots[0][0].id == idB);
  std::puts("  T3 NVS commit published candidate successfully: PASS");

  // T4: Active slot corrupted after commit: CRC fails -> fallback to alternate slot
  const auto activeSlotT4 = PatternPagingService::activePublicationSlot(0);
  const std::string activePathT4 = PatternPagingService::slotPathForProject(proj, 0, activeSlotT4);
  {
    File f = SD.open(activePathT4.c_str(), FILE_WRITE);
    assert(f);
    // Overwrite header to corrupt CRC
    uint8_t bad[32] = {0xFF};
    f.write(bad, sizeof(bad));
    f.close();
  }

  Scene recoveredT4{};
  assert(PatternPagingService::loadPage(0, recoveredT4));
  // Must fall back to alternate slot (note 40)
  assert(recoveredT4.synthABanks[0].patterns[0].steps[0].note == 40);
  std::puts("  T4 Active slot corruption falls back to valid alternate: PASS");
}

void test_scoped_undo_invalidation() {
  std::puts("--- TEST 4: Scoped Undo Invalidation ---");
  setupTestDirectory("gp_test_scoped_undo");

  const std::string proj = "undo_proj";
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);

  SceneStorageSdl storage;
  MiniAcid engine{44100.0f, &storage};
  engine.init();

  Scene& scene = engine.sceneManager().currentScene();
  if (!scene.materialSlots[0][0].id.valid()) {
    scene.materialSlots[0][0].id = PatternPagingService::allocateMaterialId();
    scene.materialSlots[0][0].kind = MaterialKind::Pattern;
  }
  if (!scene.materialSlots[1][0].id.valid()) {
    scene.materialSlots[1][0].id = PatternPagingService::allocateMaterialId();
    scene.materialSlots[1][0].kind = MaterialKind::Pattern;
  }

  auto& undo = GroovePuterUndo::undoOwner();

  // 1. Commit undo receipt for voice 0
  GroovePuterUndo::RuntimePhraseUndoPayload payload0{};
  payload0.voiceIndex = 0;
  payload0.representation = 0;

  undo.clear();
  assert(undo.commitRuntimePrepared(GroovePuterUndo::UndoKind::RuntimePhrase, payload0, []() {}));
  assert(undo.hasUndo());

  // ACCEPT on voice 0 should clear voice 0 receipt
  // Give working material a delta so accept proceeds
  SynthPattern pat0 = scene.synthABanks[0].patterns[0];
  pat0.steps[0].note = 65;
  MaterialReference ref0{{0, 0}, scene.materialSlots[0][0].id};
  engine.workingMaterial_[0].storePattern(pat0, ref0);

  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
  assert(!undo.hasUndo());
  std::puts("  Voice 0 ACCEPT cleared matching voice 0 undo receipt: PASS");

  // 2. Commit undo receipt for voice 1
  GroovePuterUndo::RuntimePhraseUndoPayload payload1{};
  payload1.voiceIndex = 1;
  payload1.representation = 0;

  undo.clear();
  assert(undo.commitRuntimePrepared(GroovePuterUndo::UndoKind::RuntimePhrase, payload1, []() {}));
  assert(undo.hasUndo());

  // ACCEPT on voice 0 should NOT clear voice 1 receipt!
  pat0.steps[0].note = 66;
  ref0.id = scene.materialSlots[0][0].id; // update ref to new version/id if mutated
  engine.workingMaterial_[0].storePattern(pat0, ref0);
  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
  assert(undo.hasUndo()); // Voice 1 receipt preserved!
  std::puts("  Voice 0 ACCEPT preserved non-matching voice 1 undo receipt: PASS");

  // ACCEPT on voice 1 should clear voice 1 receipt
  SynthPattern pat1 = scene.synthBBanks[0].patterns[0];
  pat1.steps[0].note = 70;
  MaterialReference ref1{{1, 0}, scene.materialSlots[1][0].id};
  engine.workingMaterial_[1].storePattern(pat1, ref1);
  assert(engine.acceptMaterialWorking(1) == MiniAcid::AcceptResult::Accepted);
  assert(!undo.hasUndo());
  std::puts("  Voice 1 ACCEPT cleared matching voice 1 undo receipt: PASS");
}

void test_accept_preserves_audible_and_playback_state() {
  std::puts("--- TEST 5: ACCEPT Preserves Audible & Playback State (Persistence Not Playback) ---");
  setupTestDirectory("gp_test_accept_invariants");

  const std::string proj = "inv_proj";
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);

  SceneStorageSdl storage;
  MiniAcid engine{44100.0f, &storage};
  engine.init();

  Scene& scene = engine.sceneManager().currentScene();
  scene.materialSlots[0][0].id = PatternPagingService::allocateMaterialId();
  scene.materialSlots[0][0].kind = MaterialKind::Pattern;

  // 1. Pattern Working check
  SynthPattern pat = scene.synthABanks[0].patterns[0];
  pat.steps[0].note = 58;
  pat.steps[1].note = 62;
  MaterialReference ref0{{0, 0}, scene.materialSlots[0][0].id};
  engine.workingMaterial_[0].storePattern(pat, ref0);

  const auto sourceBeforePattern = engine.currentSequencedSource(0);
  const auto repBeforePattern = engine.workingMaterial_[0].holdsMelody() ? MaterialKind::Melody : MaterialKind::Pattern;
  const SynthPattern audibleBeforePattern = engine.workingMaterial_[0].pattern();

  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);

  // Invariant assertions:
  assert(engine.currentSequencedSource(0) == sourceBeforePattern);
  assert(engine.activeMaterial(0).kind == repBeforePattern);
  const SynthPattern& audibleAfterPattern = engine.sceneManager().currentScene().synthABanks[0].patterns[0];
  assert(std::memcmp(&audibleBeforePattern, &audibleAfterPattern, sizeof(SynthPattern)) == 0);
  std::puts("  Pattern ACCEPT: sequencedSource, representation, and audible bytes strictly invariant: PASS");

  // 2. Melody Working check
  // Set material length to 2 bars -> mutation sets source to Phrase and rep to Melody
  assert(engine.setMaterialLength(0, 2) == MiniAcid::MaterialLengthResult::Changed);
  assert(engine.workingMaterial_[0].holdsMelody());
  assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);

  // Edit a note in the melody
  auto& mel = engine.workingMaterial_[0].melody();
  mel.events[0].note = 53;

  const auto sourceBeforeMelody = engine.currentSequencedSource(0);
  const auto repBeforeMelody = engine.workingMaterial_[0].holdsMelody() ? MaterialKind::Melody : MaterialKind::Pattern;
  const auto audibleBeforeMelody = engine.workingMaterial_[0].melody();

  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);

  // Invariant assertions:
  assert(engine.currentSequencedSource(0) == sourceBeforeMelody);
  assert(engine.activeMaterial(0).kind == repBeforeMelody);
  assert(engine.workingMaterial_[0].holdsMelody());
  const auto& audibleAfterMelody = engine.workingMaterial_[0].melody();
  assert(std::memcmp(&audibleBeforeMelody, &audibleAfterMelody, sizeof(PhraseRuntime::RuntimeSynthEventBuffer)) == 0);
  std::puts("  Melody ACCEPT: sequencedSource, representation, and audible bytes strictly invariant: PASS");
}

void test_page_disk_commit_without_early_ram_publication() {
  setupTestDirectory("gp_test_page_deferred_publish");
  const std::string proj = "deferred_proj";
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  assert(PatternPagingService::setProjectName(proj));

  Scene scene{};
  scene.materialSlots[0][0].kind = MaterialKind::Pattern;
  scene.materialSlots[0][0].id = MaterialId{7001};
  scene.synthABanks[0].patterns[0].steps[0].note = 48;
  SynthPattern candidate = scene.synthABanks[0].patterns[0];
  candidate.steps[0].note = 72;

  assert(PatternPagingService::commitPageCandidate(
      0, scene, 0, 0, 0, &candidate, MaterialKind::Pattern,
      MaterialId{7001}, false));
  assert(scene.synthABanks[0].patterns[0].steps[0].note == 48);
  Scene loaded{};
  assert(PatternPagingService::loadPage(0, loaded));
  assert(loaded.synthABanks[0].patterns[0].steps[0].note == 72);
  PatternPagingService::publishPageCandidateRam(
      0, scene, 0, 0, 0, &candidate, MaterialKind::Pattern,
      MaterialId{7001});
  assert(scene.synthABanks[0].patterns[0].steps[0].note == 72);
}

void test_live_pattern_accept_keeps_audio_advancing() {
  setupTestDirectory("gp_test_live_accept_audio");
  assert(PatternPagingService::setProjectName("live_accept_proj"));
  MiniAcid engine{44100.0f, nullptr};
  Scene& scene = engine.sceneManager().currentScene();
  scene.materialSlots[0][0].kind = MaterialKind::Pattern;
  scene.materialSlots[0][0].id = MaterialId{7002};
  scene.synthABanks[0].patterns[0].steps[0].note = 48;
  assert(engine.rebuildPatternRuntimeEventBank());
  SynthPattern candidate = scene.synthABanks[0].patterns[0];
  candidate.steps[0].note = 72;
  engine.workingMaterial_[0].storePattern(
      candidate, MaterialReference{{0, 0}, MaterialId{7002}});
  engine.playing = true;

  AudioMutationGate gate;
  engine.setAcceptAudioMutationGate(&gate);
  std::atomic<bool> running{true};
  std::atomic<uint32_t> blocks{0};
  gate.setAudioTaskActive(true);
  std::thread audio([&] {
    while (running.load(std::memory_order_acquire)) {
      gate.waitAtAudioBoundary();
      blocks.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::yield();
    }
  });
  while (blocks.load(std::memory_order_acquire) < 10) {
    std::this_thread::yield();
  }
  {
    AudioMutationScope scope(gate);
    const uint32_t beforeIo = blocks.load(std::memory_order_acquire);
    assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
    assert(blocks.load(std::memory_order_acquire) > beforeIo);
    assert(scene.synthABanks[0].patterns[0].steps[0].note == 72);
  }
  {
    AudioMutationScope scope(gate);
    assert(engine.setMaterialLength(0, 2) ==
           MiniAcid::MaterialLengthResult::Changed);
    const auto audibleBefore = engine.workingMaterial_[0].melody();
    const uint32_t beforeIo = blocks.load(std::memory_order_acquire);
    assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
    assert(blocks.load(std::memory_order_acquire) > beforeIo);
    assert(scene.materialSlots[0][0].kind == MaterialKind::Melody);
    assert(std::memcmp(&audibleBefore, &engine.workingMaterial_[0].melody(),
                       sizeof(audibleBefore)) == 0);
  }
  {
    AudioMutationScope scope(gate);
    auto& melody = engine.workingMaterial_[0].melody();
    melody.events[0].note = 55;
    const auto retained = melody;
    const auto descriptor = scene.materialSlots[0][0];
    SD.setRoot("/proc");
    assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::CommitFailed);
    SD.setRoot(std::filesystem::current_path());
    assert(scene.materialSlots[0][0].id == descriptor.id &&
           scene.materialSlots[0][0].kind == descriptor.kind);
    assert(std::memcmp(&retained, &engine.workingMaterial_[0].melody(),
                       sizeof(retained)) == 0);
  }
  {
    AudioMutationScope scope(gate);
    engine.songMode_ = true;
    const auto retained = engine.workingMaterial_[0].melody();
    assert(engine.acceptMaterialWorking(0) ==
           MiniAcid::AcceptResult::UnsupportedCurrentState);
    assert(std::memcmp(&retained, &engine.workingMaterial_[0].melody(),
                       sizeof(retained)) == 0);
    engine.songMode_ = false;
  }
  {
    // M2: DISCARD of an accepted Melody reads it back from SD. Like ACCEPT,
    // that read must not hold audio: blocks keep advancing, and the restored
    // Melody is the accepted one, not the unsaved edit.
    AudioMutationScope scope(gate);
    engine.workingMaterial_[0].melody().events[0].note = 57;
    const uint32_t beforeIo = blocks.load(std::memory_order_acquire);
    assert(engine.discardCurrentMaterial(0) == MiniAcid::DiscardResult::Discarded);
    assert(blocks.load(std::memory_order_acquire) > beforeIo);
    assert(engine.workingMaterial_[0].melody().events[0].note != 57);
    assert(!engine.hasUnsavedWorkingMelody(0));
  }
  running.store(false, std::memory_order_release);
  gate.setAudioTaskActive(false);
  audio.join();
}

} // namespace

int main() {
  std::puts("==================================================");
  std::puts("   GroovePuter 0.9.12 Material ACCEPT Test Suite  ");
  std::puts("==================================================");

  test_pattern_accept_and_cold_boot();
  test_melody_accept_and_cold_boot();
  test_fault_matrix_power_loss();
  test_scoped_undo_invalidation();
  test_accept_preserves_audible_and_playback_state();
  test_page_disk_commit_without_early_ram_publication();
  test_live_pattern_accept_keeps_audio_advancing();

  std::puts("==================================================");
  std::puts("   ALL MATERIAL ACCEPT TESTS PASSED SUCCESSFULLY! ");
  std::puts("==================================================");
  if (!s_origCwd.empty()) {
    std::error_code ec;
    std::filesystem::current_path(s_origCwd, ec);
  }
  return 0;
}
