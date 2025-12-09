#pragma once

#include <string>

class SceneManager;

// Abstract interface for loading and saving scene JSON blobs.
class SceneStorage {
public:
  virtual ~SceneStorage() = default;
  virtual void initializeStorage() = 0;

  // Returns true on success and fills 'out' with the serialized scene JSON.
  virtual bool readScene(std::string& out) = 0;

  // Returns true on success after writing the provided JSON string.
  virtual bool writeScene(const std::string& data) = 0;
  // Streaming variants; default to false when not implemented.
  virtual bool readScene(SceneManager& manager) { (void)manager; return false; }
  virtual bool writeScene(const SceneManager& manager) { (void)manager; return false; }
};
