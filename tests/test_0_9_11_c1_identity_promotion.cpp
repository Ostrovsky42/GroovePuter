#include <cassert>
#include <map>
#include <string>
#include <vector>

#include "src/state/material_identity.h"
#include "src/state/melody_promotion.h"

namespace {
using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

struct FakeFs final : MelodyPromotion::FileSystem {
  std::map<std::string, std::vector<uint8_t>> files;
  bool available() const override { return true; }
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

Buffer candidate(uint8_t note) {
  Buffer out{}; out.lengthTicks = PhraseRuntime::kTicksPerBar; out.count = 1;
  out.events[0] = {0, 24 * 16, note, 100, 100, 0, 0, 0};
  return out;
}
}

int main() {
  constexpr MaterialAddress address{0, 21};
  constexpr MaterialId idM{201};
  constexpr MaterialId idN{202};
  constexpr MaterialReference refM{address, idM};
  constexpr MaterialReference refN{address, idN};
  const int slot = GroovePuterMaterial::residentSlotFor(address);
  assert(slot == 5);

  Scene scene{};
  FakeFs fs;
  scene.materialSlots[address.voice][slot].kind = MaterialKind::Pattern;
  scene.materialSlots[address.voice][slot].id = idM;
  const Buffer preparedM = candidate(60);

  scene.materialSlots[address.voice][slot].id = idN;
  auto stale = MelodyPromotion::promoteResident(fs, "c1", scene, refM, slot, preparedM);
  assert(stale == MelodyPromotion::Error::IdentityMismatch);
  assert(scene.materialSlots[address.voice][slot].kind == MaterialKind::Pattern);
  assert(fs.files.empty());

  const Buffer preparedN = candidate(72);
  auto live = MelodyPromotion::promoteResident(fs, "c1", scene, refN, slot, preparedN);
  assert(live == MelodyPromotion::Error::None);
  assert(scene.materialSlots[address.voice][slot].kind == MaterialKind::Melody);

  auto bare = MelodyPromotion::promoteResident(fs, "c1", scene, address, slot, preparedN);
  assert(bare == MelodyPromotion::Error::InvalidReference);
  return 0;
}
