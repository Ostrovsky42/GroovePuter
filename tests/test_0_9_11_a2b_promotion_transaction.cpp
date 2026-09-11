#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "src/state/material_resolution.h"
#include "src/state/material_version.h"
#include "src/state/melody_promotion.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialVersionToken;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "A2-B FAIL: %s\n", message);
  ++g_failures;
}

struct TransactionFs final : MelodyPromotion::FileSystem {
  std::map<std::string, std::vector<uint8_t>> files;
  bool present = true;
  bool failWrite = false;
  bool failRename = false;
  bool alterVerifiedRead = false;

  bool available() const override { return present; }

  bool exists(const char* path) const override {
    return files.find(path) != files.end();
  }

  bool write(const char* path, const uint8_t* data, size_t length) override {
    if (failWrite) return false;
    files[path].assign(data, data + length);
    return true;
  }

  bool read(const char* path, std::vector<uint8_t>& out) const override {
    auto it = files.find(path);
    if (it == files.end()) return false;
    out = it->second;

    // Simulate a storage layer that reports a successful write but returns a
    // different, still-well-formed canonical melody on read-back. Recompute
    // the payload CRC so MelodyStore::decode succeeds and the promotion
    // transaction itself must reject the semantic mismatch.
    if (alterVerifiedRead && out.size() >= MelodyStore::kHeaderBytes + MelodyStore::kEventBytes) {
      out[MelodyStore::kHeaderBytes + 4] ^= 1u;  // first event note
      const uint8_t* payload = out.data() + MelodyStore::kHeaderBytes;
      const size_t payloadSize = out.size() - MelodyStore::kHeaderBytes;
      const uint32_t crc = MelodyStore::crc32(payload, payloadSize);
      for (int i = 0; i < 4; ++i) {
        out[12 + i] = static_cast<uint8_t>((crc >> (i * 8)) & 0xFFu);
      }
    }
    return true;
  }

  bool rename(const char* from, const char* to) override {
    if (failRename) return false;
    auto it = files.find(from);
    if (it == files.end()) return false;
    files[to] = it->second;
    files.erase(it);
    return true;
  }

  bool remove(const char* path) override {
    return files.erase(path) > 0;
  }
};

Buffer makeCandidate() {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 2;
  melody.events[0] = {0, 24 * 16, 60, 100, 100, 0, 0, 0};
  melody.events[1] = {96, 24 * 16, 67, 96, 100, 0, 0, 0};
  return melody;
}

MaterialVersionToken acceptedPatternVersion(const Scene& scene,
                                            MaterialAddress address) {
  return GroovePuterMaterial::versionForPattern(
      GroovePuterMaterial::residentPatternFor(scene, address));
}

void test_failed_write_preserves_accepted() {
  TransactionFs fs;
  fs.failWrite = true;
  Scene scene{};
  MaterialAddress address{0, 0};
  scene.synthABanks[0].patterns[0].steps[0].note = 48;
  const MaterialVersionToken before = acceptedPatternVersion(scene, address);

  const auto error = MelodyPromotion::promoteResident(
      fs, "a2b", scene, 0, address, makeCandidate());

  expect(error == MelodyPromotion::Error::WriteFailed,
         "failed write was not reported as WriteFailed");
  expect(GroovePuterMaterial::residentKind(scene, 0, 0) == MaterialKind::Pattern,
         "failed write changed resident descriptor");
  expect(acceptedPatternVersion(scene, address) == before,
         "failed write changed accepted Pattern version");
  expect(!fs.exists(MelodyPromotion::finalPath("a2b", address).c_str()),
         "failed write published a final payload");
}

void test_failed_verify_preserves_accepted() {
  TransactionFs fs;
  fs.alterVerifiedRead = true;
  Scene scene{};
  MaterialAddress address{0, 0};
  scene.synthABanks[0].patterns[0].steps[0].note = 49;
  const MaterialVersionToken before = acceptedPatternVersion(scene, address);

  const auto error = MelodyPromotion::promoteResident(
      fs, "a2b", scene, 0, address, makeCandidate());

  expect(error == MelodyPromotion::Error::VerifyFailed,
         "well-formed but different read-back was not rejected");
  expect(GroovePuterMaterial::residentKind(scene, 0, 0) == MaterialKind::Pattern,
         "verification failure changed resident descriptor");
  expect(acceptedPatternVersion(scene, address) == before,
         "verification failure changed accepted Pattern version");
  expect(!fs.exists(MelodyPromotion::finalPath("a2b", address).c_str()),
         "verification failure published a final payload");
  expect(!fs.exists(MelodyPromotion::tempPath("a2b", address).c_str()),
         "verification failure left a temporary payload referenced by the transaction");
}

void test_failed_publish_preserves_accepted() {
  TransactionFs fs;
  fs.failRename = true;
  Scene scene{};
  MaterialAddress address{0, 0};
  scene.synthABanks[0].patterns[0].steps[0].note = 50;
  const MaterialVersionToken before = acceptedPatternVersion(scene, address);

  const auto error = MelodyPromotion::promoteResident(
      fs, "a2b", scene, 0, address, makeCandidate());

  expect(error == MelodyPromotion::Error::PublishFailed,
         "failed publish was not reported as PublishFailed");
  expect(GroovePuterMaterial::residentKind(scene, 0, 0) == MaterialKind::Pattern,
         "failed publish changed resident descriptor");
  expect(acceptedPatternVersion(scene, address) == before,
         "failed publish changed accepted Pattern version");
  expect(!fs.exists(MelodyPromotion::finalPath("a2b", address).c_str()),
         "failed publish created a final payload");
  expect(!fs.exists(MelodyPromotion::tempPath("a2b", address).c_str()),
         "failed publish left a temporary payload after cleanup");
}

void test_success_publishes_verified_version() {
  TransactionFs fs;
  Scene scene{};
  MaterialAddress address{0, 0};
  const Buffer candidate = makeCandidate();
  const MaterialVersionToken expected =
      GroovePuterMaterial::versionForMelody(candidate);

  const auto error = MelodyPromotion::promoteResident(
      fs, "a2b", scene, 0, address, candidate);

  expect(error == MelodyPromotion::Error::None,
         "valid material-scoped Melody promotion failed");
  expect(GroovePuterMaterial::residentKind(scene, 0, 0) == MaterialKind::Melody,
         "successful publish did not flip descriptor last");

  Buffer loaded{};
  expect(MelodyPromotion::loadMaterial(fs, "a2b", address, loaded),
         "published payload cannot be loaded");
  expect(GroovePuterMaterial::versionForMelody(loaded) == expected,
         "published payload does not bind the candidate exact-content version");
  expect(!fs.exists(MelodyPromotion::tempPath("a2b", address).c_str()),
         "successful publish left a temporary payload");
}

}  // namespace

int main() {
  test_failed_write_preserves_accepted();
  test_failed_verify_preserves_accepted();
  test_failed_publish_preserves_accepted();
  test_success_publishes_verified_version();

  if (g_failures == 0) {
    std::printf("0.9.11 A2-B promotion transaction: PASS\n");
    return 0;
  }

  std::fprintf(stderr, "0.9.11 A2-B promotion transaction: %d failure(s)\n",
               g_failures);
  return 1;
}
