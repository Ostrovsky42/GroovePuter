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

int gFailures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "A2B_IDENTITY_TRANSACTION_FAIL: %s\n", message);
  ++gFailures;
}

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
  constexpr MaterialReference refM{address, idM};
  constexpr MaterialReference refN{address, idN};
  const std::string project = "a2b-identity-transaction";
  const int residentSlot = GroovePuterMaterial::residentSlotFor(address);

  expect(residentSlot == 5,
         "global slot 21 did not project to resident slot 5");

  Scene scene{};
  FakeFs fs;
  scene.materialSlots[address.voice][residentSlot].kind = MaterialKind::Pattern;
  scene.materialSlots[address.voice][residentSlot].id = idM;

  // Candidate M was prepared while refM was live. The address is then reused
  // by N before promotion starts: this is the exact RED witness.
  const Buffer candidateM = makeCandidate(60);
  scene.materialSlots[address.voice][residentSlot].id = idN;
  expect(!GroovePuterMaterial::materialReferenceMatches(
             refM, address,
             scene.materialSlots[address.voice][residentSlot].id),
         "stale refM unexpectedly still matches idN");

  const auto staleError = MelodyPromotion::promoteResident(
      fs, project, scene, refM, residentSlot, candidateM);
  expect(staleError == MelodyPromotion::Error::IdentityMismatch,
         "stale refM was not rejected with IdentityMismatch");
  expect(fs.files.empty(),
         "stale refM mutated storage before identity rejection");
  expect(scene.materialSlots[address.voice][residentSlot].id == idN,
         "stale promotion changed replacement identity N");
  expect(scene.materialSlots[address.voice][residentSlot].kind ==
             MaterialKind::Pattern,
         "stale promotion changed replacement N to Melody");
  expect(!fs.exists(MelodyPromotion::tempPath(project, address).c_str()),
         "stale promotion created a temporary payload");
  expect(!fs.exists(MelodyPromotion::finalPath(project, address).c_str()),
         "stale promotion published a durable payload");

  // Bare coordinates remain source-compatible but are no longer mutation
  // authority. This prevents an older caller from bypassing the reference gate.
  const auto bareError = MelodyPromotion::promoteResident(
      fs, project, scene, address, residentSlot, candidateM);
  expect(bareError == MelodyPromotion::Error::InvalidReference,
         "bare MaterialAddress remained a mutation-authority bypass");
  expect(fs.files.empty(),
         "bare-address compatibility entry point mutated storage");
  expect(scene.materialSlots[address.voice][residentSlot].kind ==
             MaterialKind::Pattern,
         "bare-address compatibility entry point changed the descriptor");

  // The replacement material can still promote normally when its own live
  // identity is presented. The previous rejection must not poison the path.
  const Buffer candidateN = makeCandidate(72);
  const auto liveError = MelodyPromotion::promoteResident(
      fs, project, scene, refN, residentSlot, candidateN);
  expect(liveError == MelodyPromotion::Error::None,
         "live refN promotion was refused after stale refM rejection");
  expect(scene.materialSlots[address.voice][residentSlot].id == idN,
         "live promotion changed MaterialId");
  expect(scene.materialSlots[address.voice][residentSlot].kind ==
             MaterialKind::Melody,
         "live refN did not publish Melody kind");

  Buffer stored{};
  const bool readable =
      MelodyPromotion::loadMaterial(fs, project, address, stored);
  expect(readable, "live refN durable payload could not be loaded");
  if (readable) {
    expect(stored.count == 1 && stored.events[0].note == 72,
           "durable payload is not N's candidate after live promotion");
  }

  if (gFailures == 0) {
    std::puts(
        "A2B_IDENTITY_TRANSACTION_GREEN: stale ref rejected before storage mutation; bare-address bypass closed; live replacement identity promotes normally");
    std::puts("0.9.11 A2-B identity-bound promotion transaction: PASS");
    return 0;
  }

  std::fprintf(stderr,
               "0.9.11 A2-B identity-bound promotion transaction: %d failure(s)\n",
               gFailures);
  return 1;
}
