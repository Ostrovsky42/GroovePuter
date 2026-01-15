#include "sample_index.h"
#include <cstring>
#include <algorithm>
#include <cstdio>

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <SD.h>
#define USE_ARDUINO_SD 1
#else
#include <dirent.h>
#define USE_ARDUINO_SD 0
#endif

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
  
  printf("SampleIndex::scanDirectory: Scanning '%s'...\n", dirPath.c_str());

#if USE_ARDUINO_SD
  // ESP32 Arduino SD library path
  File dir = SD.open(dirPath.c_str());
  if (!dir) {
    printf("SampleIndex::scanDirectory: Failed to open directory\n");
    return;
  }
  if (!dir.isDirectory()) {
    printf("SampleIndex::scanDirectory: Path is not a directory\n");
    dir.close();
    return;
  }
  
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    
    if (!entry.isDirectory()) {
      const char* name = entry.name();
      // Skip leading '/' if present
      if (name[0] == '/') name++;
      // Find last component of path
      const char* lastSlash = strrchr(name, '/');
      if (lastSlash) name = lastSlash + 1;
      
      const char* ext = strrchr(name, '.');
      if (ext && strcasecmp(ext, ".wav") == 0) {
        SampleFileInfo info;
        info.filename = name;
        info.fullPath = dirPath;
        if (info.fullPath.back() != '/') info.fullPath += "/";
        info.fullPath += name;
        
        info.id.value = calculateHash(info.filename.c_str());
        
        files_.push_back(info);
        nameToId_[info.filename] = info.id;
        
        printf("SampleIndex: Found: %s\n", info.fullPath.c_str());
      }
    }
    entry.close();
  }
  dir.close();
  
#else
  // POSIX path (for SDL/Desktop)
  DIR* dir = opendir(dirPath.c_str());
  if (!dir) {
    printf("SampleIndex::scanDirectory: opendir failed\n");
    return;
  }
  
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
        
        printf("SampleIndex: Found: %s\n", info.fullPath.c_str());
      }
    }
  }
  closedir(dir);
#endif
  
  printf("SampleIndex::scanDirectory: Found %zu files\n", files_.size());
  
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
