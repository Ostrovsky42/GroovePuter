#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H

#include <cstdint>
#include <string>

#include "src/state/material_version.h"
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
  MaterialVersionToken version{};

  constexpr bool isResolved() const {
    return status == MaterialResolutionStatus::ResolvedPattern ||
           status == MaterialResolutionStatus::ResolvedMelody;
  }
  constexpr bool hasVersion() const { return isResolved() && version.valid(); }
};

static_assert(sizeof(MaterialResolution) <= 12,
              "MaterialResolution must remain a small control-side value");

inline MaterialResolution unresolved(MaterialResolutionStatus status,
                                     MaterialKind kind) {
  return {status, kind, {}};
}

inline const SynthPattern& residentPatternFor(const Scene& scene,
                                              MaterialAddress address) {
  const int bank = songPatternBank(static_cast<int>(address.globalSlot));
  const int index = songPatternIndexInBank(static_cast<int>(address.globalSlot));
  return address.voice == 0 ? scene.synthABanks[bank].patterns[index]
                            : scene.synthBBanks[bank].patterns[index];
}

inline MaterialResolution resolveMaterial(
    const MelodyPromotion::FileSystem& fs, const std::string& project,
    const Scene& scene, int activePage, MaterialAddress address,
    PhraseRuntime::RuntimeSynthEventBuffer& melodyOut) {
  if (!materialAddressInRange(address)) {
    return unresolved(MaterialResolutionStatus::InvalidAddress,
                      MaterialKind::Pattern);
  }

  if (!materialAddressIsResident(address, activePage)) {
    return unresolved(MaterialResolutionStatus::NotResident,
                      MaterialKind::Pattern);
  }

  const int slot = residentSlotFor(address);
  if (!residentSlotInRange(address.voice, slot)) {
    return unresolved(MaterialResolutionStatus::InvalidAddress,
                      MaterialKind::Pattern);
  }

  const MaterialKind kind = residentKind(scene, address.voice, slot);
  if (kind == MaterialKind::Pattern) {
    return {MaterialResolutionStatus::ResolvedPattern, MaterialKind::Pattern,
            versionForPattern(residentPatternFor(scene, address))};
  }

  if (!fs.available()) {
    return unresolved(MaterialResolutionStatus::StorageUnavailable,
                      MaterialKind::Melody);
  }

  const std::string path = MelodyPromotion::finalPath(project, address);
  if (!fs.exists(path.c_str())) {
    return unresolved(MaterialResolutionStatus::MissingPayload,
                      MaterialKind::Melody);
  }

  if (!MelodyPromotion::loadMaterial(fs, project, address, melodyOut)) {
    return unresolved(MaterialResolutionStatus::CorruptPayload,
                      MaterialKind::Melody);
  }

  return {MaterialResolutionStatus::ResolvedMelody, MaterialKind::Melody,
          versionForMelody(melodyOut)};
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H
