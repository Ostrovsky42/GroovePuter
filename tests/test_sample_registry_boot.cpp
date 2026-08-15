#include "../src/sampler/sample_index.h"
#include "../src/sampler/ram_sample_store.h"
#include "../src/sampler/sample_loader.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace fs = std::filesystem;
using GroovePuterSampler::SampleRef;

// RamSampleStore links the split WAV inspect/decode contract even though these
// registry tests never preload PCM. Keep the test host-only and deterministic.
const char* wavLoadErrorName(WavLoadError) {
  return "registry-test-stub";
}
bool inspectWavFileBounded(const char*, WavInspectResult&, std::size_t,
                           WavLoadError*) {
  return false;
}
bool decodeWavFileBounded(const char*, const WavInspectResult&, int16_t**,
                          std::size_t, WavLoadError*) {
  return false;
}
// Compatibility entry points remain part of the 0.9.5-A public surface.
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

class IndexBackedCaptureStore final : public ISampleStore {
public:
  bool bindSampleIndex(const SampleIndex* index) override {
    boundIndex = index;
    return index != nullptr;
  }
  bool registerFile(SampleId, const std::string&) override {
    ++copiedPathCount;
    return true;
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

  const SampleIndex* boundIndex = nullptr;
  int copiedPathCount = 0;
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
  assert(index.findIdByFilename("kick.wav") == kickLegacy);
  const SampleFileInfo* kick = index.resolveLegacyFile(kickLegacy);
  assert(kick != nullptr);
  assert(kick->filename() == "kick.wav");

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

void testRecursiveLooseSampleFolders() {
  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sampler_folder_browser";
  fs::remove_all(root);
  fs::create_directories(root / "909" / "hats");
  fs::create_directories(root / "SP12");
  fs::create_directories(root / ".hidden");

  touch(root / "root.wav");
  touch(root / "909" / "kick.wav");
  touch(root / "909" / "snare.wav");
  touch(root / "909" / "hats" / "closed.wav");
  touch(root / "SP12" / "kick.wav");
  touch(root / ".hidden" / "ignored.wav");
  touch(root / "909" / "notes.txt");

  SampleIndex index;
  index.scanDirectory(root.string());

  assert(index.rootDirectory() == root.string());
  assert(index.getFiles().size() == 5);

  const auto rootFiles = index.filesInDirectory(root.string());
  assert(rootFiles.size() == 1);
  assert(rootFiles[0]->filename() == "root.wav");
  assert(rootFiles[0]->fileSizeBytes == 1);

  const auto rootDirs = index.indexedSubdirectories(root.string());
  assert(rootDirs.size() == 2);
  assert(rootDirs[0] == (root / "909").string());
  assert(rootDirs[1] == (root / "SP12").string());

  const auto files909 = index.filesInDirectory((root / "909").string());
  assert(files909.size() == 2);
  assert(files909[0]->filename() == "kick.wav");
  assert(files909[1]->filename() == "snare.wav");

  const auto dirs909 = index.indexedSubdirectories((root / "909").string());
  assert(dirs909.size() == 1);
  assert(dirs909[0] == (root / "909" / "hats").string());

  const auto hats = index.filesInDirectory((root / "909" / "hats").string());
  assert(hats.size() == 1);
  assert(hats[0]->filename() == "closed.wav");

  // Same basename in different folders is intentionally ambiguous to every
  // legacy basename-only lookup. Stable path identity and runtime IDs remain
  // unique and are the only safe selection path.
  assert(index.findIdByFilename("kick.wav").value == 0);
  assert(!index.findRefByFilename("kick.wav").valid());
  const SampleRef kick909Ref =
      SampleIndex::calculateStableRef((root / "909" / "kick.wav").string());
  const SampleRef kickSp12Ref =
      SampleIndex::calculateStableRef((root / "SP12" / "kick.wav").string());
  assert(kick909Ref.valid());
  assert(kickSp12Ref.valid());
  assert(kick909Ref != kickSp12Ref);
  assert(index.findByRef(kick909Ref) != nullptr);
  assert(index.findByRef(kickSp12Ref) != nullptr);

  const SampleId kick909Runtime = index.runtimeIdForRef(kick909Ref);
  const SampleId kickSp12Runtime = index.runtimeIdForRef(kickSp12Ref);
  assert(kick909Runtime.value != 0);
  assert(kickSp12Runtime.value != 0);
  assert(kick909Runtime != kickSp12Runtime);

  CaptureStore store;
  const SampleRegistryBindResult result = index.bindToStore(store);
  assert(result.discovered == 5);
  assert(result.registered == 5);
  assert(result.rejectedStable == 0);
  assert(result.rejectedLegacy == 2);  // the two kick.wav legacy IDs
  assert(result.rejectedStore == 0);
  assert(store.paths.size() == 5);

  fs::remove_all(root);
}

void testCatalogCanExceedResidentSlotCount() {
  const fs::path root =
      fs::temp_directory_path() / "grooveputer_sampler_registry_over_64";
  fs::remove_all(root);
  fs::create_directories(root / "bankA");
  fs::create_directories(root / "bankB");

  constexpr int kFileCount = 80;
  for (int i = 0; i < kFileCount; ++i) {
    const fs::path bank = i < 40 ? (root / "bankA") : (root / "bankB");
    touch(bank / ("sample_" + std::to_string(i) + ".wav"));
  }

  SampleIndex index;
  index.scanDirectory(root.string());
  assert(index.getFiles().size() == kFileCount);
  assert(index.indexedSubdirectories(root.string()).size() == 2);

  // kMaxSampleSlots limits simultaneously resident PCM descriptors; it must
  // not cap the SD catalog/path registry. Bind through the real Store here.
  static_assert(kFileCount > kMaxSampleSlots,
                "test must exceed the resident slot descriptor count");
  RamSampleStore store;
  const SampleRegistryBindResult result = index.bindToStore(store);
  assert(result.discovered == kFileCount);
  assert(result.registered == kFileCount);
  assert(result.rejectedStable == 0);
  assert(result.rejectedLegacy == 0);
  assert(result.rejectedStore == 0);
  assert(result.clean());

  const SampleRef lastRef = SampleIndex::calculateStableRef(
      (root / "bankB" / "sample_79.wav").string());
  assert(lastRef.valid());
  assert(index.runtimeIdForRef(lastRef).value != 0);

  IndexBackedCaptureStore borrowedStore;
  const SampleRegistryBindResult borrowed = index.bindToStore(borrowedStore);
  assert(borrowed.clean());
  assert(borrowedStore.boundIndex == &index);
  assert(borrowedStore.copiedPathCount == 0);

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
  testRecursiveLooseSampleFolders();
  testCatalogCanExceedResidentSlotCount();
  testRamStoreRefusesConflictingRuntimeRebind();
  return 0;
}
