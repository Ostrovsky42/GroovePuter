#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H

#include <cstdint>
#include <string>

#include "src/state/melody_promotion.h"

// 0.9.11 A2: explicit control-side material resolution.
//
// `MaterialAddress` tells us where to look. It does not prove that material is
// resident, readable, or even valid. This boundary converts address + project
// context into an observed material state without teaching low-level slot
// projection helpers about storage.
//
// IMPORTANT: unresolved states are never silently interpreted as Pattern.
// Pattern is returned only when the resident descriptor explicitly says that
// the canonical representation is Pattern.
namespace GroovePuterMaterial {

enum class MaterialResolutionStatus : uint8_t {
  ResolvedPattern = 0,
  ResolvedMelody,
  NotResident,
  MissingPayload,
  CorruptPayload,
  InvalidAddress,
  StorageUnavailable,
};

struct MaterialResolution {
  MaterialResolutionStatus status = MaterialResolutionStatus::InvalidAddress;
  MaterialKind kind = MaterialKind::Pattern;

  constexpr bool isResolved() const {
    return status == MaterialResolutionStatus::ResolvedPattern ||
           status == MaterialResolutionStatus::ResolvedMelody;
  }
};

static_assert(sizeof(MaterialResolution) <= 4,
              "MaterialResolution must remain a tiny control-side value");

inline MaterialResolution resolveMaterial(
    const MelodyPromotion::FileSystem& fs, const std::string& project,
    const Scene& scene, int activePage, MaterialAddress address,
    PhraseRuntime::RuntimeSynthEventBuffer& melodyOut) {
  if (!materialAddressInRange(address)) {
    return {MaterialResolutionStatus::InvalidAddress, MaterialKind::Pattern};
  }

  if (!materialAddressIsResident(address, activePage)) {
    return {MaterialResolutionStatus::NotResident, MaterialKind::Pattern};
  }

  const int slot = residentSlotFor(address);
  if (!residentSlotInRange(address.voice, slot)) {
    return {MaterialResolutionStatus::InvalidAddress, MaterialKind::Pattern};
  }

  const MaterialKind kind = residentKind(scene, address.voice, slot);
  if (kind == MaterialKind::Pattern) {
    return {MaterialResolutionStatus::ResolvedPattern, MaterialKind::Pattern};
  }

  if (!fs.available()) {
    return {MaterialResolutionStatus::StorageUnavailable,
            MaterialKind::Melody};
  }

  const std::string path = MelodyPromotion::finalPath(project, address);
  if (!fs.exists(path.c_str())) {
    return {MaterialResolutionStatus::MissingPayload, MaterialKind::Melody};
  }

  if (!MelodyPromotion::loadMaterial(fs, project, address, melodyOut)) {
    return {MaterialResolutionStatus::CorruptPayload, MaterialKind::Melody};
  }

  return {MaterialResolutionStatus::ResolvedMelody, MaterialKind::Melody};
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H
