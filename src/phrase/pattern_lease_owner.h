#pragma once

#include <cstdint>
#include <type_traits>

#include "src/dsp/song_pattern_materializer.h"
#include "src/pattern/pattern_address.h"

namespace PhrasePatternLease {

constexpr uint8_t kMaxLeasePatterns = 4;
constexpr uint8_t kLeaseOwnerCapacity = 2;
constexpr uint8_t kInvalidOwnerSlot = 0xFFu;
constexpr uint8_t kInvalidPage = 0xFFu;

static_assert(SongPatternMaterializer::kSynthAMask == PhraseCore::kTrackSynthA,
              "Pattern lease track-mask contract drifted for Synth A");
static_assert(SongPatternMaterializer::kSynthBMask == PhraseCore::kTrackSynthB,
              "Pattern lease track-mask contract drifted for Synth B");
static_assert(SongPatternMaterializer::kDrumsMask == PhraseCore::kTrackDrums,
              "Pattern lease track-mask contract drifted for Drums");
static_assert(SongPatternMaterializer::kEditableTrackMask ==
                  PhraseCore::kAllTracks,
              "Pattern lease all-track mask contract drifted");

enum class LeaseStatus : uint8_t {
  Ok = 0,
  UnsupportedLength,
  InvalidPage,
  InvalidTrackMask,
  OwnerFull,
  Exhausted,
  InvalidLease,
  ShapeMismatch,
  PageMismatch,
  PersistentReference,
  InvalidTransfer,
};

struct PatternLease {
  int16_t globalPattern[kMaxLeasePatterns] = {-1, -1, -1, -1};
  uint8_t count = 0;
  uint8_t pageIndex = kInvalidPage;
  uint8_t trackMask = 0;
  uint8_t ownerSlot = kInvalidOwnerSlot;
  uint8_t generation = 0;
  uint8_t active = 0;

  bool isActive() const { return active != 0; }
};

struct PreparedPersistentTransfer {
  PatternLease lease{};

  bool isPrepared() const { return lease.isActive(); }
};

struct AcquireResult {
  LeaseStatus status = LeaseStatus::Ok;
  uint8_t reused = 0;

  explicit operator bool() const { return status == LeaseStatus::Ok; }
  bool reusedExistingLease() const { return reused != 0; }
};

class PatternLeaseOwner {
 public:
  // P1a compatibility: Phrase audition owns all aligned editable tracks.
  AcquireResult acquire(const Scene& scene,
                        int pageIndex,
                        uint8_t count,
                        PatternLease& lease,
                        int preferredLocalSlot = 0) {
    return acquire(scene,
                   pageIndex,
                   count,
                   SongPatternMaterializer::kEditableTrackMask,
                   lease,
                   preferredLocalSlot);
  }

  AcquireResult acquire(const Scene& scene,
                        int pageIndex,
                        uint8_t count,
                        uint8_t trackMask,
                        PatternLease& lease,
                        int preferredLocalSlot = 0) {
    if (!isSupportedLeaseCount(count)) {
      return {LeaseStatus::UnsupportedLength, 0};
    }
    if (pageIndex < 0 || pageIndex >= kMaxPages) {
      return {LeaseStatus::InvalidPage, 0};
    }
    if (!isValidTrackMask(trackMask)) {
      return {LeaseStatus::InvalidTrackMask, 0};
    }

    if (lease.isActive()) {
      const int ownerSlot = validateActiveLease(lease);
      if (ownerSlot < 0) return {LeaseStatus::InvalidLease, 0};
      const PatternLease& current = records_[ownerSlot];
      if (current.pageIndex != pageIndex || current.count != count ||
          current.trackMask != trackMask) {
        return {LeaseStatus::ShapeMismatch, 0};
      }
      for (int i = 0; i < current.count; ++i) {
        if (requestedTracksAreReferenced(
                scene, current.globalPattern[i], current.trackMask)) {
          return {LeaseStatus::PersistentReference, 0};
        }
      }
      lease = current;
      return {LeaseStatus::Ok, 1};
    }

    const int ownerSlot = findFreeOwnerSlot();
    if (ownerSlot < 0) return {LeaseStatus::OwnerFull, 0};

    PatternLease candidate{};
    candidate.count = count;
    candidate.pageIndex = static_cast<uint8_t>(pageIndex);
    candidate.trackMask = trackMask;

    int start = preferredLocalSlot;
    if (start < 0 || start >= kPatternsPerPage) start = 0;
    int found = 0;
    for (int offset = 0; offset < kPatternsPerPage && found < count; ++offset) {
      const int localSlot = (start + offset) % kPatternsPerPage;
      const int bank = localSlot / Bank<SynthPattern>::kPatterns;
      const int index = localSlot % Bank<SynthPattern>::kPatterns;
      const int globalPattern = songPatternFromPageBankIndex(
          pageIndex, bank, index);
      if (isLeased(globalPattern, trackMask)) continue;
      if (!localSlotIsSafeForTrackMask(
              scene, pageIndex, localSlot, trackMask)) {
        continue;
      }
      candidate.globalPattern[found++] =
          static_cast<int16_t>(globalPattern);
    }

    if (found != count) return {LeaseStatus::Exhausted, 0};

    candidate.ownerSlot = static_cast<uint8_t>(ownerSlot);
    candidate.generation = nextGeneration(records_[ownerSlot].generation);
    candidate.active = 1;
    records_[ownerSlot] = candidate;
    lease = candidate;
    return {LeaseStatus::Ok, 0};
  }

  LeaseStatus discard(Scene& scene,
                      int currentPageIndex,
                      PatternLease& lease) {
    const int ownerSlot = validateActiveLease(lease);
    if (ownerSlot < 0) return LeaseStatus::InvalidLease;
    const PatternLease& current = records_[ownerSlot];
    if (currentPageIndex != current.pageIndex) {
      return LeaseStatus::PageMismatch;
    }

    PatternAddress addresses[kMaxLeasePatterns]{};
    for (int i = 0; i < current.count; ++i) {
      addresses[i] = patternAddressFromGlobal(current.globalPattern[i]);
      if (!addresses[i].valid() || addresses[i].page != current.pageIndex) {
        return LeaseStatus::InvalidLease;
      }
      if (requestedTracksAreReferenced(
              scene, current.globalPattern[i], current.trackMask)) {
        return LeaseStatus::PersistentReference;
      }
    }

    for (int i = 0; i < current.count; ++i) {
      clearOwnedTracks(scene, addresses[i], current.trackMask);
    }

    deactivate(ownerSlot);
    lease = PatternLease{};
    return LeaseStatus::Ok;
  }

  LeaseStatus preparePersistentTransfer(
      const Scene& scene,
      int currentPageIndex,
      const PatternLease& lease,
      PreparedPersistentTransfer& prepared) const {
    const int ownerSlot = validateActiveLease(lease);
    if (ownerSlot < 0) return LeaseStatus::InvalidLease;
    const PatternLease& current = records_[ownerSlot];
    if (currentPageIndex != current.pageIndex) {
      return LeaseStatus::PageMismatch;
    }

    for (int i = 0; i < current.count; ++i) {
      const PatternAddress address =
          patternAddressFromGlobal(current.globalPattern[i]);
      if (!address.valid() || address.page != current.pageIndex) {
        return LeaseStatus::InvalidLease;
      }
      // prepare must precede the canonical persistent mutation. A requested
      // track that is already referenced means the transaction is out of order.
      if (requestedTracksAreReferenced(
              scene, current.globalPattern[i], current.trackMask)) {
        return LeaseStatus::PersistentReference;
      }
    }

    PreparedPersistentTransfer candidate{};
    candidate.lease = current;
    prepared = candidate;
    return LeaseStatus::Ok;
  }

  LeaseStatus completePersistentTransfer(
      PatternLease& lease,
      const PreparedPersistentTransfer& prepared) {
    if (!prepared.isPrepared()) return LeaseStatus::InvalidTransfer;

    const int ownerSlot = validateActiveLease(lease);
    if (ownerSlot < 0) return LeaseStatus::InvalidLease;
    if (!sameLease(records_[ownerSlot], prepared.lease)) {
      return LeaseStatus::InvalidTransfer;
    }

    // Deliberately no Scene/persistence validation here. Once prepare has
    // succeeded and the canonical owner reports commit success, completion is
    // bookkeeping-only and cannot fail because of post-commit Scene state.
    deactivate(ownerSlot);
    lease = PatternLease{};
    return LeaseStatus::Ok;
  }

  bool isLeased(int globalPattern) const {
    if (globalPattern < 0 || globalPattern >= kMaxGlobalPatterns) return false;
    for (int ownerSlot = 0; ownerSlot < kLeaseOwnerCapacity; ++ownerSlot) {
      const PatternLease& lease = records_[ownerSlot];
      if (!lease.isActive()) continue;
      for (int i = 0; i < lease.count; ++i) {
        if (lease.globalPattern[i] == globalPattern) return true;
      }
    }
    return false;
  }

  bool isLeased(int globalPattern, uint8_t trackMask) const {
    if (globalPattern < 0 || globalPattern >= kMaxGlobalPatterns ||
        !isValidTrackMask(trackMask)) {
      return false;
    }
    for (int ownerSlot = 0; ownerSlot < kLeaseOwnerCapacity; ++ownerSlot) {
      const PatternLease& lease = records_[ownerSlot];
      if (!lease.isActive() || (lease.trackMask & trackMask) == 0) continue;
      for (int i = 0; i < lease.count; ++i) {
        if (lease.globalPattern[i] == globalPattern) return true;
      }
    }
    return false;
  }

  uint8_t activeLeaseCount() const {
    uint8_t count = 0;
    for (int ownerSlot = 0; ownerSlot < kLeaseOwnerCapacity; ++ownerSlot) {
      if (records_[ownerSlot].isActive()) ++count;
    }
    return count;
  }

 private:
  static bool isSupportedLeaseCount(uint8_t count) {
    return count == 1 || count == 2 || count == 4;
  }

  static bool isValidTrackMask(uint8_t trackMask) {
    return PhraseCore::isValidTrackMask(trackMask) &&
           (trackMask & ~SongPatternMaterializer::kEditableTrackMask) == 0;
  }

  static uint8_t nextGeneration(uint8_t current) {
    ++current;
    return current == 0 ? 1 : current;
  }

  static bool requestedTracksAreReferenced(const Scene& scene,
                                           int globalPattern,
                                           uint8_t trackMask) {
    for (int trackIndex = 0;
         trackIndex < SongPatternMaterializer::kEditableTrackCount;
         ++trackIndex) {
      const uint8_t trackBit = static_cast<uint8_t>(1u << trackIndex);
      if ((trackMask & trackBit) == 0) continue;
      const SongTrack track =
          SongPatternMaterializer::editableTrackForIndex(trackIndex);
      if (SongPatternMaterializer::globalPatternIsReferenced(
              scene, track, globalPattern)) {
        return true;
      }
    }
    return false;
  }

  static bool localSlotIsSafeForTrackMask(const Scene& scene,
                                          int pageIndex,
                                          int localSlot,
                                          uint8_t trackMask) {
    if (pageIndex < 0 || pageIndex >= kMaxPages ||
        localSlot < 0 || localSlot >= kPatternsPerPage ||
        !isValidTrackMask(trackMask)) {
      return false;
    }

    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    const int globalPattern = songPatternFromPageBankIndex(
        pageIndex, bank, index);

    for (int trackIndex = 0;
         trackIndex < SongPatternMaterializer::kEditableTrackCount;
         ++trackIndex) {
      const uint8_t trackBit = static_cast<uint8_t>(1u << trackIndex);
      if ((trackMask & trackBit) == 0) continue;
      const SongTrack track =
          SongPatternMaterializer::editableTrackForIndex(trackIndex);
      if (!SongPatternMaterializer::slotContentIsEmpty(
              scene, track, localSlot) ||
          SongPatternMaterializer::globalPatternIsReferenced(
              scene, track, globalPattern)) {
        return false;
      }
    }
    return true;
  }

  static void clearOwnedTracks(Scene& scene,
                               const PatternAddress& address,
                               uint8_t trackMask) {
    if ((trackMask & SongPatternMaterializer::kSynthAMask) != 0) {
      scene.synthABanks[address.bank].patterns[address.slot] = SynthPattern{};
    }
    if ((trackMask & SongPatternMaterializer::kSynthBMask) != 0) {
      scene.synthBBanks[address.bank].patterns[address.slot] = SynthPattern{};
    }
    if ((trackMask & SongPatternMaterializer::kDrumsMask) != 0) {
      scene.drumBanks[address.bank].patterns[address.slot] = DrumPatternSet{};
    }
  }

  static bool sameLease(const PatternLease& left,
                        const PatternLease& right) {
    if (left.count != right.count || left.pageIndex != right.pageIndex ||
        left.trackMask != right.trackMask || left.ownerSlot != right.ownerSlot ||
        left.generation != right.generation || left.active != right.active) {
      return false;
    }
    for (int i = 0; i < kMaxLeasePatterns; ++i) {
      if (left.globalPattern[i] != right.globalPattern[i]) return false;
    }
    return true;
  }

  int findFreeOwnerSlot() const {
    for (int ownerSlot = 0; ownerSlot < kLeaseOwnerCapacity; ++ownerSlot) {
      if (!records_[ownerSlot].isActive()) return ownerSlot;
    }
    return -1;
  }

  int validateActiveLease(const PatternLease& lease) const {
    if (!lease.isActive() || lease.ownerSlot >= kLeaseOwnerCapacity ||
        !isSupportedLeaseCount(lease.count) || lease.pageIndex >= kMaxPages ||
        !isValidTrackMask(lease.trackMask)) {
      return -1;
    }
    const PatternLease& current = records_[lease.ownerSlot];
    return sameLease(current, lease) ? lease.ownerSlot : -1;
  }

  void deactivate(int ownerSlot) {
    const uint8_t generation = records_[ownerSlot].generation;
    records_[ownerSlot] = PatternLease{};
    records_[ownerSlot].generation = generation;
  }

  PatternLease records_[kLeaseOwnerCapacity]{};
};

inline PatternLeaseOwner& patternLeaseOwner() {
  static PatternLeaseOwner owner{};
  return owner;
}

inline const char* statusText(LeaseStatus status) {
  switch (status) {
    case LeaseStatus::Ok: return "OK";
    case LeaseStatus::UnsupportedLength: return "Use 1/2/4 bars";
    case LeaseStatus::InvalidPage: return "Pattern page unavailable";
    case LeaseStatus::InvalidTrackMask: return "Pattern track mask unavailable";
    case LeaseStatus::OwnerFull: return "Lease owner busy";
    case LeaseStatus::Exhausted: return "No safe pattern addresses";
    case LeaseStatus::InvalidLease: return "Invalid pattern lease";
    case LeaseStatus::ShapeMismatch: return "Active lease shape changed";
    case LeaseStatus::PageMismatch: return "Return to leased pattern page";
    case LeaseStatus::PersistentReference: return "Lease became referenced";
    case LeaseStatus::InvalidTransfer: return "Invalid prepared transfer";
  }
  return "Pattern lease error";
}

static_assert(sizeof(PatternLease) == 14,
              "P1a2 PatternLease fixed budget changed");
static_assert(sizeof(PreparedPersistentTransfer) == 14,
              "P1a2 transfer token fixed budget changed");
static_assert(sizeof(PatternLeaseOwner) == 28,
              "P1a2 PatternLeaseOwner fixed budget changed");
static_assert(std::is_trivially_copyable<PatternLease>::value,
              "P1a2 PatternLease must remain a fixed value");
static_assert(std::is_trivially_copyable<PreparedPersistentTransfer>::value,
              "P1a2 transfer token must remain a fixed value");
static_assert(std::is_trivially_copyable<PatternLeaseOwner>::value,
              "P1a2 PatternLeaseOwner must remain fixed-capacity state");

}  // namespace PhrasePatternLease
