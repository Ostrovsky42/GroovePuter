#include "sample_index.h"
#include <dirent.h>
#include <cstring>
#include <algorithm>

// FNV-1a Hash
uint32_t SampleIndex::calculateHash(const char* str) {
  uint32_t hash = 2166136261u;
  while (*str) {
    hash ^= (uint8_t)*str++;
    hash *= 16777619u;
  }
  return hash;
}

void SampleIndex::scanDirectory(const std::string& dirPath) {
  files_.clear();
  nameToId_.clear();
  
  DIR* dir = opendir(dirPath.c_str());
  if (!dir) return;
  
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG) { // regular file
      char* ext = strrchr(entry->d_name, '.');
      if (ext && (strcasecmp(ext, ".wav") == 0)) {
        SampleFileInfo info;
        info.filename = entry->d_name;
        info.fullPath = dirPath;
        if (info.fullPath.back() != '/') info.fullPath += "/";
        info.fullPath += entry->d_name;
        
        info.id.value = calculateHash(info.filename.c_str());
        
        files_.push_back(info);
        nameToId_[info.filename] = info.id;
      }
    }
  }
  closedir(dir);
  
  // Sort files by name for consistent UI
  std::sort(files_.begin(), files_.end(), [](const SampleFileInfo& a, const SampleFileInfo& b) {
    return a.filename < b.filename;
  });
}

SampleId SampleIndex::findIdByFilename(const std::string& filename) const {
  auto it = nameToId_.find(filename);
  if (it != nameToId_.end()) return it->second;
  return {0};
}
