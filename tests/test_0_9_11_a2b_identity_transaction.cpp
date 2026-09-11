#include <cstdio>
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

Buffer makeCandidate(uint8_t note) {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0] = {0, 24 * 16, note, 100, 100, 0, 0, 0};
  return melody;
}

}  // namespace

int main() {
  constexpr MaterialAddress address{0, 21};
  constexpr MaterialId idM{201};
  constexpr MaterialId idN{202};
  const MaterialReference refM{address, idM};
  const int residentSlot = GroovePuterMaterial::residentSlotFor(address);

  if (residentSlot != 5) {
    std::fprintf(stderr,
                 "A2B_SETUP_FAIL: global slot 21 did not project to resident slot 5\n");
    return 2;
  }

  Scene scene{};
  FakeFs fs;
  scene.materialSlots[address.voice][residentSlot].kind = MaterialKind::Pattern;
  scene.materialSlots[address.voice][residentSlot].id = idM;

  // Candidate 60 was prepared while refM was live.
  const Buffer candidateM = makeCandidate(60);

  // ABA/reuse: the same address/resident coordinate now belongs to N before
  // promotion starts. A2 proved this makes refM stale. A2-B must carry that
  // identity authority into the mutating transaction instead of falling back
  // to address-only admission.
  scene.materialSlots[address.voice][residentSlot].id = idN;
  if (GroovePuterMaterial::materialReferenceMatches(
          refM, address,
          scene.materialSlots[address.voice][residentSlot].id)) {
    std::fprintf(stderr,
                 "A2B_SETUP_FAIL: stale refM unexpectedly still matches idN\n");
    return 2;
  }

  const auto error = MelodyPromotion::promoteResident(
      fs, "a2b-identity-transaction", scene, address, residentSlot, candidateM);

  Buffer stored{};
  const bool readable = MelodyPromotion::loadMaterial(
      fs, "a2b-identity-transaction", address, stored);
  const bool staleCandidateWasPublished =
      error == MelodyPromotion::Error::None && readable && stored.count == 1 &&
      stored.events[0].note == 60 &&
      scene.materialSlots[address.voice][residentSlot].id == idN &&
      scene.materialSlots[address.voice][residentSlot].kind == MaterialKind::Melody;

  if (staleCandidateWasPublished) {
    std::fprintf(
        stderr,
        "A2B_STALE_PROMOTION_RED: stale {address,idM} candidate was published after the resident identity changed to idN; idN was marked Melody with idM payload\n");
    return 1;
  }

  std::fprintf(stderr,
               "A2B_SETUP_FAIL: current production did not reproduce the expected stale-promotion defect\n");
  return 2;
}
