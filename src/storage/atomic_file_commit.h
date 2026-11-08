#pragma once

#include <string>

namespace GroovePuterStorage {

// Promote a fully written temporary file while preserving a backup-only
// recovery copy if an earlier promotion was interrupted.
template <typename Storage>
bool commitTempFile(Storage& storage, const std::string& path,
                    const std::string& tempPath,
                    const std::string& backupPath) {
  const bool hadMain = storage.exists(path.c_str());
  if (hadMain && storage.exists(backupPath.c_str()) &&
      !storage.remove(backupPath.c_str())) {
    storage.remove(tempPath.c_str());
    return false;
  }
  if (hadMain && !storage.rename(path.c_str(), backupPath.c_str())) {
    storage.remove(tempPath.c_str());
    return false;
  }

  if (!storage.rename(tempPath.c_str(), path.c_str())) {
    if (hadMain) storage.rename(backupPath.c_str(), path.c_str());
    storage.remove(tempPath.c_str());
    return false;
  }
  return true;
}

}  // namespace GroovePuterStorage
