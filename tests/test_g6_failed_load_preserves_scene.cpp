#include <bits/stdc++.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/state/undo_receipts.h"
#include "src/audio/pattern_paging.h"
#include "src/output/output_ownership.h"
SerialMock Serial; SDMock SD;
int main(int argc, char**) {
 namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/("gp-g6-corrupt-page-probe-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 fs::create_directories(root);fs::current_path(root);SD.setRoot(root);
 {
  SceneManager manager;manager.loadDefaultScene();
  manager.currentScene().synthABanks[0].patterns[0].steps[0].note=41;
  std::string target;assert(manager.writeSceneJson(target));
  manager.currentScene().synthABanks[0].patterns[0].steps[0].note=73;
  std::string before;assert(manager.writeSceneJson(before));
  struct LateFailure {
   const std::string& text;size_t pos=0;
   int read(){return pos<text.size()?static_cast<unsigned char>(text[pos++]):-1;}
   bool failed(){return true;}
  } reader{target};
  const bool loaded=manager.loadSceneEvented(reader, false, [&]() { return !reader.failed(); });
  std::string after;assert(manager.writeSceneJson(after));
  printf("G6_FINAL_ACK loaded=%d canonical_equal=%d\n",loaded,before==after);
  if(loaded || before!=after) return 1;
  LateFailure accepted{target};
  assert(!PatternPagingService::pageExists(0));
  assert(manager.loadSceneEvented(accepted,true,[](){return true;}));
  std::string committed;assert(manager.writeSceneJson(committed));assert(committed==target);
 }
 SceneStorageSdl storage;assert(storage.setCurrentSceneName("g6-a"));
 MiniAcid engine(kSampleRate,&storage);engine.init();engine.setSongMode(false);
 engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note=73;
 assert(engine.saveSceneToStorage());assert(engine.saveSceneAs("g6-b"));
 engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note=41;
 assert(engine.saveSceneToStorage());assert(engine.loadSceneByName("g6-a"));
 engine.set303PatternIndex(0,3);
 GroovePuterMaterial::GeneratedPhraseRecipe recipe{};
 engine.publishGeneratedPhraseRecipe(recipe);
 auto& undo=GroovePuterUndo::undoOwner();
 GroovePuterUndo::SynthPatternUndoPayload receipt{};
 assert(GroovePuterUndo::captureCurrentSynthPatternUndo(engine.sceneManager(),0,receipt));
 assert(undo.commitPrepared(GroovePuterUndo::UndoKind::Pattern,receipt,[&](){
  engine.sceneManager().currentScene().synthABanks[0].patterns[3].steps[1].note=74;
 }));
 const auto revision=GroovePuterState::sceneRevisionSnapshot();
 const auto paging=PatternPagingService::activePageIndex();
 const auto currentPage=engine.currentPageIndex();
 assert(GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA,GroovePuterOutput::Mode::Internal));
 const auto output=GroovePuterOutput::state(GroovePuterOutput::Track::SynthA);
 const auto epoch=GroovePuterOutput::midiDisableEpoch(GroovePuterOutput::Track::SynthA);
 std::string before;assert(engine.sceneManager().writeSceneJson(before));
 int corrupt=0;
 for(const auto& entry:fs::recursive_directory_iterator(root)) {
  const auto path=entry.path();
  if(entry.is_regular_file() && path.filename().string().find("page_")==0 && path.string().find("g6-b")!=std::string::npos) {
   if(argc>1) {
    std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);
    file.seekg(-1,std::ios::end);char byte=0;file.get(byte);byte^=0x5a;
    file.seekp(-1,std::ios::end);file.put(byte);
   } else {std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<"CORRUPT";}
   ++corrupt;
  }
 }
 assert(corrupt>0);
 bool loaded=engine.loadSceneByName("g6-b");
 std::string after;assert(engine.sceneManager().writeSceneJson(after));
 printf("G6_CORRUPT_PAGE files=%d loaded=%d canonical_equal=%d current_project=%s note=%d\n",corrupt,loaded,before==after,storage.getCurrentSceneName().c_str(),int(engine.sceneManager().currentScene().synthABanks[0].patterns[0].steps[0].note));
 assert(engine.currentPageIndex()==currentPage);
 assert(PatternPagingService::activePageIndex()==paging);
 assert(engine.generatedPhraseRecipe()!=nullptr);
 assert(GroovePuterState::sceneRevisionSnapshot().currentRevision==revision.currentRevision);
 assert(GroovePuterState::sceneRevisionSnapshot().persistedRevision==revision.persistedRevision);
 GroovePuterUndo::SynthPatternUndoPayload retained{};
 assert(undo.read(GroovePuterUndo::UndoKind::Pattern,retained));
 assert(std::memcmp(&receipt,&retained,sizeof(receipt))==0);
 const auto actualOutput=GroovePuterOutput::state(GroovePuterOutput::Track::SynthA);
 assert(actualOutput.explicitMode==output.explicitMode && actualOutput.mode==output.mode);
 assert(GroovePuterOutput::midiDisableEpoch(GroovePuterOutput::Track::SynthA)==epoch);
 return (!loaded && before==after && storage.getCurrentSceneName()=="g6-a")?0:1;
}
