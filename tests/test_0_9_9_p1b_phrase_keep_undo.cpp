#include "src/phrase/phrase_keep.h"
#include "src/phrase/phrase_persistence.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
std::size_t gHeapAllocations = 0;
}

void* operator new(std::size_t size) {
  ++gHeapAllocations;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
  ++gHeapAllocations;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

constexpr int kPage = 0;
constexpr uint8_t kAll = SongPatternMaterializer::kEditableTrackMask;

using PhrasePatternLease::PatternLease;
using PhrasePatternLease::LeaseStatus;
using GroovePuterUndo::PhraseUndoPayload;
using GroovePuterUndo::UndoKind;
using GroovePuterUndo::UndoResult;

struct TinyUndoPayload {
  uint32_t value{0};
};

void resetRuntime(Scene& scene) {
  PhraseCore::reset(scene.phraseBank);
  GroovePuterUndo::undoOwner().clear();
  GroovePuterState::restoreSceneRevision({100, 100});
}

PatternAddress addressOf(int globalPattern) {
  const PatternAddress address = patternAddressFromGlobal(globalPattern);
  assert(address.valid());
  assert(address.page == kPage);
  return address;
}

void writeCandidate(Scene& scene,
                    int globalPattern,
                    uint8_t trackMask,
                    int token) {
  const PatternAddress address = addressOf(globalPattern);
  if ((trackMask & SongPatternMaterializer::kSynthAMask) != 0) {
    scene.synthABanks[address.bank].patterns[address.slot].steps[0].note =
        static_cast<int8_t>(36 + token);
  }
  if ((trackMask & SongPatternMaterializer::kSynthBMask) != 0) {
    scene.synthBBanks[address.bank].patterns[address.slot].steps[0].note =
        static_cast<int8_t>(48 + token);
  }
  if ((trackMask & SongPatternMaterializer::kDrumsMask) != 0) {
    scene.drumBanks[address.bank].patterns[address.slot]
        .voices[0].steps[0].hit = 1;
    scene.drumBanks[address.bank].patterns[address.slot]
        .voices[0].steps[0].velocity = static_cast<uint8_t>(80 + token);
  }
}

bool trackBackingPresent(const Scene& scene,
                         int globalPattern,
                         uint8_t trackBit) {
  const PatternAddress address = addressOf(globalPattern);
  const int local = address.bank * Bank<SynthPattern>::kPatterns + address.slot;
  return !SongPatternMaterializer::slotContentIsEmpty(
      scene,
      SongPatternMaterializer::editableTrackForIndex(
          trackBit == SongPatternMaterializer::kSynthAMask ? 0
          : trackBit == SongPatternMaterializer::kSynthBMask ? 1 : 2),
      local);
}

bool allBackingPresent(const Scene& scene, int globalPattern) {
  return trackBackingPresent(
             scene, globalPattern, SongPatternMaterializer::kSynthAMask) &&
         trackBackingPresent(
             scene, globalPattern, SongPatternMaterializer::kSynthBMask) &&
         trackBackingPresent(
             scene, globalPattern, SongPatternMaterializer::kDrumsMask);
}

bool allBackingCleared(const Scene& scene, int globalPattern) {
  return !trackBackingPresent(
              scene, globalPattern, SongPatternMaterializer::kSynthAMask) &&
         !trackBackingPresent(
              scene, globalPattern, SongPatternMaterializer::kSynthBMask) &&
         !trackBackingPresent(
              scene, globalPattern, SongPatternMaterializer::kDrumsMask);
}

UndoResult togglePhrase(Scene& scene) {
  return GroovePuterUndo::undoOwner().togglePrepared<PhraseUndoPayload>(
      UndoKind::Phrase,
      [](const PhraseUndoPayload& receipt) {
        return receipt.pageIndex == kPage;
      },
      [&](PhraseUndoPayload& receipt) {
        GroovePuterUndo::exchangeFixedValue(scene.phraseBank, receipt.before);
      });
}

PatternLease acquireCandidate(Scene& scene,
                              uint8_t bars,
                              int preferred = 0,
                              uint8_t trackMask = kAll) {
  PatternLease lease{};
  const auto result = PhrasePatternLease::patternLeaseOwner().acquire(
      scene, kPage, bars, trackMask, lease, preferred);
  assert(result);
  for (int bar = 0; bar < lease.count; ++bar) {
    writeCandidate(scene, lease.globalPattern[bar], lease.trackMask, bar + 1);
  }
  return lease;
}

void assertKeptSlot(const Scene& scene,
                    PhraseCore::SlotId slotId,
                    const int16_t* globals,
                    uint8_t bars,
                    uint8_t trackMask) {
  const PhraseCore::PhraseSlot* slot = PhraseCore::slotAt(scene.phraseBank, slotId);
  assert(slot != nullptr);
  assert(PhraseCore::isValid(*slot));
  assert(slot->metadata.source == PhraseCore::Source::Generated);
  assert(slot->metadata.storage == PhraseCore::StorageMode::ReferenceView);
  assert(slot->metadata.lengthBars == bars);
  assert(slot->metadata.trackMask == trackMask);
  for (int bar = 0; bar < bars; ++bar) {
    for (int track = 0; track < PhraseCore::kTrackCount; ++track) {
      const uint8_t bit = PhraseCore::maskForTrackIndex(track);
      assert(slot->patternRefs[bar][track] ==
             ((trackMask & bit) != 0 ? globals[bar] : -1));
    }
  }
}

void testKeepUndoRedoLengths() {
  for (uint8_t bars : {uint8_t{1}, uint8_t{2}, uint8_t{4}}) {
    Scene scene{};
    resetRuntime(scene);
    const PhraseCore::PhraseBank original = scene.phraseBank;
    PatternLease lease = acquireCandidate(scene, bars);
    int16_t globals[PhrasePatternLease::kMaxLeasePatterns] = {-1, -1, -1, -1};
    for (int bar = 0; bar < bars; ++bar) globals[bar] = lease.globalPattern[bar];

    const std::size_t heapBefore = gHeapAllocations;
    const auto kept = PhraseKeep::keep(
        scene, kPage, lease,
        {PhraseCore::SlotId::A, PhraseCore::Role::Main});
    assert(kept);
    assert(gHeapAllocations == heapBefore);
    assert(!lease.isActive());
    assert(PhrasePatternLease::patternLeaseOwner().activeLeaseCount() == 0);
    assert(GroovePuterUndo::undoOwner().kind() == UndoKind::Phrase);
    assert(GroovePuterUndo::undoOwner().hasLifecycle());
    assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == 101);
    assertKeptSlot(scene, PhraseCore::SlotId::A, globals, bars, kAll);

    const PhraseCore::PhraseBank keptBank = scene.phraseBank;
    assert(togglePhrase(scene) == UndoResult::Restored);
    assert(std::memcmp(&scene.phraseBank, &original, sizeof(original)) == 0);
    for (int bar = 0; bar < bars; ++bar) {
      assert(allBackingPresent(scene, globals[bar]));
      assert(GroovePuterUndo::undoOwner().retainsPatternBacking(
          globals[bar], kAll));
    }

    assert(togglePhrase(scene) == UndoResult::Restored);
    assert(std::memcmp(&scene.phraseBank, &keptBank, sizeof(keptBank)) == 0);
    assertKeptSlot(scene, PhraseCore::SlotId::A, globals, bars, kAll);

    GroovePuterUndo::undoOwner().clear();
    for (int bar = 0; bar < bars; ++bar) {
      assert(allBackingPresent(scene, globals[bar]));
    }
  }
}

void testSupersededRedoBackingIsReclaimed() {
  Scene scene{};
  resetRuntime(scene);
  PatternLease lease = acquireCandidate(scene, 1);
  const int global = lease.globalPattern[0];
  assert(PhraseKeep::keep(scene, kPage, lease, {}));
  assert(togglePhrase(scene) == UndoResult::Restored);
  assert(allBackingPresent(scene, global));

  TinyUndoPayload before{7};
  assert(GroovePuterUndo::undoOwner().commitPrepared(
      UndoKind::Pattern, before, []() {}));
  assert(allBackingCleared(scene, global));
  GroovePuterUndo::undoOwner().clear();
}

void testSharedReferenceProtectsBacking() {
  Scene scene{};
  resetRuntime(scene);
  PatternLease lease = acquireCandidate(scene, 1);
  const int global = lease.globalPattern[0];
  assert(PhraseKeep::keep(scene, kPage, lease, {}));
  assert(togglePhrase(scene) == UndoResult::Restored);

  scene.songs[0].positions[0]
      .patterns[static_cast<int>(SongTrack::SynthA)] = global;
  TinyUndoPayload before{9};
  assert(GroovePuterUndo::undoOwner().commitPrepared(
      UndoKind::Pattern, before, []() {}));
  assert(trackBackingPresent(
      scene, global, SongPatternMaterializer::kSynthAMask));
  // Unshared B/Drums are released when redo ownership is superseded.
  assert(!trackBackingPresent(
      scene, global, SongPatternMaterializer::kSynthBMask));
  assert(!trackBackingPresent(
      scene, global, SongPatternMaterializer::kDrumsMask));
  GroovePuterUndo::undoOwner().clear();
}

void testNewKeepReplacesRetainedPair() {
  Scene scene{};
  resetRuntime(scene);
  PatternLease first = acquireCandidate(scene, 1, 0);
  const int oldGlobal = first.globalPattern[0];
  assert(PhraseKeep::keep(scene, kPage, first, {}));
  assert(togglePhrase(scene) == UndoResult::Restored);

  PatternLease second = acquireCandidate(scene, 1, 0);
  const int newGlobal = second.globalPattern[0];
  assert(newGlobal != oldGlobal);
  assert(PhraseKeep::keep(scene, kPage, second, {}));
  assert(allBackingCleared(scene, oldGlobal));
  assert(allBackingPresent(scene, newGlobal));

  assert(togglePhrase(scene) == UndoResult::Restored);
  assert(!PhraseCore::isValid(scene.phraseBank.slots[0]));
  assert(allBackingPresent(scene, newGlobal));
  assert(togglePhrase(scene) == UndoResult::Restored);
  const int16_t expected[1] = {static_cast<int16_t>(newGlobal)};
  assertKeptSlot(scene, PhraseCore::SlotId::A, expected, 1, kAll);
  GroovePuterUndo::undoOwner().clear();
}

void testDiscardBeforeKeepCreatesNoUndo() {
  Scene scene{};
  resetRuntime(scene);
  PatternLease lease = acquireCandidate(scene, 1);
  assert(PhrasePatternLease::patternLeaseOwner().discard(
             scene, kPage, lease) == LeaseStatus::Ok);
  assert(!GroovePuterUndo::undoOwner().hasUndo());
}

void testFailedKeepLeavesLeaseActive() {
  Scene scene{};
  resetRuntime(scene);
  PatternLease lease = acquireCandidate(scene, 1);
  const int global = lease.globalPattern[0];
  scene.songs[0].positions[0]
      .patterns[static_cast<int>(SongTrack::SynthA)] = global;

  const auto result = PhraseKeep::keep(scene, kPage, lease, {});
  assert(!result);
  assert(result.status == PhraseKeep::Status::TransferPrepareFailed);
  assert(lease.isActive());
  assert(!GroovePuterUndo::undoOwner().hasUndo());

  scene.songs[0].positions[0]
      .patterns[static_cast<int>(SongTrack::SynthA)] = -1;
  assert(PhrasePatternLease::patternLeaseOwner().discard(
             scene, kPage, lease) == LeaseStatus::Ok);
}

void testPersistenceAndTransientSanitization() {
  Scene scene{};
  resetRuntime(scene);
  PatternLease lease = acquireCandidate(scene, 1);
  const int global = lease.globalPattern[0];
  assert(PhraseKeep::keep(scene, kPage, lease, {}));

  int32_t values[PhraseCore::kPersistValueCount]{};
  for (int i = 0; i < PhraseCore::kPersistValueCount; ++i) {
    values[i] = PhraseCore::persistentValueAt(scene.phraseBank, i);
  }
  PhraseCore::PhraseBank loaded{};
  PhraseCore::beginPersistentDecode(loaded);
  for (int i = 0; i < PhraseCore::kPersistValueCount; ++i) {
    assert(PhraseCore::applyPersistentValue(loaded, i, values[i]));
  }
  assert(std::memcmp(&loaded, &scene.phraseBank, sizeof(loaded)) == 0);

  // Accepted backing is live persistent material and must remain in a save view.
  Scene persistentView = scene;
  GroovePuterUndo::undoOwner().sanitizeForPersistence(&persistentView);
  assert(allBackingPresent(persistentView, global));

  // After Undo it is redo-only runtime backing: keep it live, omit it from save.
  assert(togglePhrase(scene) == UndoResult::Restored);
  persistentView = scene;
  GroovePuterUndo::undoOwner().sanitizeForPersistence(&persistentView);
  assert(allBackingPresent(scene, global));
  assert(allBackingCleared(persistentView, global));

  GroovePuterUndo::undoOwner().clear();
  assert(allBackingCleared(scene, global));
}

void testRetainedBackingBlocksAllocators() {
  Scene scene{};
  resetRuntime(scene);
  PatternLease lease = acquireCandidate(scene, 1);
  const int global = lease.globalPattern[0];
  const PatternAddress address = addressOf(global);
  const int local = address.bank * Bank<SynthPattern>::kPatterns + address.slot;
  assert(PhraseKeep::keep(scene, kPage, lease, {}));
  assert(togglePhrase(scene) == UndoResult::Restored);

  // Even an otherwise empty retained track is an owned resource to allocators.
  scene.synthABanks[address.bank].patterns[address.slot] = SynthPattern{};
  assert(SongPatternMaterializer::findSafeFreeLocalSlot(
             scene, kPage, SongTrack::SynthA, local) != local);
  PatternLease preview{};
  assert(PhrasePatternLease::patternLeaseOwner().acquire(
      scene, kPage, 1, SongPatternMaterializer::kSynthAMask,
      preview, local));
  assert(preview.globalPattern[0] != global);
  assert(PhrasePatternLease::patternLeaseOwner().discard(
             scene, kPage, preview) == LeaseStatus::Ok);

  GroovePuterUndo::undoOwner().clear();
}

}  // namespace

static_assert(sizeof(GroovePuterUndo::PhraseUndoPayload) <= 248,
              "P1b must not inflate Phrase payload with Pattern snapshots");
static_assert(GroovePuterUndo::UndoOwner::lifecyclePayloadCapacity() == 1424,
              "P1b lifecycle tail budget changed");
static_assert(sizeof(GroovePuterUndo::UndoLifecycleMetadata) <= 112,
              "P1b lifecycle metadata budget changed");
static_assert(sizeof(GroovePuterUndo::UndoOwner) <= 1552,
              "P1b canonical UndoOwner resident budget changed");

int main() {
  testKeepUndoRedoLengths();
  testSupersededRedoBackingIsReclaimed();
  testSharedReferenceProtectsBacking();
  testNewKeepReplacesRetainedPair();
  testDiscardBeforeKeepCreatesNoUndo();
  testFailedKeepLeavesLeaseActive();
  testPersistenceAndTransientSanitization();
  testRetainedBackingBlocksAllocators();
  std::puts("0.9.9-P1b Phrase KEEP / Undo backing ownership: PASS");
  return 0;
}
