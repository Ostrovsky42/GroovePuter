#pragma once
#include <string>
#include <vector>
#include <map>
#include "sample_store.h"
#include "sample_ref.h"

struct SampleFileInfo {
  // Legacy basename-derived runtime ID. Kept unchanged in 0.9.3-B so the
  // identity PR does not alter the audio-thread/store ABI before lifecycle
  // migration in 0.9.3-C.
  SampleId id{0};

  // Stable control-side identity derived from the canonical logical path.
  GroovePuterSampler::SampleRef ref{};

  std::string filename;
  std::string fullPath;
};

class SampleIndex {
public:
  // Scans a directory for .wav files and populates the index.
  // Note: Local storage implementation.
  void scanDirectory(const std::string& dirPath);
  std::vector<std::string> getSubdirectories(const std::string& dirPath);
  
  const std::vector<SampleFileInfo>& getFiles() const { return files_; }
  
  // Legacy lookup by exact filename (e.g. "kick.wav"). This intentionally
  // returns the historical basename-derived SampleId until 0.9.3-C switches
  // registry ownership to stable SampleRef.
  SampleId findIdByFilename(const std::string& filename) const;

  // New stable identity lookup for control-side code.
  GroovePuterSampler::SampleRef findRefByFilename(const std::string& filename) const;
  const SampleFileInfo* findByRef(GroovePuterSampler::SampleRef ref) const;

  // Compatibility bridge for old scenes that persisted basename hashes.
  // Returns a stable ref only when the legacy ID resolves to exactly one file
  // in the current index. Ambiguous or missing legacy IDs fail closed.
  GroovePuterSampler::SampleRef resolveLegacyId(SampleId legacyId) const;
  
  // Historical FNV-1a basename hash. Kept for decode/migration compatibility.
  static uint32_t calculateHash(const char* str);

  // Stable path-based identity used by the recovery line from 0.9.3-B onward.
  static GroovePuterSampler::SampleRef calculateStableRef(const std::string& path);

private:
  std::vector<SampleFileInfo> files_;
  std::map<std::string, SampleId> nameToId_;
};
