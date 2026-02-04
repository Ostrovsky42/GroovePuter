#pragma once

#include <string>
#include <vector>
#include "scene_storage.h"

class SceneStorageCardputer : public SceneStorage {
public:
  SceneStorageCardputer() : isInitialized_(false), currentSceneName_(kDefaultSceneName) {}
  bool readScene(std::string& out) override;
  bool writeScene(const std::string& data) override;
  bool readScene(SceneManager& manager) override;
  bool writeScene(const SceneManager& manager) override;
  bool writeSceneAuto(const SceneManager& manager) override;
  bool readSceneAuto(SceneManager& manager) override;
  void initializeStorage() override;
  std::vector<std::string> getAvailableSceneNames() const override;
  std::string getCurrentSceneName() const override;
  bool setCurrentSceneName(const std::string& name) override;

private:
  static constexpr const char* kDefaultSceneName = "miniacid_scene";
  static constexpr const char* kSceneNamePath = "/scenes/miniacid_scene_name.txt";
  static constexpr const char* kScenesDirectory = "/scenes";
  static constexpr const char* kSceneExtension = ".json";
  static constexpr const char* kAutoSceneExtension = ".auto.json";

  std::string scenePathFor(const std::string& name) const;
  std::string autoScenePathFor(const std::string& name) const;
  std::string currentScenePath() const;
  std::string currentAutoScenePath() const;
  std::string normalizeSceneName(const std::string& name) const;
  void loadStoredSceneName();
  bool persistCurrentSceneName() const;

  bool isInitialized_;
  std::string currentSceneName_;

};
