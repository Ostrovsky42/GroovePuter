#pragma once

#include <cstdint>

#include "src/dsp/song_pattern_materializer.h"
#include "src/pattern/pattern_address.h"
#include "src/phrase/pattern_lease_owner.h"
#include "src/phrase/phrase_core.h"
#include "src/state/undo_receipts.h"

namespace PhraseUndoBacking {

inline bool addPatternBacking(GroovePuterUndo::UndoLifecycleMetadata& lifecycle,
                              int globalPattern,
                              uint8_t trackMask) {
  if (globalPattern < 0 || globalPattern >= kMaxGlobalPatterns ||
      !PhraseCore::isValidTrackMask(trackMask)) {
    return false;
  }

  for (int i = 0; i < lifecycle.count; ++i) {
    auto& resource = lifecycle.resources[i];
    if (resource.kind ==
            GroovePuterUndo::UndoRetainedResourceKind::PatternBacking &&
        resource.resourceId == globalPattern) {
      resource.mask = static_cast<uint8_t>(resource.mask | trackMask);
      return true;
    }
  }

  if (lifecycle.count >= GroovePuterUndo::kUndoRetainedResourceCapacity) {
    return false;
  }
  auto& resource = lifecycle.resources[lifecycle.count++];
  resource.resourceId = static_cast<int16_t>(globalPattern);
  resource.mask = trackMask;
  resource.kind = GroovePuterUndo::UndoRetainedResourceKind::PatternBacking;
  return true;
}

inline bool addGeneratedPhraseBacking(
    GroovePuterUndo::UndoLifecycleMetadata& lifecycle,
    const PhraseCore::PhraseBank& bank) {
  for (int slotIndex = 0; slotIndex < PhraseCore::kSlotCount; ++slotIndex) {
    const PhraseCore::PhraseSlot& slot = bank.slots[slotIndex];
    if (!PhraseCore::isValid(slot) ||
        slot.metadata.source != PhraseCore::Source::Generated ||
        slot.metadata.storage != PhraseCore::StorageMode::ReferenceView ||
        (slot.metadata.flags & PhraseCore::kFlagMutableBacking) == 0) {
      continue;
    }

    // P1b owns only the 1/2/4-bar audition KEEP contract. Refuse to silently
    // widen retained ownership if a future producer gives Generated a new shape.
    if (slot.metadata.lengthBars != 1 && slot.metadata.lengthBars != 2 &&
        slot.metadata.lengthBars != 4) {
      return false;
    }

    for (int bar = 0; bar < slot.metadata.lengthBars; ++bar) {
      for (int trackIndex = 0; trackIndex < PhraseCore::kTrackCount;
           ++trackIndex) {
        const uint8_t bit = PhraseCore::maskForTrackIndex(trackIndex);
        if ((slot.metadata.trackMask & bit) == 0) continue;
        const int globalPattern = slot.patternRefs[bar][trackIndex];
        if (globalPattern < 0 ||
            !addPatternBacking(lifecycle, globalPattern, bit)) {
          return false;
        }
      }
    }
  }
  return true;
}

inline void clearBackingTrack(Scene& scene,
                              const PatternAddress& address,
                              uint8_t trackBit) {
  if (trackBit == SongPatternMaterializer::kSynthAMask) {
    scene.synthABanks[address.bank].patterns[address.slot] = SynthPattern{};
  } else if (trackBit == SongPatternMaterializer::kSynthBMask) {
    scene.synthBBanks[address.bank].patterns[address.slot] = SynthPattern{};
  } else if (trackBit == SongPatternMaterializer::kDrumsMask) {
    scene.drumBanks[address.bank].patterns[address.slot] = DrumPatternSet{};
  }
}

inline void cleanupLifecycle(
    void* context,
    const GroovePuterUndo::UndoLifecycleMetadata& lifecycle) {
  auto* manager = static_cast<SceneManager*>(context);
  if (manager == nullptr) return;
  Scene& scene = manager->currentScene();
  const int currentPage = manager->currentPageIndex();

  for (int i = 0; i < lifecycle.count; ++i) {
    const auto& resource = lifecycle.resources[i];
    if (resource.kind !=
            GroovePuterUndo::UndoRetainedResourceKind::PatternBacking) {
      continue;
    }
    const PatternAddress address =
        patternAddressFromGlobal(resource.resourceId);
    if (!address.valid() || address.page != currentPage) continue;

    for (int trackIndex = 0;
         trackIndex < SongPatternMaterializer::kEditableTrackCount;
         ++trackIndex) {
      const uint8_t bit = static_cast<uint8_t>(1u << trackIndex);
      if ((resource.mask & bit) == 0) continue;
      const SongTrack track =
          SongPatternMaterializer::editableTrackForIndex(trackIndex);

      // Persistent Song/Phrase refs win. The newly-published canonical receipt
      // and an active temporary lease are also authoritative owners.
      if (SongPatternMaterializer::persistentGlobalPatternReferenceCount(
              scene, track, resource.resourceId) > 0 ||
          GroovePuterUndo::undoOwner().retainsPatternBacking(
              resource.resourceId, bit) ||
          PhrasePatternLease::patternLeaseOwner().isLeased(
              resource.resourceId, bit)) {
        continue;
      }
      clearBackingTrack(scene, address, bit);
    }
  }
}

inline void sanitizeLifecycleForPersistence(
    void* context,
    const GroovePuterUndo::UndoLifecycleMetadata& lifecycle,
    void* persistenceView) {
  auto* manager = static_cast<SceneManager*>(context);
  auto* target = static_cast<Scene*>(persistenceView);
  if (manager == nullptr || target == nullptr) return;
  const Scene& live = manager->currentScene();
  const int currentPage = manager->currentPageIndex();

  for (int i = 0; i < lifecycle.count; ++i) {
    const auto& resource = lifecycle.resources[i];
    if (resource.kind !=
            GroovePuterUndo::UndoRetainedResourceKind::PatternBacking) {
      continue;
    }
    const PatternAddress address =
        patternAddressFromGlobal(resource.resourceId);
    if (!address.valid() || address.page != currentPage) continue;

    for (int trackIndex = 0;
         trackIndex < SongPatternMaterializer::kEditableTrackCount;
         ++trackIndex) {
      const uint8_t bit = static_cast<uint8_t>(1u << trackIndex);
      if ((resource.mask & bit) == 0) continue;
      const SongTrack track =
          SongPatternMaterializer::editableTrackForIndex(trackIndex);
      if (SongPatternMaterializer::persistentGlobalPatternReferenceCount(
              live, track, resource.resourceId) == 0) {
        // Redo-only backing is runtime state. It remains live for Ctrl+Z
        // exchange, but must not enter a raw Pattern-page persistence image.
        clearBackingTrack(*target, address, bit);
      }
    }
  }
}

inline GroovePuterUndo::UndoLifecycleMetadata emptyLifecycle(
    SceneManager& manager) {
  GroovePuterUndo::UndoLifecycleMetadata lifecycle{};
  lifecycle.context = &manager;
  lifecycle.cleanup = cleanupLifecycle;
  lifecycle.sanitizeForPersistence = sanitizeLifecycleForPersistence;
  return lifecycle;
}

inline bool captureCurrentPhraseUndo(
    SceneManager& manager,
    GroovePuterUndo::PhraseUndoPayload& before,
    GroovePuterUndo::UndoLifecycleMetadata& lifecycle) {
  const int page = manager.currentPageIndex();
  if (page < 0 || page >= kMaxPages) return false;

  before = GroovePuterUndo::PhraseUndoPayload{};
  before.pageIndex = static_cast<uint8_t>(page);
  before.before = manager.currentScene().phraseBank;
  lifecycle = emptyLifecycle(manager);
  return addGeneratedPhraseBacking(lifecycle, before.before);
}

template <typename ApplyFn>
bool commitPhrasePrepared(
    const GroovePuterUndo::PhraseUndoPayload& before,
    const GroovePuterUndo::UndoLifecycleMetadata& lifecycle,
    ApplyFn&& apply) {
  auto& owner = GroovePuterUndo::undoOwner();
  if (lifecycle.count == 0) {
    return owner.commitPrepared(
        GroovePuterUndo::UndoKind::Phrase,
        before,
        static_cast<ApplyFn&&>(apply));
  }
  return owner.commitPreparedWithLifecycle(
      GroovePuterUndo::UndoKind::Phrase,
      before,
      lifecycle,
      static_cast<ApplyFn&&>(apply));
}

}  // namespace PhraseUndoBacking
