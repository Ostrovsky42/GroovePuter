#pragma once

#include <cstdint>

#include "src/phrase/pattern_lease_owner.h"
#include "src/phrase/phrase_undo_backing.h"

namespace PhraseKeep {

enum class Status : uint8_t {
  Ok = 0,
  InvalidTarget,
  InvalidRole,
  InvalidLease,
  TransferPrepareFailed,
  UndoBackingCapacity,
  UndoAdmissionFailed,
  TransferCompletionDefect,
};

struct Request {
  PhraseCore::SlotId targetSlot = PhraseCore::SlotId::A;
  PhraseCore::Role role = PhraseCore::Role::Main;
};

struct Result {
  Status status = Status::Ok;
  PhrasePatternLease::LeaseStatus leaseStatus =
      PhrasePatternLease::LeaseStatus::Ok;
  uint16_t phraseId = PhraseCore::kNoPhraseId;

  explicit operator bool() const { return status == Status::Ok; }
};

inline bool buildPreparedPhraseBank(
    const PhraseCore::PhraseBank& before,
    const Request& request,
    const PhrasePatternLease::PatternLease& lease,
    PhraseCore::PhraseBank& after,
    uint16_t& phraseId) {
  if (PhraseCore::slotIndex(request.targetSlot) < 0 ||
      !PhraseCore::isValidRole(request.role) ||
      !lease.isActive() ||
      (lease.count != 1 && lease.count != 2 && lease.count != 4) ||
      !PhraseCore::isValidTrackMask(lease.trackMask)) {
    return false;
  }

  after = before;
  PhraseCore::PhraseSlot* destination =
      PhraseCore::slotAt(after, request.targetSlot);
  if (destination == nullptr) return false;

  PhraseCore::clearSlotValue(*destination);
  phraseId = PhraseCore::nextPhraseId(after);
  destination->metadata.phraseId = phraseId;
  destination->metadata.parentId = PhraseCore::kNoPhraseId;
  destination->metadata.lengthBars = lease.count;
  destination->metadata.role = request.role;
  destination->metadata.source = PhraseCore::Source::Generated;
  destination->metadata.storage = PhraseCore::StorageMode::ReferenceView;
  destination->metadata.flags =
      PhraseCore::kFlagValid | PhraseCore::kFlagReferenceView |
      PhraseCore::kFlagMutableBacking;
  destination->metadata.sourceSongSlot = 0;
  destination->metadata.sourceStartRow = 0;
  destination->metadata.trackMask = lease.trackMask;

  for (int bar = 0; bar < lease.count; ++bar) {
    for (int trackIndex = 0; trackIndex < PhraseCore::kTrackCount;
         ++trackIndex) {
      const uint8_t bit = PhraseCore::maskForTrackIndex(trackIndex);
      if ((lease.trackMask & bit) != 0) {
        destination->patternRefs[bar][trackIndex] = lease.globalPattern[bar];
      }
    }
  }

  after.nextPhraseId = PhraseCore::incrementPhraseId(phraseId);
  after.version = PhraseCore::kPersistenceVersion;
  return true;
}

// Scene-level production core used by permanent host tests and future callers
// that already own the active page identity. No UI or playback state is touched.
inline Result keep(Scene& scene,
                   int pageIndex,
                   PhrasePatternLease::PatternLease& lease,
                   const Request& request) {
  Result output{};
  if (PhraseCore::slotIndex(request.targetSlot) < 0) {
    output.status = Status::InvalidTarget;
    return output;
  }
  if (!PhraseCore::isValidRole(request.role)) {
    output.status = Status::InvalidRole;
    return output;
  }
  if (!lease.isActive()) {
    output.status = Status::InvalidLease;
    output.leaseStatus = PhrasePatternLease::LeaseStatus::InvalidLease;
    return output;
  }

  auto& leaseOwner = PhrasePatternLease::patternLeaseOwner();
  PhrasePatternLease::PreparedPersistentTransfer preparedTransfer{};
  output.leaseStatus = leaseOwner.preparePersistentTransfer(
      scene, pageIndex, lease, preparedTransfer);
  if (output.leaseStatus != PhrasePatternLease::LeaseStatus::Ok) {
    output.status = Status::TransferPrepareFailed;
    return output;
  }

  GroovePuterUndo::PhraseUndoPayload before{};
  GroovePuterUndo::UndoLifecycleMetadata lifecycle{};
  if (!PhraseUndoBacking::capturePhraseUndo(
          scene, pageIndex, before, lifecycle)) {
    output.status = Status::UndoBackingCapacity;
    return output;
  }

  PhraseCore::PhraseBank after{};
  if (!buildPreparedPhraseBank(
          before.before, request, preparedTransfer.lease, after,
          output.phraseId)) {
    output.status = Status::InvalidLease;
    return output;
  }

  for (int bar = 0; bar < preparedTransfer.lease.count; ++bar) {
    if (!PhraseUndoBacking::addPatternBacking(
            lifecycle,
            preparedTransfer.lease.globalPattern[bar],
            preparedTransfer.lease.trackMask)) {
      output.status = Status::UndoBackingCapacity;
      return output;
    }
  }

  PhrasePatternLease::LeaseStatus completion =
      PhrasePatternLease::LeaseStatus::InvalidTransfer;
  const bool committed = PhraseUndoBacking::commitPhrasePrepared(
      before, lifecycle, [&]() {
        scene.phraseBank = after;
        completion = leaseOwner.completePersistentTransfer(
            lease, preparedTransfer);
      });

  if (!committed) {
    output.status = Status::UndoAdmissionFailed;
    return output;
  }
  if (completion != PhrasePatternLease::LeaseStatus::Ok) {
    output.status = Status::TransferCompletionDefect;
    output.leaseStatus = completion;
    return output;
  }

  output.status = Status::Ok;
  output.leaseStatus = PhrasePatternLease::LeaseStatus::Ok;
  return output;
}

inline Result keep(SceneManager& manager,
                   PhrasePatternLease::PatternLease& lease,
                   const Request& request) {
  return keep(
      manager.currentScene(), manager.currentPageIndex(), lease, request);
}

inline const char* statusText(Status status) {
  switch (status) {
    case Status::Ok: return "OK";
    case Status::InvalidTarget: return "Invalid Phrase slot";
    case Status::InvalidRole: return "Invalid Phrase role";
    case Status::InvalidLease: return "Invalid audition lease";
    case Status::TransferPrepareFailed: return "Lease transfer prepare failed";
    case Status::UndoBackingCapacity: return "Phrase Undo backing receipt full";
    case Status::UndoAdmissionFailed: return "Phrase Undo admission failed";
    case Status::TransferCompletionDefect: return "Lease transfer contract defect";
  }
  return "Phrase KEEP error";
}

}  // namespace PhraseKeep
