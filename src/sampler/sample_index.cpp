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

// Historical FNV-1a 32-bit basename hash.
uint32_t SampleIndex::calculateHash(const char* str) {
  uint32_t hash = 2166136261u;
  while (*str) {
    hash ^= static_cast<uint8_t>(*str++);
    hash *= 16777619u;
  }
  return hash;
}

GroovePuterSampler::SampleRef SampleIndex::calculateStableRef(
    const std::string& path) {
  return GroovePuterSampler::stableSampleRefForPath(path);
}

namespace {

void populateSampleIdentity(SampleFileInfo& info) {
  info.id.value = SampleIndex::calculateHash(info.filename.c_str());
  info.ref = SampleIndex::calculateStableRef(info.fullPath);
}

}  // namespace

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
        if (!info.fullPath.empty() && info.fullPath.back() != '/') {
          info.fullPath += "/";
        }
        info.fullPath += name;
        populateSampleIdentity(info);
        
        files_.push_back(info);
        nameToId_[info.filename] = info.id;
        
        printf("SampleIndex: Found: %s legacy=%u ref=%08x%08x\n",
               info.fullPath.c_str(),
               static_cast<unsigned>(info.id.value),
               static_cast<unsigned>(info.ref.value >> 32),
               static_cast<unsigned>(info.ref.value & 0xFFFFFFFFULL));
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
      if (ext && strcasecmp(ext, ".wav") == 0) {
        SampleFileInfo info;
        info.filename = entry->d_name;
        info.fullPath = dirPath;
        if (!info.fullPath.empty() && info.fullPath.back() != '/') {
          info.fullPath += "/";
        }
        info.fullPath += entry->d_name;
        populateSampleIdentity(info);
        
        files_.push_back(info);
        nameToId_[info.filename] = info.id;
        
        printf("SampleIndex: Found: %s legacy=%u ref=%08x%08x\n",
               info.fullPath.c_str(),
               static_cast<unsigned>(info.id.value),
               static_cast<unsigned>(info.ref.value >> 32),
               static_cast<unsigned>(info.ref.value & 0xFFFFFFFFULL));
      }
    }
  }
  closedir(dir);
#endif
  
  printf("SampleIndex::scanDirectory: Found %zu files\n", files_.size());
  
  // Sort files by name for consistent UI. Identity is path-derived and does
  // not depend on this presentation order.
  std::sort(files_.begin(), files_.end(), [](const SampleFileInfo& a, const SampleFileInfo& b) {
    return a.filename < b.filename;
  });
}

SampleId SampleIndex::findIdByFilename(const std::string& filename) const {
  auto it = nameToId_.find(filename);
  if (it != nameToId_.end()) return it->second;
  return {0};
}

GroovePuterSampler::SampleRef SampleIndex::findRefByFilename(
    const std::string& filename) const {
  for (const auto& file : files_) {
    if (file.filename == filename) return file.ref;
  }
  return {};
}

const SampleFileInfo* SampleIndex::findByRef(
    GroovePuterSampler::SampleRef ref) const {
  if (!ref.valid()) return nullptr;
  for (const auto& file : files_) {
    if (file.ref == ref) return &file;
  }
  return nullptr;
}

GroovePuterSampler::SampleRef SampleIndex::resolveLegacyId(
    SampleId legacyId) const {
  if (legacyId.value == 0) return {};

  GroovePuterSampler::SampleRef resolved{};
  bool found = false;
  for (const auto& file : files_) {
    if (file.id != legacyId) continue;
    if (found && file.ref != resolved) {
      printf("SampleIndex: Legacy ID %u is ambiguous\n",
             static_cast<unsigned>(legacyId.value));
      return {};
    }
    resolved = file.ref;
    found = true;
  }
  return found ? resolved : GroovePuterSampler::SampleRef{};
}

std::vector<std::string> SampleIndex::getSubdirectories(const std::string& dirPath) {
  std::vector<std::string> dirs;
  printf("SampleIndex::getSubdirectories: Scanning '%s'...\n", dirPath.c_str());

#if USE_ARDUINO_SD
  File dir = SD.open(dirPath.c_str());
  if (!dir || !dir.isDirectory()) return dirs;

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (entry.isDirectory()) {
      const char* name = entry.name();
      if (name[0] == '/') name++;
      const char* lastSlash = strrchr(name, '/');
      if (lastSlash) name = lastSlash + 1;
      
      // Skip system dirs or hidden
      if (name[0] != '.') {
          dirs.push_back(name);
      }
    }
    entry.close();
  }
  dir.close();
#else
  DIR* dir = opendir(dirPath.c_str());
  if (!dir) return dirs;
  
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_DIR) {
      if (entry->d_name[0] != '.') {
        dirs.push_back(entry->d_name);
      }
    }
  }
  closedir(dir);
#endif

  std::sort(dirs.begin(), dirs.end());
  return dirs;
}
