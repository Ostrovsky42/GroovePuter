#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H
#define GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H

#include <cstdio>
#include <string>
#include <vector>

#include "src/state/material_slot_access.h"
#include "src/state/melody_store.h"

// M2b/A2-B: turning a Pattern slot into a Melody slot transactionally.
//
// The descriptor changes last -- after the payload is written, read back and
// verified. A2-B adds the missing mutation-authority rule: the caller must
// present the MaterialReference that authorized preparation of the candidate.
// Address/resident-slot equality alone is not authority, because both can be
// reused by a different MaterialId before promotion starts.
namespace MelodyPromotion {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct FileSystem {
  virtual ~FileSystem() = default;
  virtual bool available() const = 0;
  virtual bool exists(const char* path) const = 0;
  virtual bool write(const char* path, const uint8_t* data, size_t length) = 0;
  virtual bool read(const char* path, std::vector<uint8_t>& out) const = 0;
  virtual bool rename(const char* from, const char* to) = 0;
  virtual bool remove(const char* path) = 0;
};

enum class Error : uint8_t {
  None = 0,
  NoStorage,
  BadSlot,
  InvalidReference,
  IdentityMismatch,
  AlreadyMelody,
  EncodeFailed,
  WriteFailed,
  VerifyFailed,
  PublishFailed,
};

inline std::string slotPath(const std::string& project,
                            GroovePuterMaterial::MaterialAddress address,
                            const char* extension) {
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "/melody/v%u_g%03u.%s",
                static_cast<unsigned>(address.voice),
                static_cast<unsigned>(address.globalSlot), extension);
  return "/projects/" + project + buffer;
}

inline std::string finalPath(
    const std::string& project,
    GroovePuterMaterial::MaterialAddress address) {
  return slotPath(project, address, "gpml");
}

inline std::string tempPath(
    const std::string& project,
    GroovePuterMaterial::MaterialAddress address) {
  return slotPath(project, address, "tmp");
}

// Page-0 compatibility path helpers. These are locators only; mutation does not
// regain address-only authority through them.
inline std::string slotPath(const std::string& project, int voice, int slot,
                            const char* extension) {
  return slotPath(project,
                  {static_cast<uint8_t>(voice), static_cast<uint8_t>(slot)},
                  extension);
}

inline std::string finalPath(const std::string& project, int voice, int slot) {
  return finalPath(
      project, {static_cast<uint8_t>(voice), static_cast<uint8_t>(slot)});
}

inline std::string tempPath(const std::string& project, int voice, int slot) {
  return tempPath(
      project, {static_cast<uint8_t>(voice), static_cast<uint8_t>(slot)});
}

inline bool sameMelody(const Buffer& a, const Buffer& b) {
  if (a.count != b.count || a.lengthTicks != b.lengthTicks) return false;
  for (uint16_t i = 0; i < a.count; ++i) {
    const auto& x = a.events[i];
    const auto& y = b.events[i];
    if (x.startTick != y.startTick || x.durationSubticks != y.durationSubticks ||
        x.note != y.note || x.velocity != y.velocity ||
        x.probability != y.probability || x.flags != y.flags ||
        x.fx != y.fx || x.fxParam != y.fxParam) {
      return false;
    }
  }
  return true;
}

inline bool loadMaterial(
    const FileSystem& fs, const std::string& project,
    GroovePuterMaterial::MaterialAddress address, Buffer& out) {
  if (!fs.available() || !GroovePuterMaterial::materialAddressInRange(address)) {
    return false;
  }
  const std::string path = finalPath(project, address);
  if (!fs.exists(path.c_str())) return false;
  std::vector<uint8_t> blob;
  if (!fs.read(path.c_str(), blob)) return false;
  return MelodyStore::decode(blob.data(), blob.size(), out);
}

inline bool loadResident(const FileSystem& fs, const std::string& project,
                         int voice, int slot, Buffer& out) {
  if (!GroovePuterMaterial::residentSlotInRange(voice, slot)) return false;
  return loadMaterial(
      fs, project,
      {static_cast<uint8_t>(voice), static_cast<uint8_t>(slot)}, out);
}

inline Error promoteResident(
    FileSystem& fs, const std::string& project, Scene& scene,
    const GroovePuterMaterial::MaterialReference& reference, int residentSlot,
    const Buffer& candidate) {
  using namespace GroovePuterMaterial;

  // Identity is admission authority. It is checked before storage availability,
  // encoding, temp creation, or any other operation that could publish a stale
  // candidate under a replacement material.
  if (!reference.id.valid()) return Error::InvalidReference;

  const MaterialAddress address = reference.address;
  if (!materialAddressInRange(address) ||
      !residentSlotInRange(address.voice, residentSlot) ||
      residentSlotFor(address) != residentSlot) {
    return Error::BadSlot;
  }

  const MaterialId actualId = residentId(scene, address.voice, residentSlot);
  if (!actualId.valid() || actualId != reference.id) {
    return Error::IdentityMismatch;
  }

  // Existing M2b transaction semantics begin here and remain unchanged.
  if (!fs.available()) return Error::NoStorage;

  if (residentKind(scene, address.voice, residentSlot) == MaterialKind::Melody) {
    return Error::AlreadyMelody;
  }

  std::vector<uint8_t> blob;
  if (!MelodyStore::encode(candidate, blob)) return Error::EncodeFailed;

  // Durable locator is global; runtime lookup remains resident.
  const std::string temp = tempPath(project, address);
  if (!fs.write(temp.c_str(), blob.data(), blob.size())) {
    return Error::WriteFailed;
  }

  // Read back what was actually stored, not what we meant to store.
  std::vector<uint8_t> verify;
  Buffer restored{};
  if (!fs.read(temp.c_str(), verify) ||
      !MelodyStore::decode(verify.data(), verify.size(), restored) ||
      !sameMelody(candidate, restored)) {
    fs.remove(temp.c_str());
    return Error::VerifyFailed;
  }

  const std::string final = finalPath(project, address);
  if (!fs.rename(temp.c_str(), final.c_str())) {
    fs.remove(temp.c_str());
    return Error::PublishFailed;
  }

  // Last. Durable publication is complete before the resident descriptor moves.
  if (!setResidentKind(scene, address.voice, residentSlot, MaterialKind::Melody)) {
    return Error::BadSlot;
  }
  return Error::None;
}

// Bare-address mutation entry points remain source-compatible only so an older
// caller fails closed instead of silently publishing against whichever identity
// happens to occupy the coordinate now. They cannot manufacture a fresh
// MaterialReference from current state because doing so would recreate the ABA
// bug A2-B closes.
inline Error promoteResident(
    FileSystem& fs, const std::string& project, Scene& scene,
    GroovePuterMaterial::MaterialAddress address, int residentSlot,
    const Buffer& candidate) {
  (void)fs;
  (void)project;
  (void)scene;
  (void)address;
  (void)residentSlot;
  (void)candidate;
  return Error::InvalidReference;
}

inline Error promoteResident(FileSystem& fs, const std::string& project,
                             Scene& scene, int voice, int slot,
                             const Buffer& candidate) {
  (void)fs;
  (void)project;
  (void)scene;
  (void)voice;
  (void)slot;
  (void)candidate;
  return Error::InvalidReference;
}

}  // namespace MelodyPromotion

#endif  // GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H
