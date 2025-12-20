#pragma once

#include <string>
#include <vector>

class SceneManager;

// Abstract interface for loading and saving scene JSON blobs.
class SceneStorage {
public:
  virtual ~SceneStorage() = default;
  virtual void initializeStorage() = 0;

  // Returns true on success and fills 'out' with the serialized scene JSON.
  virtual bool readScene(std::string& out) = 0;

  // Returns true on success after writing the provided JSON string.
  // it should also always write to a general, to persist the name of the current scene being opened.
  virtual bool writeScene(const std::string& data) = 0;
  // Streaming variants; default to false when not implemented.
  virtual bool readScene(SceneManager& manager) { (void)manager; return false; }
  virtual bool writeScene(const SceneManager& manager) { (void)manager; return false; }

  // return the scenes currently found on the storage
  virtual std::vector<std::string> getAvailableSceneNames() const = 0;
  // return the name of the current scene
  virtual std::string getCurrentSceneName() const = 0;
  // set the name of the current scene.
  virtual bool setCurrentSceneName(const std::string& name) = 0;
};
