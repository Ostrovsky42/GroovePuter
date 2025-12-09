#pragma once

#include <string>
#include "scene_storage.h"

class SceneStorageCardputer : public SceneStorage {
public:
  SceneStorageCardputer() : isInitialized_(false) {}
  bool readScene(std::string& out) override;
  bool writeScene(const std::string& data) override;
  bool readScene(SceneManager& manager) override;
  bool writeScene(const SceneManager& manager) override;
  void initializeStorage() override;

private:
  static constexpr const char* kScenePath = "/miniacid_scene.json";
  bool isInitialized_;

};
