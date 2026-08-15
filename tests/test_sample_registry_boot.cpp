#include "../src/sampler/sample_index.h"
#include "../src/sampler/ram_sample_store.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace fs = std::filesystem;
using GroovePuterSampler::SampleRef;

// RamSampleStore links the WAV probe/loader even though these registry tests
// never preload PCM. Keep the test host-only and deterministic.
bool inspectWavFileBounded(const char*, WavInfo&, std::size_t) {
  return false;
}
bool loadWavFileBounded(const char*, WavInfo&, int16_t**, std::size_t) {
  return false;
}

namespace {

void touch(const fs::path& path) {
  std::ofstream out(path, std::ios::binary);
  out.put('\0');
}

class CaptureStore final : public ISampleStore {
public:
  bool registerFile(SampleId id, const std::string& path) override {
    const auto it = paths.find(id.value);
    if (it == paths.end()) {
      paths.emplace(id.value, path);
      return true;
    }
    return it->second == path;
  }

  bool preload(SampleId) override { return false; }
  SampleHandle acquireHandle(SampleId) override { return SampleHandle::invalid(); }
  void releaseHandle(SampleHandle) override {}
  SampleView viewHandle(SampleHandle) const override { return {nullptr, 0, 0}; }
  void acquire(SampleId) override {}
  void release(SampleId) override {}
  SampleView view(SampleId) const override { return {nullptr, 0, 0}; }
  void evictLRU() override {}
  std::size_t freePoolBytes() const override { return 0; }
  void setPoolSize(std::size_t) override {}

  std::map<uint32_t, std::string> paths;
};

void testUniqueRegistryBinding() {
  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sampler_registry_unique";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / "kick.wav");
  touch(root / "snare.wav");

  SampleIndex index;
  index.scanDirectory(root.string());
  assert(index.getFiles().size() == 2);

  CaptureStore store;
  const SampleRegistryBindResult result = index.bindToStore(store);
  assert(result.discovered == 2);
  assert(result.registered == 2);
  assert(result.rejectedStable == 0);
  assert(result.rejectedLegacy == 0);
  assert(result.rejectedStore == 0);
  assert(result.clean());
  assert(store.paths.size() == 2);

  const SampleId kickLegacy{SampleIndex::calculateHash("kick.wav")};
  const SampleFileInfo* kick = index.resolveLegacyFile(kickLegacy);
  assert(kick != nullptr);
  assert(kick->filename == "kick.wav");

  const SampleRef kickRef = index.findRefByFilename("kick.wav");
  assert(kickRef.valid());
  assert(index.findByRef(kickRef) == kick);
  assert(index.runtimeIdForRef(kickRef) == kickLegacy);
  assert(index.resolveRuntimeId(kickLegacy) == kickRef);

  fs::remove_all(root);
}

void testLegacyCollisionGetsStableRuntimeOwnership() {
  // Real FNV-1a32 collision already frozen by 0.9.3-B.
  constexpr const char* kNameA = "5oetw2k1.wav";
  constexpr const char* kNameB = "qp363n87.wav";
  const uint32_t hashA = SampleIndex::calculateHash(kNameA);
  const uint32_t hashB = SampleIndex::calculateHash(kNameB);
  assert(hashA == 3960902837u);
  assert(hashA == hashB);

  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sampler_registry_collision";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / kNameA);
  touch(root / kNameB);

  SampleIndex index;
  index.scanDirectory(root.string());
  assert(index.getFiles().size() == 2);

  const SampleRef refA = index.findRefByFilename(kNameA);
  const SampleRef refB = index.findRefByFilename(kNameB);
  assert(refA.valid());
  assert(refB.valid());
  assert(refA != refB);

  const SampleId runtimeA = index.runtimeIdForRef(refA);
  const SampleId runtimeB = index.runtimeIdForRef(refB);
  assert(runtimeA.value != 0);
  assert(runtimeB.value != 0);
  assert(runtimeA != runtimeB);
  // Never bind either stable file to the ambiguous historical hash.
  assert(runtimeA.value != hashA);
  assert(runtimeB.value != hashA);
  assert(index.resolveRuntimeId(runtimeA) == refA);
  assert(index.resolveRuntimeId(runtimeB) == refB);

  CaptureStore store;
  const SampleRegistryBindResult result = index.bindToStore(store);
  assert(result.discovered == 2);
  assert(result.registered == 2);
  assert(result.rejectedStable == 0);
  // Both files remain impossible to resolve from the old ambiguous ID.
  assert(result.rejectedLegacy == 2);
  assert(result.rejectedStore == 0);
  assert(!result.clean());
  assert(store.paths.size() == 2);
  assert(store.paths.count(hashA) == 0);
  assert(index.resolveLegacyFile(SampleId{hashA}) == nullptr);
  assert(index.runtimeIdForLegacyId(SampleId{hashA}).value == 0);

  fs::remove_all(root);
}

void testRamStoreRefusesConflictingRuntimeRebind() {
  RamSampleStore store;
  const SampleId id{42};
  assert(store.registerFile(id, "/samples/kick.wav"));
  assert(store.registerFile(id, "/samples/kick.wav"));
  assert(!store.registerFile(id, "/samples/other.wav"));
}

}  // namespace

int main() {
  testUniqueRegistryBinding();
  testLegacyCollisionGetsStableRuntimeOwnership();
  testRamStoreRefusesConflictingRuntimeRebind();
  return 0;
}
