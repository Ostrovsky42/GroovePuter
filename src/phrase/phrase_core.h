#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../../scenes.h"

namespace PhraseCore {

constexpr uint8_t kSlotCount = 4;
constexpr uint8_t kMaxBars = 8;
constexpr uint8_t kTrackCount = 3;
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
  RegionOutOfRange,
  EmptyRegion,
  InvalidParent,
  DestinationOccupied,
  InvalidPhrase,
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

struct PhraseBank {
  PhraseSlot slots[kSlotCount]{};
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
  bool mutableBacking = false;
};

static_assert(sizeof(PhraseMetadata) == 12,
              "Phrase metadata must remain within the 16-byte slot budget");
static_assert(sizeof(PhraseSlot) == 60,
              "Phrase reference slot RAM budget changed");
static_assert(sizeof(PhraseBank) == 244,
              "Four-slot Phrase bank RAM budget changed");
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

inline int trackIndex(SongTrack track) {
  switch (track) {
    case SongTrack::SynthA: return 0;
    case SongTrack::SynthB: return 1;
    case SongTrack::Drums: return 2;
    case SongTrack::Voice: break;
  }
  return -1;
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

inline void reset(PhraseBank& bank) {
  for (int slot = 0; slot < kSlotCount; ++slot) {
    clearSlotValue(bank.slots[slot]);
  }
  bank.nextPhraseId = 1;
  bank.version = kPersistenceVersion;
  bank.reserved = 0;
}

inline bool isValid(const PhraseSlot& slot) {
  const PhraseMetadata& metadata = slot.metadata;
  return (metadata.flags & kFlagValid) != 0 &&
         metadata.phraseId != kNoPhraseId &&
         isValidLength(metadata.lengthBars) &&
         isValidRole(metadata.role) &&
         isValidSource(metadata.source) &&
         metadata.storage != StorageMode::Empty &&
         isValidStorage(metadata.storage) &&
         isValidTrackMask(metadata.trackMask) &&
         metadata.sourceSongSlot <= 1 &&
         metadata.sourceStartRow < Song::kMaxPositions;
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

inline int16_t patternAt(const PhraseSlot& phrase, uint8_t bar, SongTrack track) {
  const int index = trackIndex(track);
  if (!isValid(phrase) || bar >= phrase.metadata.lengthBars || index < 0) {
    return -1;
  }
  return phrase.patternRefs[bar][index];
}

inline SlotSummary summarize(const PhraseBank& bank, SlotId slotId) {
  SlotSummary summary{};
  summary.slot = slotId;
  const PhraseSlot* phrase = slotAt(bank, slotId);
  if (!phrase || !isValid(*phrase)) return summary;
  summary.valid = true;
  summary.phraseId = phrase->metadata.phraseId;
  summary.parentId = phrase->metadata.parentId;
  summary.lengthBars = phrase->metadata.lengthBars;
  summary.role = phrase->metadata.role;
  summary.source = phrase->metadata.source;
  summary.storage = phrase->metadata.storage;
  summary.trackMask = phrase->metadata.trackMask;
  summary.mutableBacking =
      (phrase->metadata.flags & kFlagMutableBacking) != 0;
  return summary;
}

inline Result clear(PhraseBank& bank, SlotId slotId) {
  Result result{};
  result.slot = slotId;
  PhraseSlot* phrase = slotAt(bank, slotId);
  if (!phrase) {
    result.error = Error::InvalidSlot;
    return result;
  }
  clearSlotValue(*phrase);
  return result;
}

inline Result captureSongRegion(PhraseBank& bank,
                                SlotId targetSlot,
                                const Song& song,
                                uint8_t sourceSongSlot,
                                uint8_t startRow,
                                uint8_t lengthBars,
                                Role role,
                                Source source,
                                uint8_t trackMask = kAllTracks,
                                uint8_t parentSlot = kNoSlot) {
  Result result{};
  result.slot = targetSlot;
  PhraseSlot* destination = slotAt(bank, targetSlot);
  if (!destination) {
    result.error = Error::InvalidSlot;
    return result;
  }
  if (!isValidLength(lengthBars)) {
    result.error = Error::InvalidLength;
    return result;
  }
  if (!isValidTrackMask(trackMask)) {
    result.error = Error::InvalidTrackMask;
    return result;
  }
  if (sourceSongSlot > 1) {
    result.error = Error::InvalidSongSlot;
    return result;
  }
  if (!isValidSource(source)) {
    result.error = Error::InvalidSource;
    return result;
  }
  if (startRow >= Song::kMaxPositions ||
      static_cast<int>(startRow) + lengthBars > Song::kMaxPositions ||
      static_cast<int>(startRow) + lengthBars > song.length) {
    result.error = Error::RegionOutOfRange;
    return result;
  }

  uint16_t parentId = kNoPhraseId;
  if (parentSlot != kNoSlot) {
    if (parentSlot >= kSlotCount ||
        !isValid(bank.slots[parentSlot])) {
      result.error = Error::InvalidParent;
      return result;
    }
    parentId = bank.slots[parentSlot].metadata.phraseId;
  } else if (source == Source::Derived) {
    result.error = Error::InvalidParent;
    return result;
  }

  PhraseSlot candidate{};
  clearSlotValue(candidate);
  bool hasMaterial = false;
  for (int bar = 0; bar < lengthBars; ++bar) {
    const SongPosition& position = song.positions[startRow + bar];
    for (int track = 0; track < kTrackCount; ++track) {
      if ((trackMask & maskForTrackIndex(track)) == 0) continue;
      const int16_t reference = position.patterns[track];
      candidate.patternRefs[bar][track] = reference;
      if (reference >= 0) hasMaterial = true;
    }
  }
  if (!hasMaterial) {
    result.error = Error::EmptyRegion;
    return result;
  }

  const uint16_t phraseId = nextPhraseId(bank);
  candidate.metadata.phraseId = phraseId;
  candidate.metadata.parentId = parentId;
  candidate.metadata.lengthBars = lengthBars;
  candidate.metadata.role = role;
  candidate.metadata.source = source;
  candidate.metadata.storage = StorageMode::ReferenceView;
  candidate.metadata.flags =
      kFlagValid | kFlagReferenceView | kFlagMutableBacking;
  candidate.metadata.sourceSongSlot = sourceSongSlot;
  candidate.metadata.sourceStartRow = startRow;
  candidate.metadata.trackMask = trackMask;

  *destination = candidate;
  bank.nextPhraseId = incrementPhraseId(phraseId);
  bank.version = kPersistenceVersion;
  result.phraseId = phraseId;
  return result;
}

inline Result deriveReferenceView(PhraseBank& bank,
                                  SlotId targetSlot,
                                  SlotId parentSlot,
                                  Role role) {
  Result result{};
  result.slot = targetSlot;
  PhraseSlot* destination = slotAt(bank, targetSlot);
  const PhraseSlot* parent = slotAt(bank, parentSlot);
  if (!destination) {
    result.error = Error::InvalidSlot;
    return result;
  }
  if (!parent || !isValid(*parent)) {
    result.error = Error::InvalidParent;
    return result;
  }

  PhraseSlot candidate = *parent;
  const uint16_t phraseId = nextPhraseId(bank);
  candidate.metadata.phraseId = phraseId;
  candidate.metadata.parentId = parent->metadata.phraseId;
  candidate.metadata.role = role;
  candidate.metadata.source = Source::Derived;
  candidate.metadata.storage = StorageMode::ReferenceView;
  candidate.metadata.flags =
      kFlagValid | kFlagReferenceView | kFlagMutableBacking;

  *destination = candidate;
  bank.nextPhraseId = incrementPhraseId(phraseId);
  bank.version = kPersistenceVersion;
  result.phraseId = phraseId;
  return result;
}

inline Result writeToSong(const PhraseBank& bank,
                          SlotId sourceSlot,
                          Song& destination,
                          uint8_t startRow,
                          bool overwrite) {
  Result result{};
  result.slot = sourceSlot;
  const PhraseSlot* phrase = slotAt(bank, sourceSlot);
  if (!phrase || !isValid(*phrase)) {
    result.error = Error::InvalidPhrase;
    return result;
  }
  if (startRow >= Song::kMaxPositions ||
      static_cast<int>(startRow) + phrase->metadata.lengthBars >
          Song::kMaxPositions) {
    result.error = Error::RegionOutOfRange;
    return result;
  }

  if (!overwrite) {
    for (int bar = 0; bar < phrase->metadata.lengthBars; ++bar) {
      for (int track = 0; track < kTrackCount; ++track) {
        if ((phrase->metadata.trackMask & maskForTrackIndex(track)) == 0) {
          continue;
        }
        if (destination.positions[startRow + bar].patterns[track] >= 0) {
          result.error = Error::DestinationOccupied;
          return result;
        }
      }
    }
  }

  for (int bar = 0; bar < phrase->metadata.lengthBars; ++bar) {
    for (int track = 0; track < kTrackCount; ++track) {
      if ((phrase->metadata.trackMask & maskForTrackIndex(track)) == 0) {
        continue;
      }
      destination.positions[startRow + bar].patterns[track] =
          phrase->patternRefs[bar][track];
    }
  }
  const int requiredLength = startRow + phrase->metadata.lengthBars;
  if (destination.length < requiredLength) destination.length = requiredLength;
  result.phraseId = phrase->metadata.phraseId;
  return result;
}

inline bool sanitize(PhraseBank& bank) {
  bool changed = false;
  if (bank.version != kPersistenceVersion) {
    reset(bank);
    return true;
  }
  if (bank.nextPhraseId == kNoPhraseId) {
    bank.nextPhraseId = 1;
    changed = true;
  }

  for (int slot = 0; slot < kSlotCount; ++slot) {
    PhraseSlot& phrase = bank.slots[slot];
    if ((phrase.metadata.flags & kFlagValid) == 0) {
      PhraseSlot empty{};
      clearSlotValue(empty);
      if (std::memcmp(&phrase, &empty, sizeof(PhraseSlot)) != 0) {
        phrase = empty;
        changed = true;
      }
      continue;
    }

    if (!isValid(phrase) ||
        static_cast<int>(phrase.metadata.sourceStartRow) +
                phrase.metadata.lengthBars >
            Song::kMaxPositions) {
      clearSlotValue(phrase);
      changed = true;
      continue;
    }

    for (int bar = 0; bar < kMaxBars; ++bar) {
      for (int track = 0; track < kTrackCount; ++track) {
        int16_t& reference = phrase.patternRefs[bar][track];
        if (bar >= phrase.metadata.lengthBars ||
            (phrase.metadata.trackMask & maskForTrackIndex(track)) == 0) {
          if (reference != -1) {
            reference = -1;
            changed = true;
          }
          continue;
        }
        if (reference < -1 || reference >= kMaxGlobalPatterns) {
          reference = -1;
          changed = true;
        }
      }
    }
  }
  return changed;
}

}  // namespace PhraseCore
