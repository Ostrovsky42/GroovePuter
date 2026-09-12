#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_IDENTITY_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_IDENTITY_H

#include <cstdint>

namespace GroovePuterMaterial {

// Project-relative location only. A MaterialAddress says where a material is
// currently stored; it is not the material's identity and may be reused.
struct MaterialAddress {
  uint8_t voice = 0;
  uint8_t globalSlot = 0;
};

inline bool operator==(MaterialAddress lhs, MaterialAddress rhs) {
  return lhs.voice == rhs.voice && lhs.globalSlot == rhs.globalSlot;
}

inline bool operator!=(MaterialAddress lhs, MaterialAddress rhs) {
  return !(lhs == rhs);
}

static_assert(sizeof(MaterialAddress) == 2,
              "MaterialAddress must remain a two-byte embedded value");

// Opaque identity within one project namespace. Zero is deliberately invalid:
// legacy/unassigned storage must fail closed instead of degrading to
// address-only identity.
struct MaterialId {
  uint32_t value = 0;

  constexpr bool valid() const { return value != 0; }
};

inline bool operator==(MaterialId lhs, MaterialId rhs) {
  return lhs.value == rhs.value;
}

inline bool operator!=(MaterialId lhs, MaterialId rhs) {
  return !(lhs == rhs);
}

static_assert(sizeof(MaterialId) == sizeof(uint32_t),
              "MaterialId must remain a four-byte opaque value");

struct MaterialReference {
  MaterialAddress address{};
  MaterialId id{};
};

// References are intentionally fail-closed. A matching address is never
// sufficient because a slot may later contain a different material.
inline bool materialReferenceMatches(const MaterialReference& reference,
                                     MaterialAddress actualAddress,
                                     MaterialId actualId) {
  return reference.id.valid() && actualId.valid() &&
         reference.address == actualAddress && reference.id == actualId;
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_IDENTITY_H
