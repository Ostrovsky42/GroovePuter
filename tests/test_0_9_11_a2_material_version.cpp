#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "scenes.h"
#include "src/state/material_resolution.h"
#include "src/state/material_version.h"

namespace {

using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialResolutionStatus;
using GroovePuterMaterial::MaterialVersionToken;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "A2 VERSION FAIL: %s\n", message);
  ++g_failures;
}

struct FakeFs : MelodyPromotion::FileSystem {
  std::map<std::string, std::vector<uint8_t>> files;
  bool present = true;

  bool available() const override { return present; }
  bool exists(const char* path) const override {
    return files.find(path) != files.end();
  }
  bool write(const char* path, const uint8_t* data, size_t length) override {
    if (!present) return false;
    files[path].assign(data, data + length);
    return true;
  }
  bool read(const char* path, std::vector<uint8_t>& out) const override {
    if (!present) return false;
    const auto it = files.find(path);
    if (it == files.end()) return false;
    out = it->second;
    return true;
  }
  bool rename(const char* from, const char* to) override {
    if (!present) return false;
    const auto it = files.find(from);
    if (it == files.end()) return false;
    files[to] = it->second;
    files.erase(it);
    return true;
  }
  bool remove(const char* path) override { return files.erase(path) > 0; }
};

constexpr MaterialAddress kAddress{0, 5};
constexpr int kPage = 0;
const std::string kProject = "a2-version";

Buffer makeMelody(uint8_t note) {
  Buffer result{};
  result.count = 1;
  result.lengthTicks = PhraseRuntime::kTicksPerBar;
  result.events[0] = {0, 24 * 16, note, 100, 100, 0, 0, 0};
  return result;
}

SynthPattern& residentPattern(Scene& scene, MaterialAddress address) {
  const int bank = songPatternBank(address.globalSlot);
  const int index = songPatternIndexInBank(address.globalSlot);
  return address.voice == 0 ? scene.synthABanks[bank].patterns[index]
                            : scene.synthBBanks[bank].patterns[index];
}

}  // namespace

int main() {
  static_assert(sizeof(MaterialVersionToken) == 8,
                "A2 exact material version token must remain eight bytes");

  // Same address, different accepted Pattern content must be distinguishable.
  {
    Scene scene{};
    SynthPattern& pattern = residentPattern(scene, kAddress);
    const MaterialVersionToken before =
        GroovePuterMaterial::versionForPattern(pattern);
    pattern.steps[0].note = 60;
    pattern.steps[0].accent = true;
    const MaterialVersionToken after =
        GroovePuterMaterial::versionForPattern(pattern);
    expect(before != after,
           "Pattern edit did not change the exact material version token");
  }

  // Unrelated Scene state is not part of this material's exact version.
  {
    FakeFs fs;
    Scene scene{};
    residentPattern(scene, kAddress).steps[0].note = 64;
    Buffer scratch{};
    const auto first = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, scratch);
    scene.masterVolume = 0.17f;
    const auto second = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, scratch);
    expect(first.status == MaterialResolutionStatus::ResolvedPattern &&
               second.status == MaterialResolutionStatus::ResolvedPattern,
           "Pattern setup did not resolve");
    expect(first.version == second.version,
           "unrelated Scene change altered material version");
  }

  // Persisted Melody and its decoded reload must produce the same exact token.
  {
    FakeFs fs;
    Scene scene{};
    const Buffer candidate = makeMelody(67);
    expect(MelodyPromotion::promoteResident(
               fs, kProject, scene, kPage, kAddress, candidate) ==
               MelodyPromotion::Error::None,
           "test promotion failed");

    Buffer firstBuffer{};
    const auto first = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, firstBuffer);
    expect(first.status == MaterialResolutionStatus::ResolvedMelody,
           "published Melody did not resolve");
    expect(first.version == GroovePuterMaterial::versionForMelody(candidate),
           "resolver version does not match canonical Melody content");

    Buffer secondBuffer{};
    const auto second = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, secondBuffer);
    expect(second.version == first.version,
           "same persisted Melody changed version across reload");

    fs.files[MelodyPromotion::finalPath(kProject, kAddress)].clear();
    Buffer failed{};
    const auto broken = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, failed);
    expect(!broken.isResolved(), "broken Melody unexpectedly resolved");
    expect(!broken.hasVersion(),
           "unresolved material exposed a trustworthy exact version");
  }

  if (g_failures == 0) {
    std::printf("0.9.11 A2 material version: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "0.9.11 A2 material version: %d failure(s)\n",
               g_failures);
  return 1;
}
