#pragma once
#ifndef GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H
#define GROOVEPUTER_SRC_STATE_MELODY_PROMOTION_H

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

#include <cstdio>
#include <string>
#include <vector>

#include "src/platform/log.h"
#include "src/state/material_publication_record.h"
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

class SdFileSystem : public FileSystem {
 public:
  bool available() const override { return true; }
  bool exists(const char* path) const override { return SD.exists(path); }
  static bool createDirectoriesRecursive(const std::string& dirPath) {
    if (dirPath.empty() || dirPath == "/" || SD.exists(dirPath.c_str())) {
      return true;
    }
    size_t pos = 1;
    while ((pos = dirPath.find('/', pos)) != std::string::npos) {
      std::string sub = dirPath.substr(0, pos);
      if (!sub.empty() && sub != "/" && !SD.exists(sub.c_str())) {
        if (!SD.mkdir(sub.c_str())) return false;
      }
      ++pos;
    }
    return SD.exists(dirPath.c_str()) || SD.mkdir(dirPath.c_str());
  }

  bool write(const char* path, const uint8_t* data, size_t length) override {
    std::string p(path);
    auto pos = p.find_last_of('/');
    if (pos != std::string::npos) {
      std::string dir = p.substr(0, pos);
      createDirectoriesRecursive(dir);
    }
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
      LOG_DEBUG("[SdFileSystem::write] SD.open(%s, FILE_WRITE) FAILED\n", path);
      return false;
    }
    const size_t written = f.write(data, length);
    f.flush();
    f.close();
    return written == length;
  }
  bool read(const char* path, std::vector<uint8_t>& out) const override {
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    out.resize(f.size());
    const size_t readCount = f.read(out.data(), out.size());
    f.close();
    return readCount == out.size();
  }
  bool rename(const char* from, const char* to) override {
    return SD.rename(from, to);
  }
  bool remove(const char* path) override {
    return !SD.exists(path) || SD.remove(path);
  }
};

inline FileSystem& defaultFileSystem() {
  static SdFileSystem fs;
  return fs;
}

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

inline std::string slotPath(const std::string& project,
                            GroovePuterMaterial::MaterialAddress address,
                            GroovePuterMaterial::PublicationSlot slot) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "/melody/v%u_g%03u_%c.gpml",
                static_cast<unsigned>(address.voice),
                static_cast<unsigned>(address.globalSlot),
                GroovePuterMaterial::publicationSlotSuffix(slot));
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

inline std::string slotPath(const std::string& project, int voice, int slot,
                            GroovePuterMaterial::PublicationSlot pubSlot) {
  return slotPath(project,
                  {static_cast<uint8_t>(voice), static_cast<uint8_t>(slot)},
                  pubSlot);
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
    GroovePuterMaterial::MaterialAddress address, Buffer& out,
    uint32_t* outGeneration = nullptr) {
  if (!fs.available() || !GroovePuterMaterial::materialAddressInRange(address)) {
    return false;
  }

  // 1. Check dual-generation slots A and B
  const std::string pathA = slotPath(project, address, GroovePuterMaterial::PublicationSlot::SlotA);
  const std::string pathB = slotPath(project, address, GroovePuterMaterial::PublicationSlot::SlotB);
  uint32_t genA = 0, genB = 0;
  Buffer bufA{}, bufB{};
  bool validA = false, validB = false;

  std::vector<uint8_t> blobA;
  if (fs.exists(pathA.c_str()) && fs.read(pathA.c_str(), blobA)) {
    validA = MelodyStore::decode(blobA.data(), blobA.size(), bufA, &genA);
  }
  std::vector<uint8_t> blobB;
  if (fs.exists(pathB.c_str()) && fs.read(pathB.c_str(), blobB)) {
    validB = MelodyStore::decode(blobB.data(), blobB.size(), bufB, &genB);
  }

  if (validA && validB) {
    if (genA >= genB) {
      out = bufA;
      if (outGeneration) *outGeneration = genA;
      return true;
    } else {
      out = bufB;
      if (outGeneration) *outGeneration = genB;
      return true;
    }
  }
  if (validA) {
    out = bufA;
    if (outGeneration) *outGeneration = genA;
    return true;
  }
  if (validB) {
    out = bufB;
    if (outGeneration) *outGeneration = genB;
    return true;
  }

  // 2. Fallback to legacy single file
  const std::string path = finalPath(project, address);
  if (!fs.exists(path.c_str())) return false;
  std::vector<uint8_t> blob;
  if (!fs.read(path.c_str(), blob)) return false;
  return MelodyStore::decode(blob.data(), blob.size(), out, outGeneration);
}

inline bool loadResident(const FileSystem& fs, const std::string& project,
                         int voice, int slot, Buffer& out) {
  if (!GroovePuterMaterial::residentSlotInRange(voice, slot)) return false;
  return loadMaterial(
      fs, project,
      {static_cast<uint8_t>(voice), static_cast<uint8_t>(slot)}, out);
}

inline Error commitMelodyCandidate(
    FileSystem& fs, const std::string& project,
    GroovePuterMaterial::MaterialAddress address,
    const Buffer& candidate,
    uint32_t& committedGeneration) {
  if (!fs.available()) return Error::NoStorage;
  if (!GroovePuterMaterial::materialAddressInRange(address)) return Error::BadSlot;

  // Probe active slot and generation
  const std::string pathA = slotPath(project, address, GroovePuterMaterial::PublicationSlot::SlotA);
  const std::string pathB = slotPath(project, address, GroovePuterMaterial::PublicationSlot::SlotB);
  uint32_t genA = 0, genB = 0;
  Buffer temp{};
  std::vector<uint8_t> blob;
  bool validA = fs.exists(pathA.c_str()) && fs.read(pathA.c_str(), blob) &&
                MelodyStore::decode(blob.data(), blob.size(), temp, &genA);
  bool validB = fs.exists(pathB.c_str()) && fs.read(pathB.c_str(), blob) &&
                MelodyStore::decode(blob.data(), blob.size(), temp, &genB);

  GroovePuterMaterial::PublicationSlot activeSlot = GroovePuterMaterial::PublicationSlot::None;
  uint32_t currentGen = 0;
  if (validA && validB) {
    if (genA >= genB) { activeSlot = GroovePuterMaterial::PublicationSlot::SlotA; currentGen = genA; }
    else { activeSlot = GroovePuterMaterial::PublicationSlot::SlotB; currentGen = genB; }
  } else if (validA) {
    activeSlot = GroovePuterMaterial::PublicationSlot::SlotA; currentGen = genA;
  } else if (validB) {
    activeSlot = GroovePuterMaterial::PublicationSlot::SlotB; currentGen = genB;
  }

  const auto targetSlot = (activeSlot == GroovePuterMaterial::PublicationSlot::SlotA)
      ? GroovePuterMaterial::PublicationSlot::SlotB
      : GroovePuterMaterial::PublicationSlot::SlotA;
  const uint32_t nextGen = currentGen + 1;

  std::vector<uint8_t> encoded;
  if (!MelodyStore::encode(candidate, encoded, nextGen)) return Error::EncodeFailed;

  const std::string targetPath = slotPath(project, address, targetSlot);
  LOG_DEBUG("[MELODY_CANDIDATE] address={v:%u, g:%u} activeSlot=%d currentGen=%u targetSlot=%d nextGen=%u path=%s\n",
            address.voice, address.globalSlot, static_cast<int>(activeSlot), currentGen,
            static_cast<int>(targetSlot), nextGen, targetPath.c_str());
  fs.remove(targetPath.c_str());
  if (!fs.write(targetPath.c_str(), encoded.data(), encoded.size())) {
    LOG_DEBUG("[MELODY_CANDIDATE] fs.write FAILED path=%s\n", targetPath.c_str());
    return Error::WriteFailed;
  }

  // Readback verify
  std::vector<uint8_t> verify;
  Buffer restored{};
  uint32_t restoredGen = 0;
  if (!fs.read(targetPath.c_str(), verify) ||
      !MelodyStore::decode(verify.data(), verify.size(), restored, &restoredGen) ||
      restoredGen != nextGen ||
      !sameMelody(candidate, restored)) {
    LOG_DEBUG("[MELODY_CANDIDATE] verify FAILED: read=%d restoredGen=%u expectedGen=%u same=%d\n",
              fs.exists(targetPath.c_str()) ? 1 : 0, restoredGen, nextGen,
              sameMelody(candidate, restored) ? 1 : 0);
    fs.remove(targetPath.c_str());
    return Error::VerifyFailed;
  }

  committedGeneration = nextGen;
  return Error::None;
}

inline Error promoteResident(
    FileSystem& fs, const std::string& project, Scene& scene,
    const GroovePuterMaterial::MaterialReference& reference, int residentSlot,
    const Buffer& candidate) {
  using namespace GroovePuterMaterial;

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

  if (!fs.available()) return Error::NoStorage;

  if (residentKind(scene, address.voice, residentSlot) == MaterialKind::Melody) {
    return Error::AlreadyMelody;
  }

  std::vector<uint8_t> blob;
  if (!MelodyStore::encode(candidate, blob)) return Error::EncodeFailed;

  const std::string temp = tempPath(project, address);
  if (!fs.write(temp.c_str(), blob.data(), blob.size())) {
    return Error::WriteFailed;
  }

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

  if (!setResidentKind(scene, address.voice, residentSlot, MaterialKind::Melody)) {
    return Error::BadSlot;
  }
  return Error::None;
}

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
