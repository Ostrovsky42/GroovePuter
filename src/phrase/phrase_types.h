#pragma once

#include <cstdint>
#include <type_traits>

namespace PhraseCore {

constexpr uint8_t kSlotCount = 4;
constexpr uint8_t kMaxBars = 8;
constexpr uint8_t kTrackCount = 3;
constexpr uint8_t kArrangementCapacity = 16;
constexpr uint8_t kNoSlot = 0xFFu;
constexpr uint16_t kNoPhraseId = 0u;
constexpr uint8_t kPersistenceVersion = 1u;

constexpr uint8_t kTrackSynthA = 1u << 0;
constexpr uint8_t kTrackSynthB = 1u << 1;
constexpr uint8_t kTrackDrums = 1u << 2;
constexpr uint8_t kAllTracks = kTrackSynthA | kTrackSynthB | kTrackDrums;

constexpr uint8_t kFlagValid = 1u << 0;
constexpr uint8_t kFlagReferenceView = 1u << 1;
constexpr uint8_t kFlagMutableBacking = 1u << 2;

enum class SlotId : uint8_t {
  A = 0,
  B = 1,
  C = 2,
  D = 3,
};

enum class Role : uint8_t {
  Main = 0,
  Variation = 1,
  Break = 2,
  Ending = 3,
};

enum class Source : uint8_t {
  None = 0,
  InternalPattern = 1,
  Generated = 2,
  Derived = 3,
  SmfRegion = 4,
  LiveCapture = 5,
};

enum class StorageMode : uint8_t {
  Empty = 0,
  ReferenceView = 1,
  OwnedEvents = 2,
};

enum class Error : uint8_t {
  None = 0,
  InvalidSlot,
  InvalidLength,
  InvalidTrackMask,
  InvalidSongSlot,
  InvalidSource,
  InvalidRole,
  RegionOutOfRange,
  EmptyRegion,
  InvalidParent,
  DestinationOccupied,
  InvalidPhrase,
  InvalidArrangementPosition,
  ArrangementFull,
  ArrangementEmpty,
};

struct PhraseMetadata {
  uint16_t phraseId = kNoPhraseId;
  uint16_t parentId = kNoPhraseId;
  uint8_t lengthBars = 0;
  Role role = Role::Main;
  Source source = Source::None;
  StorageMode storage = StorageMode::Empty;
  uint8_t flags = 0;
  uint8_t sourceSongSlot = 0;
  uint8_t sourceStartRow = 0;
  uint8_t trackMask = 0;
};

struct PhraseSlot {
  PhraseMetadata metadata{};
  int16_t patternRefs[kMaxBars][kTrackCount]{};
};

struct PhraseArrangement {
  uint8_t slots[kArrangementCapacity]{};
  uint8_t length = 0;
  uint8_t reserved = 0;
};

struct PhraseBank {
  PhraseSlot slots[kSlotCount]{};
  PhraseArrangement arrangement{};
  uint16_t nextPhraseId = 1;
  uint8_t version = kPersistenceVersion;
  uint8_t reserved = 0;
};

struct Result {
  Error error = Error::None;
  SlotId slot = SlotId::A;
  uint16_t phraseId = kNoPhraseId;

  explicit operator bool() const { return error == Error::None; }
};

struct ArrangementResult {
  Error error = Error::None;
  uint8_t position = 0;
  uint8_t length = 0;
  uint8_t totalBars = 0;
  bool changed = false;

  explicit operator bool() const { return error == Error::None; }
};

struct SlotSummary {
  bool valid = false;
  SlotId slot = SlotId::A;
  uint16_t phraseId = kNoPhraseId;
  uint16_t parentId = kNoPhraseId;
  uint8_t lengthBars = 0;
  Role role = Role::Main;
  Source source = Source::None;
  StorageMode storage = StorageMode::Empty;
  uint8_t trackMask = 0;
  uint8_t sourceSongSlot = 0;
  uint8_t sourceStartRow = 0;
  bool mutableBacking = false;
};

struct BarPreview {
  int16_t patternRefs[kTrackCount] = {-1, -1, -1};
  uint16_t synthAMask = 0;
  uint16_t synthBMask = 0;
  uint16_t drumMask = 0;
  uint8_t energy = 0;
  uint8_t resolvedMask = 0;
};

static_assert(sizeof(PhraseMetadata) == 12,
              "Phrase metadata must remain within the 16-byte slot budget");
static_assert(sizeof(PhraseSlot) == 60,
              "Phrase reference slot RAM budget changed");
static_assert(sizeof(PhraseArrangement) == 18,
              "Phrase arrangement RAM budget changed");
static_assert(sizeof(PhraseBank) == 262,
              "Phrase bank RAM budget changed");
static_assert(std::is_trivially_copyable<PhraseBank>::value,
              "PhraseBank must stay a fixed-capacity persistence value");

inline int slotIndex(SlotId slot) {
  const int value = static_cast<int>(slot);
  return value >= 0 && value < kSlotCount ? value : -1;
}

inline bool isValidLength(uint8_t bars) {
  return bars == 1 || bars == 2 || bars == 4 || bars == 8;
}

inline bool isValidRole(Role role) {
  return static_cast<uint8_t>(role) <= static_cast<uint8_t>(Role::Ending);
}

inline bool isValidSource(Source source) {
  const uint8_t value = static_cast<uint8_t>(source);
  return value >= static_cast<uint8_t>(Source::InternalPattern) &&
         value <= static_cast<uint8_t>(Source::LiveCapture);
}

inline bool isSongReferenceSource(Source source) {
  return source == Source::InternalPattern || source == Source::Generated ||
         source == Source::Derived;
}

inline bool isValidStorage(StorageMode storage) {
  const uint8_t value = static_cast<uint8_t>(storage);
  return value <= static_cast<uint8_t>(StorageMode::OwnedEvents);
}

inline bool isValidTrackMask(uint8_t trackMask) {
  return trackMask != 0 && (trackMask & ~kAllTracks) == 0;
}

inline uint8_t maskForTrackIndex(int trackIndex) {
  return trackIndex >= 0 && trackIndex < kTrackCount
             ? static_cast<uint8_t>(1u << trackIndex)
             : 0;
}

inline const char* slotName(SlotId slot) {
  switch (slot) {
    case SlotId::A: return "A";
    case SlotId::B: return "B";
    case SlotId::C: return "C";
    case SlotId::D: return "D";
  }
  return "?";
}

inline const char* roleName(Role role) {
  switch (role) {
    case Role::Main: return "MAIN";
    case Role::Variation: return "VARIATION";
    case Role::Break: return "BREAK";
    case Role::Ending: return "ENDING";
  }
  return "UNKNOWN";
}

inline const char* sourceName(Source source) {
  switch (source) {
    case Source::None: return "NONE";
    case Source::InternalPattern: return "PATTERN";
    case Source::Generated: return "GENERATED";
    case Source::Derived: return "DERIVED";
    case Source::SmfRegion: return "SMF";
    case Source::LiveCapture: return "LIVE";
  }
  return "UNKNOWN";
}

inline const char* storageName(StorageMode storage) {
  switch (storage) {
    case StorageMode::Empty: return "EMPTY";
    case StorageMode::ReferenceView: return "REFERENCE VIEW";
    case StorageMode::OwnedEvents: return "OWNED EVENTS";
  }
  return "UNKNOWN";
}

inline void clearSlotValue(PhraseSlot& slot) {
  slot.metadata = PhraseMetadata{};
  for (int bar = 0; bar < kMaxBars; ++bar) {
    for (int track = 0; track < kTrackCount; ++track) {
      slot.patternRefs[bar][track] = -1;
    }
  }
}

inline void clearArrangementValue(PhraseArrangement& arrangement) {
  for (int position = 0; position < kArrangementCapacity; ++position) {
    arrangement.slots[position] = kNoSlot;
  }
  arrangement.length = 0;
  arrangement.reserved = 0;
}

inline void reset(PhraseBank& bank) {
  for (int slot = 0; slot < kSlotCount; ++slot) {
    clearSlotValue(bank.slots[slot]);
  }
  clearArrangementValue(bank.arrangement);
  bank.nextPhraseId = 1;
  bank.version = kPersistenceVersion;
  bank.reserved = 0;
}

inline PhraseSlot* slotAt(PhraseBank& bank, SlotId slot) {
  const int index = slotIndex(slot);
  return index >= 0 ? &bank.slots[index] : nullptr;
}

inline const PhraseSlot* slotAt(const PhraseBank& bank, SlotId slot) {
  const int index = slotIndex(slot);
  return index >= 0 ? &bank.slots[index] : nullptr;
}

inline uint16_t nextPhraseId(const PhraseBank& bank) {
  return bank.nextPhraseId == kNoPhraseId ? 1 : bank.nextPhraseId;
}

inline uint16_t incrementPhraseId(uint16_t phraseId) {
  ++phraseId;
  return phraseId == kNoPhraseId ? 1 : phraseId;
}

}  // namespace PhraseCore
