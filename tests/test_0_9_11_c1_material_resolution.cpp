#include <cassert>
#include <map>
#include <string>
#include <vector>

#include "src/state/material_resolution.h"

namespace {
using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialResolutionStatus;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct FakeFs final : MelodyPromotion::FileSystem {
  bool online = true;
  std::map<std::string, std::vector<uint8_t>> files;
  bool available() const override { return online; }
  bool exists(const char* path) const override { return files.find(path) != files.end(); }
  bool write(const char* path, const uint8_t* data, size_t length) override {
    files[path].assign(data, data + length); return true;
  }
  bool read(const char* path, std::vector<uint8_t>& out) const override {
    auto it = files.find(path); if (it == files.end()) return false; out = it->second; return true;
  }
  bool rename(const char* from, const char* to) override {
    auto it = files.find(from); if (it == files.end()) return false;
    files[to] = it->second; files.erase(it); return true;
  }
  bool remove(const char* path) override { return files.erase(path) > 0; }
};
}

int main() {
  constexpr MaterialAddress address{0, 0};
  constexpr MaterialReference ref{address, MaterialId{501}};
  Scene scene{};
  scene.materialSlots[0][0].id = ref.id;
  scene.materialSlots[0][0].kind = MaterialKind::Pattern;
  scene.synthABanks[0].patterns[0].steps[0].note = 60;
  FakeFs fs;
  Buffer out{};

  auto pattern = GroovePuterMaterial::resolveMaterial(fs, "c1", scene, 0, ref, out);
  assert(pattern.status == MaterialResolutionStatus::ResolvedPattern);
  assert(pattern.kind == MaterialKind::Pattern);
  assert(pattern.hasVersion());

  MaterialReference invalid{address, MaterialId{}};
  auto noId = GroovePuterMaterial::resolveMaterial(fs, "c1", scene, 0, invalid, out);
  assert(noId.status == MaterialResolutionStatus::InvalidReference);
  assert(!noId.hasVersion());

  scene.materialSlots[0][0].id = MaterialId{502};
  auto stale = GroovePuterMaterial::resolveMaterial(fs, "c1", scene, 0, ref, out);
  assert(stale.status == MaterialResolutionStatus::IdentityMismatch);
  assert(!stale.hasVersion());

  scene.materialSlots[0][0].id = ref.id;
  scene.materialSlots[0][0].kind = MaterialKind::Melody;
  fs.online = false;
  auto offline = GroovePuterMaterial::resolveMaterial(fs, "c1", scene, 0, ref, out);
  assert(offline.status == MaterialResolutionStatus::StorageUnavailable);
  assert(!offline.hasVersion());

  fs.online = true;
  auto missing = GroovePuterMaterial::resolveMaterial(fs, "c1", scene, 0, ref, out);
  assert(missing.status == MaterialResolutionStatus::MissingPayload);
  assert(!missing.hasVersion());
  return 0;
}
