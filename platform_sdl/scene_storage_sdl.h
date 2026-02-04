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
  void initializeStorage() override;
  std::vector<std::string> getAvailableSceneNames() const override;
  std::string getCurrentSceneName() const override;
  bool setCurrentSceneName(const std::string& name) override;

private:
  static constexpr const char* kDefaultSceneName = "miniacid_scene";
  static constexpr const char* kSceneNameFile = "miniacid_scene_name.txt";
  static constexpr const char* kSceneExtension = ".json";

  std::string normalizeSceneName(const std::string& name) const;
  std::string sceneFilePath() const;
  void loadStoredSceneName();
  bool persistCurrentSceneName() const;
  std::vector<std::string> findSceneNamesOnDisk() const;
  std::vector<std::string> findSceneNamesLocalStorage() const;
  std::string sceneKeyForStorage(const std::string& name) const;

  std::string currentSceneName_;
};
