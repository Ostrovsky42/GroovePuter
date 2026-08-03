#pragma once

#include <string>
#include <vector>
#include "../scene_storage.h"

class SceneStorageSdl : public SceneStorage {
public:
  SceneStorageSdl();
  bool readScene(std::string& out) override;
  bool writeScene(const std::string& data) override;
  bool writeScene(const SceneManager& manager) override;
  bool readScene(SceneManager& manager) override;
  bool writeSceneAuto(const SceneManager& manager) override;
  bool readSceneAuto(SceneManager& manager) override;
  bool hasSceneAuto() const override;
  bool clearSceneAuto() override;
  void initializeStorage() override;
  std::vector<std::string> getAvailableSceneNames() const override;
  std::string getCurrentSceneName() const override;
  bool setCurrentSceneName(const std::string& name) override;

private:
  static constexpr const char* kDefaultSceneName = "grooveputer_scene";
  static constexpr const char* kSceneNameFile = "grooveputer_scene_name.txt";
  static constexpr const char* kSceneExtension = ".json";
  static constexpr const char* kAutoSceneExtension = ".auto.json";

  std::string normalizeSceneName(const std::string& name) const;
  std::string sceneFilePath() const;
  std::string autoSceneFilePath() const;
  std::string autoSceneKeyForStorage() const;
  void loadStoredSceneName();
  bool persistCurrentSceneName() const;
  std::vector<std::string> findSceneNamesOnDisk() const;
  std::vector<std::string> findSceneNamesLocalStorage() const;
  std::string sceneKeyForStorage(const std::string& name) const;

  std::string currentSceneName_;
};
