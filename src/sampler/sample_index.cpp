#include "sample_index.h"

#include <cstring>
#include <algorithm>
#include <cstdio>
#include <vector>

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <SD.h>
#define USE_ARDUINO_SD 1
#else
#include <dirent.h>
#include <sys/stat.h>
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

constexpr int kMaxSampleDirectoryDepth = 8;

std::string normalizeDirectoryPath(std::string path) {
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  return path;
}

std::string joinPath(const std::string& parent, const std::string& child) {
  if (parent.empty()) return child;
  if (parent.back() == '/') return parent + child;
  return parent + "/" + child;
}

[[maybe_unused]] std::string baseName(const std::string& path) {
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string parentDirectory(const std::string& path) {
  const std::string normalized = normalizeDirectoryPath(path);
  const std::size_t slash = normalized.find_last_of('/');
  if (slash == std::string::npos) return {};
  if (slash == 0) return "/";
  return normalized.substr(0, slash);
}

bool isHiddenName(const std::string& name) {
  return !name.empty() && name.front() == '.';
}

bool isWavName(const std::string& name) {
  const char* ext = strrchr(name.c_str(), '.');
  return ext != nullptr && strcasecmp(ext, ".wav") == 0;
}

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
  rootDirectory_ = normalizeDirectoryPath(dirPath);

  printf("SampleIndex::scanDirectory: Recursively scanning '%s'...\n",
         rootDirectory_.c_str());

  if (rootDirectory_.empty()) {
    printf("SampleIndex::scanDirectory: Empty directory path\n");
    return;
  }

  scanDirectoryRecursive(rootDirectory_, 0);

  std::sort(files_.begin(), files_.end(),
            [](const SampleFileInfo& a, const SampleFileInfo& b) {
              if (a.filename != b.filename) return a.filename < b.filename;
              return a.fullPath < b.fullPath;
            });

  printf("SampleIndex::scanDirectory: Found %zu files\n", files_.size());
}

void SampleIndex::scanDirectoryRecursive(const std::string& dirPath, int depth) {
  if (depth > kMaxSampleDirectoryDepth) {
    printf("SampleIndex::scanDirectory: depth limit reached at '%s'\n",
           dirPath.c_str());
    return;
  }

  std::vector<std::string> childDirectories;

#if USE_ARDUINO_SD
  File dir = SD.open(dirPath.c_str());
  if (!dir) {
    if (depth == 0) {
      printf("SampleIndex::scanDirectory: Failed to open directory\n");
    } else {
      printf("SampleIndex::scanDirectory: Failed to open child '%s'\n",
             dirPath.c_str());
    }
    return;
  }
  if (!dir.isDirectory()) {
    printf("SampleIndex::scanDirectory: Path is not a directory: %s\n",
           dirPath.c_str());
    dir.close();
    return;
  }

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    const std::string name = baseName(entry.name());
    const bool directory = entry.isDirectory();
    entry.close();

    if (name.empty() || isHiddenName(name)) continue;

    const std::string fullPath = joinPath(dirPath, name);
    if (directory) {
      childDirectories.push_back(fullPath);
      continue;
    }

    if (!isWavName(name)) continue;

    SampleFileInfo info;
    info.filename = name;
    info.fullPath = fullPath;
    populateLegacySampleId(info);

    files_.push_back(info);
    logDiscoveredSample(info);
  }
  dir.close();

#else
  DIR* dir = opendir(dirPath.c_str());
  if (!dir) {
    if (depth == 0) {
      printf("SampleIndex::scanDirectory: opendir failed\n");
    } else {
      printf("SampleIndex::scanDirectory: child opendir failed: %s\n",
             dirPath.c_str());
    }
    return;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    const std::string name = entry->d_name;
    if (name == "." || name == ".." || isHiddenName(name)) continue;

    const std::string fullPath = joinPath(dirPath, name);
    bool directory = entry->d_type == DT_DIR;
    bool regular = entry->d_type == DT_REG;

    if (entry->d_type == DT_UNKNOWN) {
      struct stat st {};
      if (lstat(fullPath.c_str(), &st) == 0 && !S_ISLNK(st.st_mode)) {
        directory = S_ISDIR(st.st_mode);
        regular = S_ISREG(st.st_mode);
      }
    }

    if (directory) {
      childDirectories.push_back(fullPath);
      continue;
    }
    if (!regular || !isWavName(name)) continue;

    SampleFileInfo info;
    info.filename = name;
    info.fullPath = fullPath;
    populateLegacySampleId(info);

    files_.push_back(info);
    logDiscoveredSample(info);
  }
  closedir(dir);
#endif

  std::sort(childDirectories.begin(), childDirectories.end());
  for (const auto& child : childDirectories) {
    scanDirectoryRecursive(child, depth + 1);
  }
}

std::vector<const SampleFileInfo*> SampleIndex::filesInDirectory(
    const std::string& dirPath) const {
  const std::string target = normalizeDirectoryPath(dirPath);
  std::vector<const SampleFileInfo*> result;

  for (const auto& file : files_) {
    if (normalizeDirectoryPath(parentDirectory(file.fullPath)) == target) {
      result.push_back(&file);
    }
  }

  std::sort(result.begin(), result.end(),
            [](const SampleFileInfo* a, const SampleFileInfo* b) {
              if (a->filename != b->filename) return a->filename < b->filename;
              return a->fullPath < b->fullPath;
            });
  return result;
}

std::vector<std::string> SampleIndex::indexedSubdirectories(
    const std::string& dirPath) const {
  const std::string target = normalizeDirectoryPath(dirPath);
  if (target.empty()) return {};

  const std::string prefix = target.back() == '/' ? target : target + "/";
  std::vector<std::string> result;

  for (const auto& file : files_) {
    if (file.fullPath.rfind(prefix, 0) != 0) continue;
    const std::string remainder = file.fullPath.substr(prefix.size());
    const std::size_t slash = remainder.find('/');
    if (slash == std::string::npos) continue;

    const std::string child = prefix + remainder.substr(0, slash);
    if (std::find(result.begin(), result.end(), child) == result.end()) {
      result.push_back(child);
    }
  }

  std::sort(result.begin(), result.end());
  return result;
}

SampleId SampleIndex::findIdByFilename(const std::string& filename) const {
  const SampleFileInfo* match = nullptr;
  for (const auto& file : files_) {
    if (file.filename != filename) continue;
    if (match != nullptr && match->fullPath != file.fullPath) return {0};
    match = &file;
  }
  return match ? match->id : SampleId{0};
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
      const std::string name = baseName(entry.name());
      if (!name.empty() && !isHiddenName(name)) dirs.push_back(name);
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
      const std::string name = entry->d_name;
      if (name != "." && name != ".." && !isHiddenName(name)) {
        dirs.push_back(name);
      }
    }
  }
  closedir(dir);
#endif

  std::sort(dirs.begin(), dirs.end());
  return dirs;
}
