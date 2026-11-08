#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_PUBLICATION_RECORD_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_PUBLICATION_RECORD_H

#include <cstddef>
#include <cstdint>

namespace GroovePuterMaterial {

constexpr uint32_t kPublicationMagic = 0x47504D50u;  // 'GPMP'
constexpr uint16_t kPublicationSchema = 1;

enum class PublicationSlot : uint8_t {
  SlotA = 0,
  SlotB = 1,
  None = 0xFF,
};

inline char publicationSlotSuffix(PublicationSlot slot) {
  switch (slot) {
    case PublicationSlot::SlotA: return 'a';
    case PublicationSlot::SlotB: return 'b';
    default: return '\0';
  }
}

inline PublicationSlot publicationSlotFromSuffix(char suffix) {
  if (suffix == 'a' || suffix == 'A') return PublicationSlot::SlotA;
  if (suffix == 'b' || suffix == 'B') return PublicationSlot::SlotB;
  return PublicationSlot::None;
}

inline PublicationSlot alternateSlot(PublicationSlot slot) {
  return slot == PublicationSlot::SlotA ? PublicationSlot::SlotB : PublicationSlot::SlotA;
}

struct MaterialPublicationRecord {
  uint32_t magic{kPublicationMagic};
  uint16_t schema{kPublicationSchema};
  uint8_t slot{static_cast<uint8_t>(PublicationSlot::None)};
  uint8_t pageIndex{0};
  uint32_t storageGeneration{0};
  uint32_t checksum{0};
};

static_assert(sizeof(MaterialPublicationRecord) == 16,
              "MaterialPublicationRecord must remain exactly 16 bytes");

inline uint32_t checksumPublicationRecord(const MaterialPublicationRecord& record) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&record);
  constexpr size_t checksumOffset = offsetof(MaterialPublicationRecord, checksum);
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < checksumOffset; ++i) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}

inline bool validPublicationRecord(const MaterialPublicationRecord& record,
                                   int expectedPage = -1) {
  if (record.magic != kPublicationMagic ||
      record.schema != kPublicationSchema ||
      (record.slot != static_cast<uint8_t>(PublicationSlot::SlotA) &&
       record.slot != static_cast<uint8_t>(PublicationSlot::SlotB)) ||
      record.checksum != checksumPublicationRecord(record)) {
    return false;
  }
  if (expectedPage >= 0 && record.pageIndex != static_cast<uint8_t>(expectedPage)) {
    return false;
  }
  return true;
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_PUBLICATION_RECORD_H
