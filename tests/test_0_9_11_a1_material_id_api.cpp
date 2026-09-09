// 0.9.11 A1 target contract: persistent material identity is global while
// Scene mutation remains an explicitly resident-page operation.

#include <cstdio>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "src/state/melody_promotion.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using MelodyPromotion::Error;
using GroovePuterMaterial::MaterialAddress;

static_assert(sizeof(MaterialAddress) == 2,
              "MaterialAddress must remain a two-byte embedded value");
static_assert(std::is_trivially_copyable<MaterialAddress>::value,
              "MaterialAddress must remain trivially copyable");

struct FakeFs : MelodyPromotion::FileSystem {
  std::map<std::string, std::vector<uint8_t>> files;

  bool available() const override { return true; }
  bool exists(const char* path) const override {
    return files.find(path) != files.end();
  }
  bool write(const char* path, const uint8_t* data, size_t length) override {
    files[path].assign(data, data + length);
    return true;
  }
  bool read(const char* path, std::vector<uint8_t>& out) const override {
    const auto it = files.find(path);
    if (it == files.end()) return false;
    out = it->second;
    return true;
  }
  bool rename(const char* from, const char* to) override {
    const auto it = files.find(from);
    if (it == files.end()) return false;
    files[to] = it->second;
    files.erase(it);
    return true;
  }
  bool remove(const char* path) override { return files.erase(path) > 0; }
};

Buffer melodyWithNote(uint8_t note) {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0] = {0, 24 * 16, note, 100, 100, 0, 0, 0};
  return melody;
}

}  // namespace

int main() {
  constexpr MaterialAddress page0Address{0, 5};
  constexpr MaterialAddress page1Address{0, 21};

  const std::string project = "a1-api";
  if (MelodyPromotion::finalPath(project, page0Address) ==
      MelodyPromotion::finalPath(project, page1Address)) {
    std::fprintf(stderr, "A1 API FAIL: distinct global slots share one path\n");
    return 1;
  }

  FakeFs fs;
  Scene page0{};
  Scene page1{};

  if (MelodyPromotion::promoteResident(fs, project, page0, 0, page0Address,
                                       melodyWithNote(60)) != Error::None ||
      MelodyPromotion::promoteResident(fs, project, page1, 1, page1Address,
                                       melodyWithNote(72)) != Error::None) {
    std::fprintf(stderr, "A1 API FAIL: valid global material promotion failed\n");
    return 1;
  }

  // page1Address belongs to page 1. A page-0 Scene must not be mutated through it.
  Scene wrongPage{};
  if (MelodyPromotion::promoteResident(fs, project, wrongPage, 0,
                                       page1Address, melodyWithNote(84)) !=
      Error::BadSlot) {
    std::fprintf(stderr,
                 "A1 API FAIL: non-resident MaterialAddress was accepted by Scene\n");
    return 1;
  }

  if (GroovePuterMaterial::residentKind(page0, 0, 5) !=
          GroovePuterMaterial::MaterialKind::Melody ||
      GroovePuterMaterial::residentKind(page1, 0, 5) !=
          GroovePuterMaterial::MaterialKind::Melody) {
    std::fprintf(stderr,
                 "A1 API FAIL: address did not publish resident descriptor\n");
    return 1;
  }

  std::printf("0.9.11 A1 MaterialAddress API: PASS\n");
  return 0;
}
