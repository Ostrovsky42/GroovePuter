#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include "sample_store.h"
#include "sample_ref.h"

struct SampleFileInfo {
  // Legacy basename-derived runtime ID. Kept for current Scene compatibility;
  // stable control-side identity is derived from fullPath via SampleRef.
  SampleId id{0};
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
  // Scans a directory for .wav files and populates the index.
  // Note: Local storage implementation.
  void scanDirectory(const std::string& dirPath);
  std::vector<std::string> getSubdirectories(const std::string& dirPath);

  const std::vector<SampleFileInfo>& getFiles() const { return files_; }

  // Legacy lookup by exact filename (e.g. "kick.wav"). This returns the
  // historical basename-derived SampleId for compatibility/migration only.
  SampleId findIdByFilename(const std::string& filename) const;

  // Stable identity is derived from the already-stored fullPath on demand.
  // Keeping it out of SampleFileInfo avoids per-file heap growth on ADV.
  GroovePuterSampler::SampleRef findRefByFilename(const std::string& filename) const;
  const SampleFileInfo* findByRef(GroovePuterSampler::SampleRef ref) const;

  // Compatibility bridge for old scenes that persisted basename hashes.
  // Returns a stable ref/file only when the legacy ID resolves to exactly one
  // indexed path. Ambiguous or missing legacy IDs fail closed.
  GroovePuterSampler::SampleRef resolveLegacyId(SampleId legacyId) const;
  const SampleFileInfo* resolveLegacyFile(SampleId legacyId) const;

  // Build the control-side runtime registry only from identities that are
  // unambiguous both as stable SampleRef and as the current legacy runtime ID.
  // 0.9.3-D will migrate persistence ownership to SampleRef; until then this
  // validation prevents basename/hash collisions from silently rebinding PCM.
  SampleRegistryBindResult bindToStore(ISampleStore& store) const;

  // Historical FNV-1a basename hash. Kept for decode/migration compatibility.
  static uint32_t calculateHash(const char* str);

  // Stable path-based identity used by the recovery line from 0.9.3-B onward.
  static GroovePuterSampler::SampleRef calculateStableRef(const std::string& path);

private:
  std::vector<SampleFileInfo> files_;
  std::map<std::string, SampleId> nameToId_;
};
