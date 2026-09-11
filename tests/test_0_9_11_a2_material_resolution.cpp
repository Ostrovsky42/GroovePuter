#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "scenes.h"
#include "src/state/material_resolution.h"

namespace {

using GroovePuterMaterial::MaterialAddress;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialResolution;
using GroovePuterMaterial::MaterialResolutionStatus;
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "A2 RESOLUTION FAIL: %s\n", message);
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

Buffer makeMelody(uint8_t note = 60) {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0] = {0, 24 * 16, note, 100, 100, 0, 0, 0};
  return melody;
}

constexpr MaterialAddress kAddress{0, 5};
constexpr int kPage = 0;
const std::string kProject = "a2-resolution";

}  // namespace

int main() {
  static_assert(sizeof(MaterialResolution) <= 12,
                "control-side resolution result must stay a small value");

  {
    FakeFs fs;
    Scene scene{};
    Buffer out{};
    const MaterialResolution result = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, out);
    expect(result.status == MaterialResolutionStatus::ResolvedPattern,
           "resident Pattern did not resolve explicitly");
    expect(result.kind == MaterialKind::Pattern,
           "resolved Pattern reported the wrong kind");
    expect(result.isResolved(), "resolved Pattern reported unresolved");
    expect(result.hasVersion(), "resolved Pattern has no exact version");
  }

  {
    FakeFs fs;
    Scene scene{};
    Buffer out{};
    constexpr MaterialAddress otherPage{0, 17};
    const MaterialResolution result = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, otherPage, out);
    expect(result.status == MaterialResolutionStatus::NotResident,
           "non-resident address collapsed to a musical kind");
    expect(!result.isResolved(), "non-resident address reported resolved");
    expect(!result.hasVersion(), "non-resident address exposed a version");
  }

  {
    FakeFs fs;
    Scene scene{};
    Buffer out{};
    constexpr MaterialAddress invalidVoice{255, 5};
    const MaterialResolution result = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, invalidVoice, out);
    expect(result.status == MaterialResolutionStatus::InvalidAddress,
           "invalid address did not fail explicitly");
    expect(!result.isResolved(), "invalid address reported resolved");
  }

  {
    FakeFs fs;
    Scene scene{};
    Buffer out{};
    expect(GroovePuterMaterial::setMaterialKind(
               scene, kAddress.voice, kAddress.globalSlot, kPage,
               MaterialKind::Melody),
           "test could not mark the resident slot Melody");
    const MaterialResolution result = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, out);
    expect(result.status == MaterialResolutionStatus::MissingPayload,
           "missing Melody payload did not fail explicitly");
    expect(!result.isResolved(), "missing Melody payload reported resolved");
    expect(!result.hasVersion(), "missing Melody payload exposed a version");
  }

  {
    FakeFs fs;
    Scene scene{};
    const Buffer candidate = makeMelody(67);
    expect(MelodyPromotion::promoteResident(
               fs, kProject, scene, kPage, kAddress, candidate) ==
               MelodyPromotion::Error::None,
           "test promotion failed");
    Buffer out{};
    const MaterialResolution result = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, out);
    expect(result.status == MaterialResolutionStatus::ResolvedMelody,
           "published Melody did not resolve");
    expect(result.kind == MaterialKind::Melody,
           "resolved Melody reported the wrong kind");
    expect(result.isResolved(), "resolved Melody reported unresolved");
    expect(result.hasVersion(), "resolved Melody has no exact version");
    expect(out.count == 1 && out.events[0].note == 67,
           "resolver did not return the published Melody events");
  }

  {
    FakeFs fs;
    Scene scene{};
    expect(MelodyPromotion::promoteResident(
               fs, kProject, scene, kPage, kAddress, makeMelody()) ==
               MelodyPromotion::Error::None,
           "test promotion failed");
    const std::string path = MelodyPromotion::finalPath(kProject, kAddress);
    fs.files[path] = {0x00, 0x01, 0x02, 0x03};
    Buffer out{};
    const MaterialResolution result = GroovePuterMaterial::resolveMaterial(
        fs, kProject, scene, kPage, kAddress, out);
    expect(result.status == MaterialResolutionStatus::CorruptPayload,
           "corrupt Melody payload was not distinguished from missing data");
    expect(!result.isResolved(), "corrupt Melody payload reported resolved");
    expect(!result.hasVersion(), "corrupt Melody payload exposed a version");
  }

  if (g_failures == 0) {
    std::printf("0.9.11 A2 material resolution: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "0.9.11 A2 material resolution: %d failure(s)\n",
               g_failures);
  return 1;
}
