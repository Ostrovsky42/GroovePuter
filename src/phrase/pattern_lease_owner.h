#pragma once

#include <cstdint>
#include <type_traits>

#include "src/dsp/phrase_generator.h"
#include "src/pattern/pattern_address.h"

namespace PhrasePatternLease {

constexpr uint8_t kMaxLeasePatterns = 4;
constexpr uint8_t kLeaseOwnerCapacity = 2;
constexpr uint8_t kInvalidOwnerSlot = 0xFFu;
constexpr uint8_t kInvalidPage = 0xFFu;

enum class LeaseStatus : uint8_t {
  Ok = 0,
  UnsupportedLength,
  InvalidPage,
  OwnerFull,
  Exhausted,
  InvalidLease,
  ShapeMismatch,
  PageMismatch,
  PersistentReference,
  IncompletePersistentOwnership,
};

struct PatternLease {
  int16_t globalPattern[kMaxLeasePatterns] = {-1, -1, -1, -1};
  uint8_t count = 0;
  uint8_t pageIndex = kInvalidPage;
  uint8_t ownerSlot = kInvalidOwnerSlot;
  uint8_t generation = 0;
  uint8_t active = 0;

  bool isActive() const { return active != 0; }
};

struct AcquireResult {
  LeaseStatus status = LeaseStatus::Ok;
  uint8_t reused = 0;

  explicit operator bool() const { return status == LeaseStatus::Ok; }
  bool reusedExistingLease() const { return reused != 0; }
};

class PatternLeaseOwner {
 public:
  AcquireResult acquire(const Scene& scene,
                        int pageIndex,
                        uint8_t count,
                        PatternLease& lease,
                        int preferredLocalSlot = 0) {
    if (!isSupportedLeaseCount(count)) {
      return {LeaseStatus::UnsupportedLength, 0};
    }
    if (pageIndex < 0 || pageIndex >= kMaxPages) {
      return {LeaseStatus::InvalidPage, 0};
    }

    if (lease.isActive()) {
      const int ownerSlot = validateActiveLease(lease);
      if (ownerSlot < 0) return {LeaseStatus::InvalidLease, 0};
      const PatternLease& current = records_[ownerSlot];
      if (current.pageIndex != pageIndex || current.count != count) {
        return {LeaseStatus::ShapeMismatch, 0};
      }
      for (int i = 0; i < current.count; ++i) {
        if (PhraseGenerator::globalPatternIsReferenced(
                scene, current.globalPattern[i])) {
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

    int start = preferredLocalSlot;
    if (start < 0 || start >= kPatternsPerPage) start = 0;
    int found = 0;
    for (int offset = 0; offset < kPatternsPerPage && found < count; ++offset) {
      const int localSlot = (start + offset) % kPatternsPerPage;
      const int bank = localSlot / Bank<SynthPattern>::kPatterns;
      const int index = localSlot % Bank<SynthPattern>::kPatterns;
      const int globalPattern = songPatternFromPageBankIndex(
          pageIndex, bank, index);
      if (isLeased(globalPattern)) continue;
      if (!PhraseGenerator::localSlotIsSafeForPhrase(
              scene, pageIndex, localSlot)) {
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
      if (PhraseGenerator::globalPatternIsReferenced(
              scene, current.globalPattern[i])) {
        return LeaseStatus::PersistentReference;
      }
    }

    for (int i = 0; i < current.count; ++i) {
      const PatternAddress& address = addresses[i];
      scene.synthABanks[address.bank].patterns[address.slot] = SynthPattern{};
      scene.synthBBanks[address.bank].patterns[address.slot] = SynthPattern{};
      scene.drumBanks[address.bank].patterns[address.slot] = DrumPatternSet{};
    }

    deactivate(ownerSlot);
    lease = PatternLease{};
    return LeaseStatus::Ok;
  }

  LeaseStatus transferCommittedOwnership(const Scene& scene,
                                         int currentPageIndex,
                                         PatternLease& lease) {
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
      if (!hasCompletePersistentOwnership(
              scene, current.globalPattern[i])) {
        return LeaseStatus::IncompletePersistentOwnership;
      }
    }

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

  static uint8_t nextGeneration(uint8_t current) {
    ++current;
    return current == 0 ? 1 : current;
  }

  static bool hasCompletePersistentOwnership(const Scene& scene,
                                             int globalPattern) {
    for (int trackIndex = 0;
         trackIndex < SongPatternMaterializer::kEditableTrackCount;
         ++trackIndex) {
      const SongTrack track =
          SongPatternMaterializer::editableTrackForIndex(trackIndex);
      if (SongPatternMaterializer::globalPatternReferenceCount(
              scene, track, globalPattern) <= 0) {
        return false;
      }
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
        !isSupportedLeaseCount(lease.count) || lease.pageIndex >= kMaxPages) {
      return -1;
    }
    const PatternLease& current = records_[lease.ownerSlot];
    if (!current.isActive() || current.count != lease.count ||
        current.pageIndex != lease.pageIndex ||
        current.generation != lease.generation) {
      return -1;
    }
    for (int i = 0; i < current.count; ++i) {
      if (current.globalPattern[i] != lease.globalPattern[i]) return -1;
    }
    return lease.ownerSlot;
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
    case LeaseStatus::OwnerFull: return "Lease owner busy";
    case LeaseStatus::Exhausted: return "No safe pattern addresses";
    case LeaseStatus::InvalidLease: return "Invalid pattern lease";
    case LeaseStatus::ShapeMismatch: return "Active lease shape changed";
    case LeaseStatus::PageMismatch: return "Return to leased pattern page";
    case LeaseStatus::PersistentReference: return "Lease became referenced";
    case LeaseStatus::IncompletePersistentOwnership:
      return "Committed backing lacks aligned references";
  }
  return "Pattern lease error";
}

static_assert(sizeof(PatternLease) == 14,
              "P1a PatternLease fixed budget changed");
static_assert(sizeof(PatternLeaseOwner) == 28,
              "P1a PatternLeaseOwner fixed budget changed");
static_assert(std::is_trivially_copyable<PatternLease>::value,
              "P1a PatternLease must remain a fixed value");
static_assert(std::is_trivially_copyable<PatternLeaseOwner>::value,
              "P1a PatternLeaseOwner must remain fixed-capacity state");

}  // namespace PhrasePatternLease
