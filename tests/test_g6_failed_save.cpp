#include <cassert>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdio>
#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
SerialMock Serial; SDMock SD;
class SaveFaultStorage final : public SceneStorageSdl {
public:
 bool failWrite=false, failCleanup=false, failRollback=false;
 bool setCurrentSceneName(const std::string& name) override {
  if(failRollback && name=="source" && getCurrentSceneName()!="source") return false;
  return SceneStorageSdl::setCurrentSceneName(name);
 }
 unsigned writes=0, cleanups=0;
 bool writeScene(const SceneManager& manager) override {
  ++writes;
  return !failWrite && SceneStorageSdl::writeScene(manager);
 }
 bool clearSceneAuto() override {
  ++cleanups;
  return !failCleanup && SceneStorageSdl::clearSceneAuto();
 }
};
static std::string read(const std::filesystem::path& p) {
 std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};
}
int main() {
 namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("gp-g6-save-fault-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root);fs::current_path(root);SD.setRoot(root);
 SaveFaultStorage storage;assert(storage.setCurrentSceneName("source"));
 MiniAcid engine(kSampleRate,&storage);engine.init();engine.setSongMode(false);
 assert(engine.saveSceneAs("source"));
 auto& note=engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note;
 note=61;
 auto source=read(root/"source.json");assert(!source.empty());
 storage.failWrite=true;unsigned cleanup=storage.cleanups;
 assert(!engine.saveSceneAs("source"));assert(storage.cleanups==cleanup);
 assert(read(root/"source.json")==source);assert(note==61);
 assert(!engine.saveSceneAs("refused-target"));
 assert(storage.getCurrentSceneName()=="source");
 assert(PatternPagingService::currentProjectName()=="source");
 assert(read(root/"grooveputer_scene_name.txt")=="source");assert(note==61);
 storage.failRollback=true;
 assert(!engine.saveSceneAs("rollback-io-failed"));
 assert(storage.getCurrentSceneName()=="source");
 assert(PatternPagingService::currentProjectName()=="source");
 assert(note==61);
 storage.failRollback=false;
 storage.failWrite=false;storage.failCleanup=true;cleanup=storage.cleanups;
 assert(!engine.saveSceneAs("source"));assert(storage.cleanups==cleanup+1);
 assert(note==61);assert(storage.getCurrentSceneName()=="source");
 // Main data can already be durable when cleanup fails; success must still be false.
 assert(read(root/"source.json")!=source);
 storage.failCleanup=false;assert(engine.saveSceneAs("source"));
 assert(engine.loadSceneByName("source"));
 assert(engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note==61);
 puts("G6 write refusal, failed Save As rollback, cleanup refusal and recovery: PASS");
 return 0;
}
