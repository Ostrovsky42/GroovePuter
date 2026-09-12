#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H
#define GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H

#include <cstdint>
#include <string>

#include "src/state/material_identity.h"
#include "src/state/material_version.h"
#include "src/state/melody_promotion.h"

// 0.9.11 A2: fail-closed control-side material resolution.
//
// MaterialAddress says where to look. MaterialId says which material is allowed
// to be there. MaterialVersionToken is computed only after that identity has
// been proven and answers a separate question: whether the canonical musical
// state is exactly the same.
namespace GroovePuterMaterial {

enum class MaterialResolutionStatus : uint8_t {
  ResolvedPattern = 0,
  ResolvedMelody,
  NotResident,
  MissingPayload,
  CorruptPayload,
  InvalidAddress,
  InvalidReference,
  IdentityMismatch,
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
                                     MaterialKind kind = MaterialKind::Pattern) {
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
    const Scene& scene, int activePage, const MaterialReference& reference,
    PhraseRuntime::RuntimeSynthEventBuffer& melodyOut) {
  // Identity validity is independent of address validity. An unassigned id may
  // never degrade into address-only lookup.
  if (!reference.id.valid()) {
    return unresolved(MaterialResolutionStatus::InvalidReference);
  }

  const MaterialAddress address = reference.address;
  if (!materialAddressInRange(address)) {
    return unresolved(MaterialResolutionStatus::InvalidAddress);
  }

  if (!materialAddressIsResident(address, activePage)) {
    return unresolved(MaterialResolutionStatus::NotResident);
  }

  const int residentSlot = residentSlotFor(address);
  if (!residentSlotInRange(address.voice, residentSlot)) {
    return unresolved(MaterialResolutionStatus::InvalidAddress);
  }

  // This check is the identity boundary. Nothing representation-specific is
  // inspected, loaded or fingerprinted before it succeeds.
  const MaterialId actualId = residentId(scene, address.voice, residentSlot);
  if (!actualId.valid() || actualId != reference.id) {
    return unresolved(MaterialResolutionStatus::IdentityMismatch);
  }

  const MaterialKind kind = residentKind(scene, address.voice, residentSlot);
  if (kind == MaterialKind::Pattern) {
    // Version is deliberately last: it proves exact state, never identity.
    const MaterialVersionToken version =
        versionForPattern(residentPatternFor(scene, address));
    return {MaterialResolutionStatus::ResolvedPattern, MaterialKind::Pattern,
            version};
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

  // Version is computed only after identity, representation and payload have
  // all been proven trustworthy.
  const MaterialVersionToken version = versionForMelody(melodyOut);
  return {MaterialResolutionStatus::ResolvedMelody, MaterialKind::Melody,
          version};
}

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_MATERIAL_RESOLUTION_H
