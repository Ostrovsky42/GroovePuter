#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
SerialMock Serial; SDMock SD;
class FaultAfterValidatedRead : public SceneStorageSdl {
public:
 bool armed=false,beforePrepare=false;int damaged=0;std::filesystem::path root;
 void damage() {
  for(const auto& entry:std::filesystem::recursive_directory_iterator(root)) {
   const auto path=entry.path();
   if(entry.is_regular_file() && path.extension()==".gpml" && path.string().find("g6-melody-b")!=std::string::npos) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"LATE IO FAILURE";++damaged;
   }
  }
 }
 bool readScene(SceneManager& manager, const SceneSynthPreparation& prepare = {}, bool publishSelection = false) override {
  if(armed && beforePrepare && getCurrentSceneName()=="g6-melody-b") {
   return SceneStorageSdl::readScene(manager,[&](const Scene& scene,uint8_t a,uint8_t b){
    damage();return prepare && prepare(scene,a,b);
   }, publishSelection);
  }
  const bool ok=SceneStorageSdl::readScene(manager, prepare, publishSelection);
  if(ok && armed && getCurrentSceneName()=="g6-melody-b") damage();
  return ok;
 }
};
int main(int argc,char** argv){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("gp-g6-melody-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root);fs::current_path(root);SD.setRoot(root);
 FaultAfterValidatedRead storage;storage.root=root;assert(storage.setCurrentSceneName("g6-melody-a"));
 MiniAcid engine(kSampleRate,&storage);engine.init();engine.setSongMode(false);
 assert(engine.saveSceneToStorage());assert(engine.saveSceneAs("g6-melody-b"));
 for(int v=0;v<2;++v){
  engine.set303PatternIndex(v,0);
  PhraseRuntime::RuntimeSynthEventBuffer melody{};melody.lengthTicks=8*PhraseRuntime::kTicksPerBar;melody.count=8;
  for(int i=0;i<8;++i){auto& e=melody.events[i];e.startTick=i*PhraseRuntime::kTicksPerBar;e.durationSubticks=48*PhraseRuntime::kSubticksPerTick;e.note=48+v*12+i;e.velocity=100;e.probability=100;}
  engine.workingMaterial_[v].storeMelody(melody);
  assert(engine.acceptMaterialWorking(v)==MiniAcid::AcceptResult::Accepted);
 }
 if(argc>1 && std::string(argv[1])=="nonselected") {
  engine.set303PatternIndex(0,1);engine.set303PatternIndex(1,1);
 }
 assert(engine.saveSceneToStorage());
 assert(engine.loadSceneByName("g6-melody-b"));
 std::string targetCanonical;assert(engine.sceneManager().writeSceneJson(targetCanonical));
 const auto targetA=GroovePuterMaterial::versionForMelody(*engine.workingMaterial_[0].melodyIfHeld());
 const auto targetB=GroovePuterMaterial::versionForMelody(*engine.workingMaterial_[1].melodyIfHeld());
 assert(engine.loadSceneByName("g6-melody-a"));
 PhraseRuntime::RuntimeSynthEventBuffer unsaved{};
 unsaved.lengthTicks=8*PhraseRuntime::kTicksPerBar;unsaved.count=1;
 unsaved.events[0].note=77;unsaved.events[0].velocity=100;
 unsaved.events[0].probability=100;unsaved.events[0].durationSubticks=128;
 for(int voice=0;voice<2;++voice) engine.workingMaterial_[voice].storeMelody(unsaved);
 const auto workingVersion=GroovePuterMaterial::versionForMelody(unsaved);
 std::string before;assert(engine.sceneManager().writeSceneJson(before));
 storage.armed=true;storage.beforePrepare=argc>1 && std::string(argv[1])=="prepare-fail";
 const bool loaded=engine.loadSceneByName("g6-melody-b");
 std::string after;assert(engine.sceneManager().writeSceneJson(after));
 printf("G6_LATE_MELODY_READ files=%d loaded=%d canonical_equal=%d project=%s heldA=%u heldB=%u\n",storage.damaged,loaded,before==after,storage.getCurrentSceneName().c_str(),unsigned(engine.workingMaterial_[0].holdsMelody()),unsigned(engine.workingMaterial_[1].holdsMelody()));
 fflush(stdout);
 if(storage.beforePrepare) assert(!loaded);
 if(loaded) {
  assert(storage.getCurrentSceneName()=="g6-melody-b" && after==targetCanonical);
  const auto* a=engine.workingMaterial_[0].melodyIfHeld();
  const auto* b=engine.workingMaterial_[1].melodyIfHeld();
  assert(a && b && a->count==8 && b->count==8);
  assert(GroovePuterMaterial::versionForMelody(*a)==targetA);
  assert(GroovePuterMaterial::versionForMelody(*b)==targetB);
  puts("G6 late-read fault: complete prepared target retained, no hydration reread: PASS");
  return 0;
 }
 for(int voice=0;voice<2;++voice) {
  const auto* retained=engine.workingMaterial_[voice].melodyIfHeld();
  assert(retained && GroovePuterMaterial::versionForMelody(*retained)==workingVersion);
 }
 return !loaded && before==after && storage.getCurrentSceneName()=="g6-melody-a"?0:1;
}
