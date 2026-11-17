#ifndef GROOVEPUTER_SRC_STORAGE_DATA_ROOT_MIGRATION_H
#define GROOVEPUTER_SRC_STORAGE_DATA_ROOT_MIGRATION_H

// One-time copy of GroovePuter data from the card root into GROOVEPUTER_DATA_ROOT.
//
// Rules:
//  - Copies, never renames or deletes. Legacy folders stay on the card.
//  - Per file: a file is copied only when the target does not exist yet. A
//    folder that already exists (SceneManager or PatternPaging may create it
//    before storage init) therefore does not block the copy, and a retry after a
//    failed run fills only the gaps. New data is never overwritten by old data.
//  - Each file is written to "<target>.migrating", size-checked, then renamed,
//    so an interrupted copy never leaves a truncated file under the final name.
//  - The marker is written only when every source was copied. Once it exists,
//    the migration does nothing.
//  - Scene voice cache ("scenes/voices") is not copied; it is rebuilt in place.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

#include "../platform/log.h"
#include "data_root.h"

namespace GroovePuterStorage {
namespace migration_detail {

inline constexpr char kLegacyScenes[] = "/scenes";
inline constexpr char kLegacyPatterns[] = "/patterns";
inline constexpr char kLegacyProjects[] = "/projects";
inline constexpr char kLegacyMidi[] = "/midi";
inline constexpr char kLegacyTiltFile[] = "/grooveputer_tilt.txt";
inline constexpr char kVoiceCacheLeaf[] = "voices";
inline constexpr char kStagingSuffix[] = ".migrating";
inline constexpr char kMarkerText[] = "grooveputer data root v1\n";

inline bool endsWith(const std::string& value, const char* suffix) {
  const std::string s(suffix);
  return value.size() >= s.size() &&
         value.compare(value.size() - s.size(), s.size(), s) == 0;
}

inline std::string leafOf(const std::string& name) {
  const size_t slash = name.find_last_of('/');
  return slash == std::string::npos ? name : name.substr(slash + 1);
}

inline bool ensureDirectory(const std::string& path) {
  return SD.exists(path.c_str()) || SD.mkdir(path.c_str());
}

// Stale temporaries from the old firmware or an interrupted copy are not data.
inline bool isStaleTemporary(const std::string& leaf) {
  return endsWith(leaf, ".tmp") || endsWith(leaf, kStagingSuffix);
}

inline bool copyFileIfAbsent(const std::string& source, const std::string& target) {
  if (SD.exists(target.c_str())) return true;

  File in = SD.open(source.c_str(), FILE_READ);
  if (!in) return false;
  const size_t expected = in.size();

  const std::string staging = target + kStagingSuffix;
  SD.remove(staging.c_str());
  File out = SD.open(staging.c_str(), FILE_WRITE);
  if (!out) {
    in.close();
    return false;
  }

  uint8_t buffer[512];
  size_t copied = 0;
  bool ok = true;
  while (in.available()) {
    const size_t readCount = in.read(buffer, sizeof(buffer));
    if (readCount == 0 || out.write(buffer, readCount) != readCount) {
      ok = false;
      break;
    }
    copied += readCount;
  }
  out.flush();
  out.close();
  in.close();

  ok = ok && copied == expected;
  if (ok) {
    File check = SD.open(staging.c_str(), FILE_READ);
    ok = check && check.size() == expected;
    check.close();
  }
  if (ok) ok = SD.rename(staging.c_str(), target.c_str());
  if (!ok) {
    SD.remove(staging.c_str());
    LOG_PRINTLN("[DATA-ROOT] copy failed");
  }
  return ok;
}

// Copies the tree below `source` into `target`. `skipLeaf` names one
// top-level subfolder to leave out (the voice cache, under scenes). Best effort
// over all entries; returns false if any entry failed.
inline bool copyTree(const std::string& source, const std::string& target,
                     const char* skipLeaf) {
  if (!SD.exists(source.c_str())) return true;
  if (!ensureDirectory(target)) return false;

  // Collect names and close the source handle before recursing. The card has
  // only a few file handles.
  std::vector<std::pair<std::string, bool>> entries;
  {
    File dir = SD.open(source.c_str(), FILE_READ);
    if (!dir || !dir.isDirectory()) return false;
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
      const bool isDirectory = entry.isDirectory();
      std::string leaf = leafOf(entry.name());
      entry.close();
      if (leaf.empty() || isStaleTemporary(leaf)) continue;
      entries.emplace_back(std::move(leaf), isDirectory);
    }
    dir.close();
  }

  bool ok = true;
  for (const auto& [leaf, isDirectory] : entries) {
    if (isDirectory) {
      if (skipLeaf != nullptr && leaf == skipLeaf) continue;
      ok = copyTree(source + "/" + leaf, target + "/" + leaf, nullptr) && ok;
    } else {
      ok = copyFileIfAbsent(source + "/" + leaf, target + "/" + leaf) && ok;
    }
  }
  return ok;
}

inline bool writeMarker() {
  SD.remove(kMigrationMarker);
  File marker = SD.open(kMigrationMarker, FILE_WRITE);
  if (!marker) return false;
  marker.print(kMarkerText);
  marker.flush();
  marker.close();
  return SD.exists(kMigrationMarker);
}

}  // namespace migration_detail

// Read-side fallback. Returns dataRootPath when it exists under the data root;
// otherwise the same file under the legacy card root when that exists; otherwise
// dataRootPath itself (so the caller sees a plain miss). Only reads use this.
// Writes always go to the data root.
inline std::string readPathFor(const std::string& dataRootPath) {
  using namespace migration_detail;
  if (SD.exists(dataRootPath.c_str())) return dataRootPath;
  const size_t rootLen = sizeof(kDataRoot) - 1;
  if (dataRootPath.size() <= rootLen ||
      dataRootPath.compare(0, rootLen, kDataRoot) != 0 ||
      dataRootPath[rootLen] != '/') {
    return dataRootPath;
  }
  const std::string legacy = dataRootPath.substr(rootLen);
  return SD.exists(legacy.c_str()) ? legacy : dataRootPath;
}

// Call once after the card is mounted and before any scene, pattern, MIDI or
// TILT file is touched. Idempotent: returns true without I/O when the marker
// exists. Returns false if any copy failed; the marker is then not written and
// the next init retries.
inline bool migrateLegacyDataRootOnce() {
  using namespace migration_detail;
  if (SD.exists(kMigrationMarker)) return true;
  if (!ensureDirectory(kDataRoot)) return false;

  bool ok = true;
  ok = copyTree(kLegacyScenes, kScenesDirectory, kVoiceCacheLeaf) && ok;
  ok = copyTree(kLegacyPatterns, kPatternsDirectory, nullptr) && ok;
  ok = copyTree(kLegacyProjects, kProjectsDirectory, nullptr) && ok;
  ok = copyTree(kLegacyMidi, kMidiDirectory, nullptr) && ok;
  if (SD.exists(kLegacyTiltFile)) {
    ok = copyFileIfAbsent(kLegacyTiltFile, kTiltSettingsFile) && ok;
  }
  if (!ok) {
    LOG_PRINTLN("[DATA-ROOT] migration incomplete; will retry on next init");
    return false;
  }
  if (!writeMarker()) {
    LOG_PRINTLN("[DATA-ROOT] migration marker not written");
    return false;
  }
  LOG_PRINTLN("[DATA-ROOT] migration complete");
  return true;
}

}  // namespace GroovePuterStorage

#endif  // GROOVEPUTER_SRC_STORAGE_DATA_ROOT_MIGRATION_H
