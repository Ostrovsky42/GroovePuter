#include "../src/sampler/sample_index.h"
#include "../src/sampler/sample_scene_persistence.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace fs = std::filesystem;
using GroovePuterSampler::SampleRef;
using GroovePuterSampler::SceneSampleFilterDirection;

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

struct StringWriter {
  std::string data;
  std::size_t write(const uint8_t* bytes, std::size_t len) {
    data.append(reinterpret_cast<const char*>(bytes), len);
    return len;
  }
};

struct StringReader {
  explicit StringReader(std::string source) : data(std::move(source)) {}
  int read() {
    if (position >= data.size()) return -1;
    return static_cast<unsigned char>(data[position++]);
  }
  std::string data;
  std::size_t position = 0;
};

std::string onePadScene(uint32_t id) {
  return std::string("{\"note\":\"samplerPads\",\"state\":{\"samplerPads\":[") +
      "{\"id\":" + std::to_string(id) +
      ",\"vol\":0.75,\"pch\":1.25,\"str\":17,\"end\":4096,"
      "\"chk\":3,\"rev\":true,\"lop\":true}]}}";
}

void testHexRoundTripAboveJsonExactIntegerRange() {
  const SampleRef original{0xFEDCBA9876543210ULL};
  assert(original.value > (1ULL << 53));
  char encoded[17] = {};
  assert(GroovePuterSampler::encodeSampleRefHex(original, encoded));
  assert(std::string(encoded) == "fedcba9876543210");
  SampleRef decoded{};
  assert(GroovePuterSampler::decodeSampleRefHex(encoded, 16, decoded));
  assert(decoded == original);
  assert(!GroovePuterSampler::decodeSampleRefHex("fedcba987654321z", 16, decoded));
  assert(!GroovePuterSampler::decodeSampleRefHex("0000000000000000", 16, decoded));
}

void testUniqueSaveLoadPreservesPadParameters() {
  const fs::path root = fs::temp_directory_path() / "grooveputer_sampler_d_unique";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / "kick.wav");

  SampleIndex index;
  index.scanDirectory(root.string());
  CaptureStore store;
  assert(index.bindToStore(store).registered == 1);

  const SampleRef ref = index.findRefByFilename("kick.wav");
  const SampleId runtime = index.runtimeIdForRef(ref);
  assert(ref.valid());
  assert(runtime.value != 0);

  const std::string runtimeScene = onePadScene(runtime.value);
  std::string persisted;
  assert(GroovePuterSampler::transformSamplerSceneString(
      runtimeScene, SceneSampleFilterDirection::Save, &index, persisted));
  assert(persisted.find("\"ref\":\"") != std::string::npos);
  assert(persisted.find("\"vol\":0.75") != std::string::npos);
  assert(persisted.find("\"pch\":1.25") != std::string::npos);
  assert(persisted.find("\"str\":17") != std::string::npos);
  assert(persisted.find("\"end\":4096") != std::string::npos);
  assert(persisted.find("\"chk\":3") != std::string::npos);
  assert(persisted.find("\"rev\":true") != std::string::npos);
  assert(persisted.find("\"lop\":true") != std::string::npos);

  std::string loaded;
  assert(GroovePuterSampler::transformSamplerSceneString(
      persisted, SceneSampleFilterDirection::Load, &index, loaded));
  assert(loaded.find("\"id\":" + std::to_string(runtime.value)) !=
         std::string::npos);
  assert(loaded.find("\"vol\":0.75") != std::string::npos);
  assert(loaded.find("\"pch\":1.25") != std::string::npos);
  assert(loaded.find("\"str\":17") != std::string::npos);
  assert(loaded.find("\"end\":4096") != std::string::npos);
  assert(loaded.find("\"chk\":3") != std::string::npos);
  assert(loaded.find("\"rev\":true") != std::string::npos);
  assert(loaded.find("\"lop\":true") != std::string::npos);

  fs::remove_all(root);
}

void testLegacyIdOnlyCompatibilityAndMissingSampleBehavior() {
  const fs::path root = fs::temp_directory_path() / "grooveputer_sampler_d_legacy";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / "snare.wav");

  SampleIndex index;
  index.scanDirectory(root.string());
  CaptureStore store;
  index.bindToStore(store);

  const uint32_t legacy = SampleIndex::calculateHash("snare.wav");
  std::string loaded;
  assert(GroovePuterSampler::transformSamplerSceneString(
      onePadScene(legacy), SceneSampleFilterDirection::Load, &index, loaded));
  assert(loaded.find("\"id\":" + std::to_string(legacy)) !=
         std::string::npos);

  constexpr uint32_t kMissingLegacyId = 123456789u;
  assert(index.legacyMatchCount(SampleId{kMissingLegacyId}) == 0);
  assert(GroovePuterSampler::transformSamplerSceneString(
      onePadScene(kMissingLegacyId), SceneSampleFilterDirection::Load,
      &index, loaded));
  assert(loaded.find("\"id\":123456789") != std::string::npos);

  fs::remove_all(root);
}

void testCollisionPairStableRefsAreBothUsableButLegacyIsNot() {
  constexpr const char* kNameA = "5oetw2k1.wav";
  constexpr const char* kNameB = "qp363n87.wav";
  constexpr uint32_t kCollisionId = 3960902837u;

  const fs::path root = fs::temp_directory_path() / "grooveputer_sampler_d_collision";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / kNameA);
  touch(root / kNameB);

  SampleIndex index;
  index.scanDirectory(root.string());
  CaptureStore store;
  const auto bind = index.bindToStore(store);
  assert(bind.registered == 2);
  assert(bind.rejectedLegacy == 2);

  const SampleRef refA = index.findRefByFilename(kNameA);
  const SampleRef refB = index.findRefByFilename(kNameB);
  const SampleId runtimeA = index.runtimeIdForRef(refA);
  const SampleId runtimeB = index.runtimeIdForRef(refB);
  assert(refA.valid() && refB.valid() && refA != refB);
  assert(runtimeA.value != 0 && runtimeB.value != 0 && runtimeA != runtimeB);
  assert(runtimeA.value != kCollisionId && runtimeB.value != kCollisionId);
  assert(store.paths.count(kCollisionId) == 0);

  std::string savedA;
  assert(GroovePuterSampler::transformSamplerSceneString(
      onePadScene(runtimeA.value), SceneSampleFilterDirection::Save,
      &index, savedA));
  assert(savedA.find("\"id\":3960902837") != std::string::npos);

  std::string loadedA;
  assert(GroovePuterSampler::transformSamplerSceneString(
      savedA, SceneSampleFilterDirection::Load, &index, loadedA));
  assert(loadedA.find("\"id\":" + std::to_string(runtimeA.value)) !=
         std::string::npos);

  std::string legacyLoaded;
  assert(!GroovePuterSampler::transformSamplerSceneString(
      onePadScene(kCollisionId), SceneSampleFilterDirection::Load,
      &index, legacyLoaded));

  fs::remove_all(root);
}

void testMalformedStableRefStillFailsClosed() {
  const fs::path root = fs::temp_directory_path() / "grooveputer_sampler_d_malformed";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / "hat.wav");

  SampleIndex index;
  index.scanDirectory(root.string());
  CaptureStore store;
  index.bindToStore(store);
  const SampleRef ref = index.findRefByFilename("hat.wav");
  const SampleId runtime = index.runtimeIdForRef(ref);

  std::string persisted;
  assert(GroovePuterSampler::transformSamplerSceneString(
      onePadScene(runtime.value), SceneSampleFilterDirection::Save,
      &index, persisted));

  const std::size_t refStart = persisted.find("\"ref\":\"");
  assert(refStart != std::string::npos);
  std::string malformed = persisted;
  malformed[refStart + 7] = 'z';
  std::string out;
  assert(!GroovePuterSampler::transformSamplerSceneString(
      malformed, SceneSampleFilterDirection::Load, &index, out));

  fs::remove_all(root);
}

void testMissingStableRefDegradesSilentAndSurvivesResave() {
  const fs::path root = fs::temp_directory_path() / "grooveputer_sampler_d_missing_ref";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path wav = root / "clap.wav";
  touch(wav);

  SampleIndex presentIndex;
  presentIndex.scanDirectory(root.string());
  CaptureStore presentStore;
  assert(presentIndex.bindToStore(presentStore).registered == 1);
  const SampleRef originalRef = presentIndex.findRefByFilename("clap.wav");
  const SampleId runtime = presentIndex.runtimeIdForRef(originalRef);
  assert(originalRef.valid() && runtime.value != 0);

  std::string persisted;
  assert(GroovePuterSampler::transformSamplerSceneString(
      onePadScene(runtime.value), SceneSampleFilterDirection::Save,
      &presentIndex, persisted));
  const std::size_t refKey = persisted.find("\"ref\":\"");
  assert(refKey != std::string::npos);
  const std::string persistedRef = persisted.substr(refKey + 7, 16);

  fs::remove(wav);
  SampleIndex missingIndex;
  missingIndex.scanDirectory(root.string());
  std::string loadedMissing;
  assert(GroovePuterSampler::transformSamplerSceneString(
      persisted, SceneSampleFilterDirection::Load,
      &missingIndex, loadedMissing));
  assert(loadedMissing.find("\"id\":0") != std::string::npos);
  assert(loadedMissing.find("\"ref\":\"" + persistedRef + "\"") !=
         std::string::npos);

  // SceneManager only retains runtime id=0. The control-side unresolved-ref
  // sidecar must keep the physical identity alive across a later Save.
  std::string resavedMissing;
  assert(GroovePuterSampler::transformSamplerSceneString(
      onePadScene(0), SceneSampleFilterDirection::Save,
      &missingIndex, resavedMissing));
  assert(resavedMissing.find("\"id\":0") != std::string::npos);
  assert(resavedMissing.find("\"ref\":\"" + persistedRef + "\"") !=
         std::string::npos);

  touch(wav);
  SampleIndex restoredIndex;
  restoredIndex.scanDirectory(root.string());
  CaptureStore restoredStore;
  assert(restoredIndex.bindToStore(restoredStore).registered == 1);
  const SampleId restoredRuntime = restoredIndex.runtimeIdForRef(originalRef);
  assert(restoredRuntime.value != 0);

  std::string restored;
  assert(GroovePuterSampler::transformSamplerSceneString(
      resavedMissing, SceneSampleFilterDirection::Load,
      &restoredIndex, restored));
  assert(restored.find("\"id\":" + std::to_string(restoredRuntime.value)) !=
         std::string::npos);

  fs::remove_all(root);
}

void testBoundedStreamingReaderWriter() {
  const fs::path root = fs::temp_directory_path() / "grooveputer_sampler_d_stream";
  fs::remove_all(root);
  fs::create_directories(root);
  touch(root / "clap.wav");

  SampleIndex index;
  index.scanDirectory(root.string());
  CaptureStore store;
  index.bindToStore(store);
  const SampleRef ref = index.findRefByFilename("clap.wav");
  const SampleId runtime = index.runtimeIdForRef(ref);
  const std::string source = onePadScene(runtime.value);

  StringWriter sink;
  GroovePuterSampler::SamplerSceneWriteFilter<StringWriter> writer(sink, &index);
  assert(writer.write(reinterpret_cast<const uint8_t*>(source.data()),
                      source.size()) == source.size());
  assert(writer.finish());
  assert(sink.data.find("\"ref\":\"") != std::string::npos);

  StringReader sourceReader(sink.data);
  GroovePuterSampler::SamplerSceneReadFilter<StringReader> reader(sourceReader,
                                                                  &index);
  std::string decoded;
  for (int c = reader.read(); c >= 0; c = reader.read()) {
    decoded.push_back(static_cast<char>(c));
  }
  assert(!reader.failed());
  assert(decoded.find("\"id\":" + std::to_string(runtime.value)) !=
         std::string::npos);

  // Pad objects larger than the bounded control-side scratch are rejected.
  std::string oversized = "{\"state\":{\"samplerPads\":[{\"id\":0,\"x\":\"";
  oversized.append(GroovePuterSampler::SamplerSceneFilter::kMaxPadObjectBytes,
                   'a');
  oversized += "\"}]}}";
  std::string rejected;
  assert(!GroovePuterSampler::transformSamplerSceneString(
      oversized, SceneSampleFilterDirection::Load, &index, rejected));

  fs::remove_all(root);
}

}  // namespace

int main() {
  testHexRoundTripAboveJsonExactIntegerRange();
  testUniqueSaveLoadPreservesPadParameters();
  testLegacyIdOnlyCompatibilityAndMissingSampleBehavior();
  testCollisionPairStableRefsAreBothUsableButLegacyIsNot();
  testMalformedStableRefStillFailsClosed();
  testMissingStableRefDegradesSilentAndSurvivesResave();
  testBoundedStreamingReaderWriter();
  return 0;
}
