#pragma once
#ifndef SCENE_STORAGE_H
#define SCENE_STORAGE_H

#include <string>
#include <functional>
#include <cstdint>
#include <vector>

class SceneManager;
struct Scene;
// Temporary load preparation; selected resident slots A/B come from parsed JSON.
using SceneSynthPreparation = std::function<bool(const Scene&, uint8_t, uint8_t)>;

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
  virtual bool readScene(SceneManager& manager, const SceneSynthPreparation& prepare = {}, bool publishSelection = false) = 0;
  virtual bool writeScene(const SceneManager& manager) = 0;
  
  // Auto-save variants (saves to *.auto.json)
  virtual bool writeSceneAuto(const SceneManager& manager) = 0;
  // Reads only the recovery snapshot. The caller owns fallback to the main scene.
  virtual bool readSceneAuto(SceneManager& manager, const SceneSynthPreparation& prepare = {}, bool publishSelection = false) = 0;
  virtual bool hasSceneAuto() const = 0;
  virtual bool clearSceneAuto() = 0;

  // return the scenes currently found on the storage
  virtual std::vector<std::string> getAvailableSceneNames() const = 0;
  // return the name of the current scene
  virtual std::string getCurrentSceneName() const = 0;
  // set the name of the current scene.
  virtual bool setCurrentSceneName(const std::string& name) = 0;
  // Load selection admits/migrates a namespace but defers the durable boot pointer.
  virtual bool beginSceneLoad(const std::string& name) = 0;
  // Failure rollback is RAM-only and must remain usable when storage has failed.
  virtual void restoreSceneSelection(const std::string& name) = 0;
};

#endif // SCENE_STORAGE_H
