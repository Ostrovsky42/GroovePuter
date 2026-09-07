#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H
#define GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H

#include <cstdio>
#include <string>
#include <vector>

#include "src/state/material_slot_access.h"
#include "src/state/melody_store.h"

// M2b: turning a Pattern slot into a Melody slot, transactionally.
//
// The invariant worth the whole design: a descriptor saying MELODY must never
// exist without a readable melody behind it. A slot pointing at a missing file
// is a slot the engine will try to play and cannot, and nothing downstream can
// recover the music from that.
//
// So the descriptor changes last -- after the payload is written, closed, read
// back and verified against what was sent. Every failure before that leaves the
// slot exactly as it was. An orphaned temporary after power loss is acceptable:
// it wastes space and nothing points at it. The reverse is not.
//
// Domain only. No UI entry point, no engine dependency: the caller projects the
// Pattern into a candidate and hands it over; this commits it or refuses.
namespace MelodyPromotion {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

// The storage this needs, small enough that a test can be honest about failure
// at every step a real card fails at.
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
  AlreadyMelody,
  EncodeFailed,
  WriteFailed,
  VerifyFailed,
  PublishFailed,
};
// Addressed by what the melody already is -- project, voice and slot -- rather
// than by a separate identifier. A second ID space would be another mapping
// able to drift from what it names.
//
// The project name is passed in, and the caller passes
// PatternPagingService::currentProjectName(): the same owner that already
// decides which project's pattern pages are in play. Two owners of "which
// project is this" would file a melody under one project and its pattern under
// another -- the class of bug this design keeps removing.
inline std::string slotPath(const std::string& project, int voice, int slot,
                            const char* extension) {
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "/melody/v%d_s%02d.%s", voice, slot,
                extension);
  return "/projects/" + project + buffer;
}

inline std::string finalPath(const std::string& project, int voice, int slot) {
  return slotPath(project, voice, slot, "gpml");
}

inline std::string tempPath(const std::string& project, int voice, int slot) {
  return slotPath(project, voice, slot, "tmp");
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

inline bool loadResident(const FileSystem& fs, const std::string& project,
                         int voice, int slot, Buffer& out) {
  if (!fs.available()) return false;
  const std::string path = finalPath(project, voice, slot);
  if (!fs.exists(path.c_str())) return false;
  std::vector<uint8_t> blob;
  if (!fs.read(path.c_str(), blob)) return false;
  return MelodyStore::decode(blob.data(), blob.size(), out);
}

inline Error promoteResident(FileSystem& fs, const std::string& project,
                             Scene& scene, int voice, int slot,
                             const Buffer& candidate) {
  // Refused before the first mutation, so a missing card cannot leave the
  // project half-changed.
  if (!fs.available()) return Error::NoStorage;
  if (!GroovePuterMaterial::residentSlotInRange(voice, slot)) {
    return Error::BadSlot;
  }
  // One-way per slot. Promoting again would overwrite the melody with a fresh
  // projection and destroy every edit made since.
  if (GroovePuterMaterial::residentKind(scene, voice, slot) ==
      GroovePuterMaterial::MaterialKind::Melody) {
    return Error::AlreadyMelody;
  }

  std::vector<uint8_t> blob;
  if (!MelodyStore::encode(candidate, blob)) return Error::EncodeFailed;

  const std::string temp = tempPath(project, voice, slot);
  if (!fs.write(temp.c_str(), blob.data(), blob.size())) {
    return Error::WriteFailed;
  }

  // Read back what was actually stored, not what we meant to store. A write
  // that reports success and lands wrong is the failure this catches.
  std::vector<uint8_t> verify;
  Buffer restored{};
  if (!fs.read(temp.c_str(), verify) ||
      !MelodyStore::decode(verify.data(), verify.size(), restored) ||
      !sameMelody(candidate, restored)) {
    fs.remove(temp.c_str());
    return Error::VerifyFailed;
  }

  const std::string final = finalPath(project, voice, slot);
  if (!fs.rename(temp.c_str(), final.c_str())) {
    fs.remove(temp.c_str());
    return Error::PublishFailed;
  }

  // Last. Everything above can fail without the project noticing; past this
  // line the slot is a Melody and the payload is already there to prove it.
  if (!GroovePuterMaterial::setResidentKind(
          scene, voice, slot, GroovePuterMaterial::MaterialKind::Melody)) {
    return Error::BadSlot;
  }
  return Error::None;
}

}  // namespace MelodyPromotion

#endif  // GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H
