#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
SerialMock Serial; SDMock SD;
class ReadFailureStorage:public SceneStorageSdl{
public:
 bool armed=false,unavailable=false;std::filesystem::path root;
 bool readScene(SceneManager& manager,const SceneSynthPreparation& prepare={})override{
  if(armed && getCurrentSceneName()=="target-b"){
   if(unavailable){std::ofstream(root/"sd-offline")<<"offline";SD.setRoot(root/"sd-offline");}
   else {const auto pointer=root/"grooveputer_scene_name.txt";std::filesystem::rename(pointer,root/"pointer-before-failure");std::filesystem::create_directory(pointer);}
   return false;
  }
  return SceneStorageSdl::readScene(manager,prepare);
 }
};
int main(int argc,char**){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("gp-g6-selection-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root);fs::current_path(root);SD.setRoot(root);
 ReadFailureStorage storage;storage.root=root;storage.unavailable=argc>1;
 assert(storage.setCurrentSceneName("source-a"));MiniAcid engine(kSampleRate,&storage);engine.init();engine.setSongMode(false);
 engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note=73;assert(engine.saveSceneToStorage());
 assert(engine.saveSceneAs("target-b"));engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note=41;assert(engine.saveSceneToStorage());
 assert(engine.loadSceneByName("source-a"));std::string before;assert(engine.sceneManager().writeSceneJson(before));
 storage.armed=true;const bool loaded=engine.loadSceneByName("target-b");std::string after;assert(engine.sceneManager().writeSceneJson(after));
 printf("G6_SELECTION_FAILURE loaded=%d canonical_equal=%d storage=%s paging=%s unavailable=%d\n",loaded,before==after,storage.getCurrentSceneName().c_str(),PatternPagingService::currentProjectName().c_str(),storage.unavailable);fflush(stdout);
 return !loaded && before==after && storage.getCurrentSceneName()=="source-a" && PatternPagingService::currentProjectName()=="source-a"?0:1;
}
