#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include "sample_store.h"
#include "sample_ref.h"

struct SampleFileInfo {
  SampleId id{0}; // legacy basename-derived ID retained for old Scene compatibility
  std::string filename;
  std::string fullPath;
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
  void scanDirectory(const std::string& dirPath);
  std::vector<std::string> getSubdirectories(const std::string& dirPath);

  const std::vector<SampleFileInfo>& getFiles() const { return files_; }

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
  SampleId runtimeIdForFile(const SampleFileInfo& file) const;
  bool runtimeCandidateReserved(uint32_t candidate) const;

  std::vector<SampleFileInfo> files_;
  std::map<std::string, SampleId> nameToId_;
};
