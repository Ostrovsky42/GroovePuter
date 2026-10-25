#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
SerialMock Serial; SDMock SD;
int main(int argc,char** argv){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("gp-g6-melody-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root);fs::current_path(root);SD.setRoot(root);
 const std::string mode=argc>1?argv[1]:"header";
 const int storedPage=mode=="last-slot"?kMaxPages-1:1;
 const int storedBank=mode=="last-slot"?1:0;
 const int storedPattern=mode=="last-slot"?7:0;
 SceneStorageSdl storage;assert(storage.setCurrentSceneName("g6-melody-a"));
 MiniAcid engine(kSampleRate,&storage);engine.init();engine.setSongMode(false);
 assert(engine.saveSceneToStorage());assert(engine.saveSceneAs("g6-melody-b"));
 engine.sceneManager().setPage(storedPage);assert(engine.sceneManager().currentPageIndex()==storedPage);
 engine.setCurrentPage(storedPage);assert(engine.rebuildPatternRuntimeEventBank());
 for(int v=0;v<2;++v){
  engine.set303BankIndex(v,storedBank);engine.set303PatternIndex(v,storedPattern);
  PhraseRuntime::RuntimeSynthEventBuffer melody{};melody.lengthTicks=8*PhraseRuntime::kTicksPerBar;melody.count=8;
  for(int i=0;i<8;++i){auto& e=melody.events[i];e.startTick=i*PhraseRuntime::kTicksPerBar;e.durationSubticks=48*PhraseRuntime::kSubticksPerTick;e.note=48+v*12+i;e.velocity=100;e.probability=100;}
  engine.workingMaterial_[v].storeMelody(melody);
  assert(engine.acceptMaterialWorking(v)==MiniAcid::AcceptResult::Accepted);
 }
 engine.sceneManager().setPage(0);assert(engine.sceneManager().currentPageIndex()==0);
 engine.setCurrentPage(0);assert(engine.rebuildPatternRuntimeEventBank());
 engine.sceneManager().setSongLength(2);
 engine.sceneManager().setSongPattern(1,SongTrack::SynthA,songPatternFromPageBankIndex(storedPage,storedBank,storedPattern));
 engine.sceneManager().setSongPattern(1,SongTrack::SynthB,songPatternFromPageBankIndex(storedPage,storedBank,storedPattern));
 if(argc>1 && std::string(argv[1])=="nonselected") {
  engine.set303PatternIndex(0,1);engine.set303PatternIndex(1,1);
 }
 assert(engine.saveSceneToStorage());
 if(mode=="drums-page-missing") {
  engine.sceneManager().setSongPattern(1,SongTrack::SynthA,0);
  engine.sceneManager().setSongPattern(1,SongTrack::SynthB,0);
  engine.sceneManager().setSongPattern(1,SongTrack::Drums,songPatternFromPageBankIndex(storedPage,storedBank,storedPattern));
  assert(engine.saveSceneToStorage());
 }
 if(mode=="legacy-json") {
  for(const auto& entry:fs::recursive_directory_iterator(root)) {
   const auto path=entry.path();
   if(entry.is_regular_file() && path.string().find("g6-melody-b")!=std::string::npos && path.filename().string().find("page_00")==0) fs::remove(path);
  }
 }
 assert(engine.loadSceneByName("g6-melody-b"));
 assert(engine.loadSceneByName("g6-melody-a"));
 PhraseRuntime::RuntimeSynthEventBuffer unsaved{};
 unsaved.lengthTicks=8*PhraseRuntime::kTicksPerBar;unsaved.count=1;
 unsaved.events[0].note=77;unsaved.events[0].velocity=100;
 unsaved.events[0].probability=100;unsaved.events[0].durationSubticks=128;
 for(int voice=0;voice<2;++voice) engine.workingMaterial_[voice].storeMelody(unsaved);
 const auto workingVersion=GroovePuterMaterial::versionForMelody(unsaved);
 std::string before;assert(engine.sceneManager().writeSceneJson(before));
 int corrupted=0;
 for(const auto& entry:fs::recursive_directory_iterator(root)){
  const auto path=entry.path();
  const bool pageFault=mode=="page-crc" || mode=="page-missing" || mode=="drums-page-missing";
  const bool target=pageFault ? path.filename().string().find("page_01")==0 : path.extension()==".gpml";
  if(entry.is_regular_file() && path.string().find("g6-melody-b")!=std::string::npos && target){
   if(mode=="page-missing" || mode=="drums-page-missing") fs::remove(path);
   else if(mode=="page-crc") {
    std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);
    file.seekg(-1,std::ios::end);char byte=0;file.get(byte);byte^=0x5a;
    file.seekp(-1,std::ios::end);file.put(byte);
   } else
   if(argc>1 && std::string(argv[1])=="oversized") {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);out.seekp(65535);out.put('x');
   } else if(argc>1 && std::string(argv[1])=="missing") fs::remove(path);
   else if(argc>1 && std::string(argv[1])=="crc") {
    std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);
    file.seekg(-1,std::ios::end);char byte=0;file.get(byte);byte^=0x5a;
    file.seekp(-1,std::ios::end);file.put(byte);
   } else {std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<"BROKEN";}
   ++corrupted;
  }
 }
 assert(corrupted>0);
 const bool loaded=engine.loadSceneByName("g6-melody-b");
 std::string after;assert(engine.sceneManager().writeSceneJson(after));
 printf("G6_NONRESIDENT_MELODY files=%d loaded=%d canonical_equal=%d project=%s heldA=%u heldB=%u\n",corrupted,loaded,before==after,storage.getCurrentSceneName().c_str(),unsigned(engine.workingMaterial_[0].holdsMelody()),unsigned(engine.workingMaterial_[1].holdsMelody()));
 fflush(stdout);
 for(int voice=0;voice<2;++voice) {
  const auto* retained=engine.workingMaterial_[voice].melodyIfHeld();
  assert(retained && GroovePuterMaterial::versionForMelody(*retained)==workingVersion);
 }
 return !loaded && before==after && storage.getCurrentSceneName()=="g6-melody-a"?0:1;
}
