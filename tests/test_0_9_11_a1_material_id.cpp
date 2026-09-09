// 0.9.11 A1 characterization: persistent material identity must not alias
// resident slots from different pages.

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "src/state/melody_promotion.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using MelodyPromotion::Error;
using GroovePuterMaterial::MaterialId;

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
  FakeFs fs;
  Scene page0{};
  Scene page1{};

  constexpr MaterialId page0Id{0, 5};
  constexpr MaterialId page1Id{0, 21};
  const std::string project = "a1-page-alias";

  const Error first = MelodyPromotion::promoteResident(
      fs, project, page0, 0, page0Id, melodyWithNote(60));
  const Error second = MelodyPromotion::promoteResident(
      fs, project, page1, 1, page1Id, melodyWithNote(72));

  if (first != Error::None || second != Error::None) {
    std::fprintf(stderr,
                 "A1 setup failed: both global material promotions must succeed\n");
    return 2;
  }

  if (fs.files.size() != 2u) {
    std::fprintf(stderr,
                 "A1 FAIL: distinct global materials do not own two payloads "
                 "(files=%zu)\n",
                 fs.files.size());
    return 1;
  }

  Buffer back0{};
  Buffer back1{};
  if (!MelodyPromotion::loadMaterial(fs, project, page0Id, back0) ||
      !MelodyPromotion::loadMaterial(fs, project, page1Id, back1) ||
      back0.count != 1 || back1.count != 1 || back0.events[0].note != 60 ||
      back1.events[0].note != 72) {
    std::fprintf(stderr,
                 "A1 FAIL: cross-page materials are not independently loadable\n");
    return 1;
  }

  std::printf("0.9.11 A1 material identity: PASS\n");
  return 0;
}
