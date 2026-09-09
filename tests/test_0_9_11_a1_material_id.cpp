// 0.9.11 A1 characterization: persistent material identity must not alias
// resident slots from different pages.
//
// This is intentionally written against the current production API. Two Scene
// objects model two different resident pages of the same project. Today both
// page-local slot 5 values address the same melody path, so the second publish
// overwrites the first. A1 must make those materials distinct before callers
// start depending on a canonical MaterialId.

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "src/state/melody_promotion.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using MelodyPromotion::Error;

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

  constexpr int kVoice = 0;
  constexpr int kLocalSlot = 5;
  const std::string project = "a1-page-alias";

  const Error first = MelodyPromotion::promoteResident(
      fs, project, page0, kVoice, kLocalSlot, melodyWithNote(60));
  const Error second = MelodyPromotion::promoteResident(
      fs, project, page1, kVoice, kLocalSlot, melodyWithNote(72));

  if (first != Error::None || second != Error::None) {
    std::fprintf(stderr,
                 "A1 RED setup failed: both page-local promotions must succeed\n");
    return 2;
  }

  // Correct behavior requires two independently addressable payloads. Current
  // production leaves one file because page is absent from persistent identity.
  if (fs.files.size() != 2u) {
    std::fprintf(stderr,
                 "A1 RED: same local slot on different pages aliases one melody "
                 "payload (files=%zu)\n",
                 fs.files.size());
    return 1;
  }

  std::printf("0.9.11 A1 material identity: PASS\n");
  return 0;
}
