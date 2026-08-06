#pragma once

#include <cstdint>

#include "phrase_types.h"

namespace PhraseCore {

constexpr int kPersistHeaderValues = 2;
constexpr int kPersistMetadataValues = 10;
constexpr int kPersistReferenceValues = kMaxBars * kTrackCount;
constexpr int kPersistValuesPerSlot =
    kPersistMetadataValues + kPersistReferenceValues;
constexpr int kPersistValueCount =
    kPersistHeaderValues + kSlotCount * kPersistValuesPerSlot;

inline int32_t persistentValueAt(const PhraseBank& bank, int flatIndex) {
  if (flatIndex == 0) return bank.version;
  if (flatIndex == 1) return bank.nextPhraseId;
  if (flatIndex < kPersistHeaderValues || flatIndex >= kPersistValueCount) {
    return 0;
  }

  const int relative = flatIndex - kPersistHeaderValues;
  const int slotIndexValue = relative / kPersistValuesPerSlot;
  const int slotOffset = relative % kPersistValuesPerSlot;
  const PhraseSlot& slot = bank.slots[slotIndexValue];
  const PhraseMetadata& metadata = slot.metadata;

  switch (slotOffset) {
    case 0: return metadata.phraseId;
    case 1: return metadata.parentId;
    case 2: return metadata.lengthBars;
    case 3: return static_cast<uint8_t>(metadata.role);
    case 4: return static_cast<uint8_t>(metadata.source);
    case 5: return static_cast<uint8_t>(metadata.storage);
    case 6: return metadata.flags;
    case 7: return metadata.sourceSongSlot;
    case 8: return metadata.sourceStartRow;
    case 9: return metadata.trackMask;
    default: break;
  }

  const int refOffset = slotOffset - kPersistMetadataValues;
  const int bar = refOffset / kTrackCount;
  const int track = refOffset % kTrackCount;
  return slot.patternRefs[bar][track];
}

inline uint8_t persistentByte(int32_t value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

inline uint16_t persistentWord(int32_t value) {
  if (value < 0) return 0;
  if (value > 65535) return 65535;
  return static_cast<uint16_t>(value);
}

inline int16_t persistentReference(int32_t value) {
  if (value < -1 || value > 32767) return -1;
  return static_cast<int16_t>(value);
}

inline bool applyPersistentValue(PhraseBank& bank,
                                 int flatIndex,
                                 int32_t value) {
  if (flatIndex < 0 || flatIndex >= kPersistValueCount) return false;
  if (flatIndex == 0) {
    bank.version = persistentByte(value);
    return true;
  }
  if (flatIndex == 1) {
    bank.nextPhraseId = persistentWord(value);
    return true;
  }

  const int relative = flatIndex - kPersistHeaderValues;
  const int slotIndexValue = relative / kPersistValuesPerSlot;
  const int slotOffset = relative % kPersistValuesPerSlot;
  PhraseSlot& slot = bank.slots[slotIndexValue];
  PhraseMetadata& metadata = slot.metadata;

  switch (slotOffset) {
    case 0: metadata.phraseId = persistentWord(value); return true;
    case 1: metadata.parentId = persistentWord(value); return true;
    case 2: metadata.lengthBars = persistentByte(value); return true;
    case 3:
      metadata.role = static_cast<Role>(persistentByte(value));
      return true;
    case 4:
      metadata.source = static_cast<Source>(persistentByte(value));
      return true;
    case 5:
      metadata.storage = static_cast<StorageMode>(persistentByte(value));
      return true;
    case 6: metadata.flags = persistentByte(value); return true;
    case 7: metadata.sourceSongSlot = persistentByte(value); return true;
    case 8: metadata.sourceStartRow = persistentByte(value); return true;
    case 9: metadata.trackMask = persistentByte(value); return true;
    default: break;
  }

  const int refOffset = slotOffset - kPersistMetadataValues;
  const int bar = refOffset / kTrackCount;
  const int track = refOffset % kTrackCount;
  slot.patternRefs[bar][track] = persistentReference(value);
  return true;
}

inline void beginPersistentDecode(PhraseBank& bank) {
  reset(bank);
}

}  // namespace PhraseCore
