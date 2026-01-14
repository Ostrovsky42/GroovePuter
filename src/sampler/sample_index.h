#pragma once
#include <string>
#include <vector>
#include <map>
#include "sample_store.h"

struct SampleFileInfo {
  SampleId id;
  std::string filename;
  std::string fullPath;
};

class SampleIndex {
public:
  // Scans a directory for .wav files and populates the index.
  // Note: Local storage implementation.
  void scanDirectory(const std::string& dirPath);
  
  const std::vector<SampleFileInfo>& getFiles() const { return files_; }
  
  // Find ID by exact filename (e.g. "kick.wav")
  SampleId findIdByFilename(const std::string& filename) const;
  
  // Helper to get hash (reused from store logic conceptually)
  static uint32_t calculateHash(const char* str);

private:
  std::vector<SampleFileInfo> files_;
  std::map<std::string, SampleId> nameToId_;
};
