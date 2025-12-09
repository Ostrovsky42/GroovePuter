#pragma once

#include <string>
#include "../scene_storage.h"

class SceneStorageSdl : public SceneStorage {
public:
  bool readScene(std::string& out) override;
  bool writeScene(const std::string& data) override;
  void initializeStorage() override {};
};
