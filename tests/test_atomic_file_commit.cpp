#include <cassert>
#include <map>
#include <string>

#include "src/storage/atomic_file_commit.h"

struct FaultStorage {
  std::map<std::string, std::string> files;
  int renameCount = 0;
  int failRenameAt[3] = {0, 0, 0};

  bool exists(const char* path) const { return files.count(path) != 0; }
  bool remove(const char* path) { return files.erase(path) != 0; }
  bool rename(const char* from, const char* to) {
    ++renameCount;
    for (int failed : failRenameAt) if (renameCount == failed) return false;
    auto source = files.find(from);
    if (source == files.end() || files.count(to)) return false;
    files[to] = source->second;
    files.erase(source);
    return true;
  }
};

int main() {
  FaultStorage storage;
  storage.files["pointer"] = "project-A";
  storage.files["pointer.tmp"] = "project-B";
  // Main->backup succeeds; promotion and immediate backup restoration fail.
  storage.failRenameAt[0] = 2;
  storage.failRenameAt[1] = 3;
  assert(!GroovePuterStorage::commitTempFile(
      storage, "pointer", "pointer.tmp", "pointer.bak"));
  assert(!storage.exists("pointer"));
  assert(storage.files.at("pointer.bak") == "project-A");

  // A later attempt with no main must retain the only old pointer on failure.
  storage.files["pointer.tmp"] = "project-B";
  storage.failRenameAt[2] = 4;
  assert(!GroovePuterStorage::commitTempFile(
      storage, "pointer", "pointer.tmp", "pointer.bak"));
  assert(storage.files.at("pointer.bak") == "project-A");

  // Once storage recovers, the new pointer can be promoted without deleting A.
  storage.files["pointer.tmp"] = "project-B";
  assert(GroovePuterStorage::commitTempFile(
      storage, "pointer", "pointer.tmp", "pointer.bak"));
  assert(storage.files.at("pointer") == "project-B");
  assert(storage.files.at("pointer.bak") == "project-A");
}
