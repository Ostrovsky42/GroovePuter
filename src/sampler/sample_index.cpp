#include "sample_index.h"
#include "sample_scene_persistence.h"

#include <cstring>
#include <algorithm>
#include <cstdio>
#include <vector>

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <SD.h>
#define USE_ARDUINO_SD 1
#else
#include <dirent.h>
#define USE_ARDUINO_SD 0
#endif

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

void populateLegacySampleId(SampleFileInfo& info) {
  info.id.value = SampleIndex::calculateHash(info.filename.c_str());
}

void logDiscoveredSample(const SampleFileInfo& info) {
  const auto ref = SampleIndex::calculateStableRef(info.fullPath);
  printf("SampleIndex: Found: %s legacy=%u ref=%08x%08x\n",
         info.fullPath.c_str(),
         static_cast<unsigned>(info.id.value),
         static_cast<unsigned>(ref.value >> 32),
         static_cast<unsigned>(ref.value & 0xFFFFFFFFULL));
}

uint32_t foldStableRef(GroovePuterSampler::SampleRef ref) {
  uint32_t value = static_cast<uint32_t>(ref.value >> 32) ^
                   static_cast<uint32_t>(ref.value & 0xFFFFFFFFULL);
  return value == 0 ? 1u : value;
}

}  // namespace

void SampleIndex::scanDirectory(const std::string& dirPath) {
  files_.clear();
  nameToId_.clear();

  printf("SampleIndex::scanDirectory: Scanning '%s'...\n", dirPath.c_str());

#if USE_ARDUINO_SD
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
      if (name[0] == '/') name++;
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
        populateLegacySampleId(info);

        files_.push_back(info);
        nameToId_[info.filename] = info.id;
        logDiscoveredSample(info);
      }
    }
    entry.close();
  }
  dir.close();

#else
  DIR* dir = opendir(dirPath.c_str());
  if (!dir) {
    printf("SampleIndex::scanDirectory: opendir failed\n");
    return;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG) {
      char* ext = strrchr(entry->d_name, '.');
      if (ext && strcasecmp(ext, ".wav") == 0) {
        SampleFileInfo info;
        info.filename = entry->d_name;
        info.fullPath = dirPath;
        if (!info.fullPath.empty() && info.fullPath.back() != '/') {
          info.fullPath += "/";
        }
        info.fullPath += entry->d_name;
        populateLegacySampleId(info);

        files_.push_back(info);
        nameToId_[info.filename] = info.id;
        logDiscoveredSample(info);
      }
    }
  }
  closedir(dir);
#endif

  printf("SampleIndex::scanDirectory: Found %zu files\n", files_.size());

  std::sort(files_.begin(), files_.end(),
            [](const SampleFileInfo& a, const SampleFileInfo& b) {
              if (a.filename != b.filename) return a.filename < b.filename;
              return a.fullPath < b.fullPath;
            });
}

SampleId SampleIndex::findIdByFilename(const std::string& filename) const {
  auto it = nameToId_.find(filename);
  if (it != nameToId_.end()) return it->second;
  return {0};
}

GroovePuterSampler::SampleRef SampleIndex::findRefByFilename(
    const std::string& filename) const {
  const SampleFileInfo* match = nullptr;
  for (const auto& file : files_) {
    if (file.filename != filename) continue;
    if (match != nullptr && match->fullPath != file.fullPath) return {};
    match = &file;
  }
  return match ? calculateStableRef(match->fullPath)
               : GroovePuterSampler::SampleRef{};
}

const SampleFileInfo* SampleIndex::findByRef(
    GroovePuterSampler::SampleRef ref) const {
  if (!ref.valid()) return nullptr;

  const SampleFileInfo* match = nullptr;
  for (const auto& file : files_) {
    if (calculateStableRef(file.fullPath) != ref) continue;
    if (match && match->fullPath != file.fullPath) {
      printf("SampleIndex: Stable SampleRef collision for %08x%08x\n",
             static_cast<unsigned>(ref.value >> 32),
             static_cast<unsigned>(ref.value & 0xFFFFFFFFULL));
      return nullptr;
    }
    match = &file;
  }
  return match;
}

std::size_t SampleIndex::legacyMatchCount(SampleId legacyId) const {
  if (legacyId.value == 0) return 0;
  std::size_t count = 0;
  for (const auto& file : files_) {
    if (file.id == legacyId) ++count;
  }
  return count;
}

GroovePuterSampler::SampleRef SampleIndex::resolveLegacyId(
    SampleId legacyId) const {
  const std::size_t matches = legacyMatchCount(legacyId);
  if (matches != 1) {
    if (legacyId.value != 0 && matches > 1) {
      printf("SampleIndex: Legacy ID %u is ambiguous\n",
             static_cast<unsigned>(legacyId.value));
    }
    return {};
  }

  for (const auto& file : files_) {
    if (file.id == legacyId) return calculateStableRef(file.fullPath);
  }
  return {};
}

const SampleFileInfo* SampleIndex::resolveLegacyFile(SampleId legacyId) const {
  const auto ref = resolveLegacyId(legacyId);
  if (!ref.valid()) return nullptr;
  return findByRef(ref);
}

bool SampleIndex::runtimeCandidateReserved(uint32_t candidate) const {
  if (candidate == 0) return true;
  // Reserve every historical basename ID, including ambiguous values. This
  // prevents a stable-only runtime ID from making an old ambiguous Scene
  // silently select one file from a collision set.
  for (const auto& file : files_) {
    if (file.id.value == candidate) return true;
  }
  return false;
}

SampleId SampleIndex::runtimeIdForFile(const SampleFileInfo& target) const {
  const auto stableRef = calculateStableRef(target.fullPath);
  const SampleFileInfo* stableFile = findByRef(stableRef);
  if (!stableRef.valid() || stableFile == nullptr ||
      stableFile->fullPath != target.fullPath) {
    return {0};
  }

  // Preserve the old runtime ID whenever it is already unambiguous. This
  // keeps all existing sampler callers source/behavior compatible in D.
  if (legacyMatchCount(target.id) == 1) return target.id;

  struct AmbiguousEntry {
    const SampleFileInfo* file;
    GroovePuterSampler::SampleRef ref;
  };
  std::vector<AmbiguousEntry> ambiguous;
  ambiguous.reserve(files_.size());

  for (const auto& file : files_) {
    if (legacyMatchCount(file.id) <= 1) continue;
    const auto ref = calculateStableRef(file.fullPath);
    if (!ref.valid() || findByRef(ref) == nullptr) continue;
    ambiguous.push_back({&file, ref});
  }

  std::sort(ambiguous.begin(), ambiguous.end(),
            [](const AmbiguousEntry& a, const AmbiguousEntry& b) {
              if (a.ref.value != b.ref.value) return a.ref.value < b.ref.value;
              return a.file->fullPath < b.file->fullPath;
            });

  std::vector<uint32_t> assigned;
  assigned.reserve(ambiguous.size());

  for (const auto& entry : ambiguous) {
    uint32_t candidate = foldStableRef(entry.ref);
    for (uint64_t attempts = 0; attempts < 0xFFFFFFFFULL; ++attempts) {
      bool used = runtimeCandidateReserved(candidate);
      if (!used) {
        used = std::find(assigned.begin(), assigned.end(), candidate) !=
               assigned.end();
      }
      if (!used) break;
      ++candidate;
      if (candidate == 0) candidate = 1;
    }

    if (runtimeCandidateReserved(candidate) ||
        std::find(assigned.begin(), assigned.end(), candidate) !=
            assigned.end()) {
      return {0};
    }
    assigned.push_back(candidate);
    if (entry.file->fullPath == target.fullPath) return {candidate};
  }

  return {0};
}

SampleId SampleIndex::runtimeIdForRef(
    GroovePuterSampler::SampleRef ref) const {
  const SampleFileInfo* file = findByRef(ref);
  return file ? runtimeIdForFile(*file) : SampleId{0};
}

SampleId SampleIndex::runtimeIdForLegacyId(SampleId legacyId) const {
  const SampleFileInfo* file = resolveLegacyFile(legacyId);
  return file ? runtimeIdForFile(*file) : SampleId{0};
}

GroovePuterSampler::SampleRef SampleIndex::resolveRuntimeId(
    SampleId runtimeId) const {
  if (runtimeId.value == 0) return {};

  GroovePuterSampler::SampleRef match{};
  for (const auto& file : files_) {
    const auto ref = calculateStableRef(file.fullPath);
    if (!ref.valid()) continue;
    if (runtimeIdForFile(file) != runtimeId) continue;
    if (match.valid() && match != ref) return {};
    match = ref;
  }
  return match;
}

const SampleFileInfo* SampleIndex::resolveRuntimeFile(
    SampleId runtimeId) const {
  const auto ref = resolveRuntimeId(runtimeId);
  return ref.valid() ? findByRef(ref) : nullptr;
}

SampleRegistryBindResult SampleIndex::bindToStore(ISampleStore& store) const {
  // C guarantees bind happens before Scene restore on Cardputer. D reuses the
  // same moment to publish the control-side identity authority to persistence.
  GroovePuterSampler::setScenePersistenceSampleIndex(this);

  SampleRegistryBindResult result{};
  result.discovered = files_.size();

  for (const auto& file : files_) {
    const auto stableRef = calculateStableRef(file.fullPath);
    const SampleFileInfo* stableFile = findByRef(stableRef);
    if (!stableRef.valid() || stableFile == nullptr ||
        stableFile->fullPath != file.fullPath) {
      ++result.rejectedStable;
      continue;
    }

    if (legacyMatchCount(file.id) > 1) ++result.rejectedLegacy;

    const SampleId runtimeId = runtimeIdForFile(file);
    if (runtimeId.value == 0) {
      ++result.rejectedStable;
      continue;
    }

    if (!store.registerFile(runtimeId, file.fullPath)) {
      ++result.rejectedStore;
      continue;
    }
    ++result.registered;
  }

  return result;
}

std::vector<std::string> SampleIndex::getSubdirectories(
    const std::string& dirPath) {
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

      if (name[0] != '.') dirs.push_back(name);
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
      if (entry->d_name[0] != '.') dirs.push_back(entry->d_name);
    }
  }
  closedir(dir);
#endif

  std::sort(dirs.begin(), dirs.end());
  return dirs;
}
