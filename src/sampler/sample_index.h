#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "sample_store.h"
#include "sample_ref.h"

struct SampleFileInfo {
  SampleId id{0}; // legacy basename-derived ID retained for old Scene compatibility
  uint32_t fileSizeBytes{0};
  std::string fullPath;

  std::string_view filename() const {
    const std::size_t slash = fullPath.find_last_of('/');
    return slash == std::string::npos
        ? std::string_view(fullPath)
        : std::string_view(fullPath).substr(slash + 1);
  }
};

struct SampleRegistryBindResult {
  std::size_t discovered = 0;
  std::size_t registered = 0;
  std::size_t rejectedStable = 0;
  std::size_t rejectedLegacy = 0;
  std::size_t rejectedStore = 0;

  bool clean() const {
    return registered == discovered && rejectedStable == 0 &&
           rejectedLegacy == 0 && rejectedStore == 0;
  }
};

class SampleIndex {
public:
  // Recursively indexes loose WAV files below dirPath. The resulting catalog is
  // stable for the session and is also used by the sampler folder browser, so
  // browsing never has to replace the persistence/runtime identity registry.
  void scanDirectory(const std::string& dirPath);

  // Legacy/live filesystem helper retained for compatibility. New sampler UI
  // browsing uses indexedSubdirectories() instead so render/input paths never
  // touch the SD filesystem.
  std::vector<std::string> getSubdirectories(const std::string& dirPath);

  const std::vector<SampleFileInfo>& getFiles() const { return files_; }
  const std::string& rootDirectory() const { return rootDirectory_; }

  // Memory-only browser views over the already indexed catalog.
  std::vector<const SampleFileInfo*> filesInDirectory(
      const std::string& dirPath) const;
  std::vector<std::string> indexedSubdirectories(
      const std::string& dirPath) const;

  // Legacy basename lookup is fail-closed when multiple indexed files share
  // the same filename. Stable/path identity APIs must be used in that case.
  SampleId findIdByFilename(const std::string& filename) const;
  GroovePuterSampler::SampleRef findRefByFilename(const std::string& filename) const;
  const SampleFileInfo* findByRef(GroovePuterSampler::SampleRef ref) const;

  GroovePuterSampler::SampleRef resolveLegacyId(SampleId legacyId) const;
  const SampleFileInfo* resolveLegacyFile(SampleId legacyId) const;
  std::size_t legacyMatchCount(SampleId legacyId) const;

  // Control-side mapping between stable persisted identity and compact
  // audio/runtime identity. Unambiguous legacy IDs are preserved exactly;
  // ambiguous legacy collisions receive deterministic session-safe IDs that
  // never reuse any historical basename hash present in the index.
  SampleId runtimeIdForRef(GroovePuterSampler::SampleRef ref) const;
  SampleId runtimeIdForLegacyId(SampleId legacyId) const;
  GroovePuterSampler::SampleRef resolveRuntimeId(SampleId runtimeId) const;
  const SampleFileInfo* resolveRuntimeFile(SampleId runtimeId) const;

  SampleRegistryBindResult bindToStore(ISampleStore& store) const;

  static uint32_t calculateHash(const char* str);
  static GroovePuterSampler::SampleRef calculateStableRef(const std::string& path);

private:
  void scanDirectoryRecursive(const std::string& dirPath, int depth);
  SampleId runtimeIdForFile(const SampleFileInfo& file) const;
  bool runtimeCandidateReserved(uint32_t candidate) const;

  std::vector<SampleFileInfo> files_;
  std::string rootDirectory_;
};
