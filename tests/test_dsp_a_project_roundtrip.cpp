#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
SerialMock Serial;
SDMock SD;

int main() {
  namespace fs=std::filesystem;
  const auto previous=fs::current_path();
  const auto root=fs::temp_directory_path()/"gp-dsp-a-project-roundtrip";
  fs::remove_all(root);fs::create_directories(root);fs::current_path(root);SD.setRoot(root);
  for(const char* name : {"TB303","SH101","WAVEMORPH","AY","SID","SN76489"}) {
    SceneStorageSdl storage;
    const std::string project=std::string("dsp-a-")+name;
    assert(storage.setCurrentSceneName(project));
    MiniAcid engine(kSampleRate,&storage);engine.init();engine.setSongMode(false);
    SynthVoiceState expected[2];
    for(int v=0;v<2;++v) {
      engine.setSynthEngine(v,name);
      for(uint8_t p=0;p<engine.synthParameterCount(v);++p) engine.synthVoices_[v].setParameterNormalized(p,.11f*(p+1)+v*.03f);
      if(std::string(name)=="TB303") {
        engine.set303Parameter(TB303ParamId::Oscillator,3,v);
        engine.set303Parameter(TB303ParamId::FilterType,4,v);
      }
      expected[v]=engine.synthVoices_[v].getState();
    }
    engine.params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(.63f);
    engine.setTrackVolume(VoiceId::SynthA,.71f);engine.setTrackVolume(VoiceId::SynthB,.43f);
    assert(engine.saveSceneToStorage());
    engine.setSynthEngine(0,"AY");engine.setSynthEngine(1,"TB303");
    engine.params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(.12f);
    engine.setTrackVolume(VoiceId::SynthA,.1f);engine.setTrackVolume(VoiceId::SynthB,.1f);
    assert(engine.loadSceneByName(project));
    for(int v=0;v<2;++v) {
      assert(engine.currentSynthEngineName(v)==name);
      const auto got=engine.synthVoices_[v].getState();
      assert(got.engineType==expected[v].engineType && got.paramCount==expected[v].paramCount);
      for(uint8_t p=0;p<got.paramCount;++p) {
        if(std::fabs(got.params[p]-expected[v].params[p])>=.0002f) std::fprintf(stderr,"ROUNDTRIP engine=%s voice=%d param=%u expected=%.9g got=%.9g\n",name,v,p,expected[v].params[p],got.params[p]);
        assert(std::fabs(got.params[p]-expected[v].params[p])<.0002f);
      }
    }
    assert(std::fabs(engine.mainVolume()-.63f)<.0002f);
    assert(std::fabs(engine.getTrackVolume(VoiceId::SynthA)-.71f)<.0002f);
    assert(std::fabs(engine.getTrackVolume(VoiceId::SynthB)-.43f)<.0002f);
    if(std::string(name)=="TB303") {
      const auto path=root/(project+".json");
      std::ifstream input(path);std::string old((std::istreambuf_iterator<char>(input)),{});input.close();
      for(const char* key : {"aCount","bCount"}) {
        const std::string from=std::string("\"")+key+"\":6";
        const auto at=old.find(from);assert(at!=std::string::npos);
        old.replace(at,from.size(),std::string("\"")+key+"\":4");
      }
      std::ofstream output(path);output<<old;output.close();
      engine.setSynthEngine(0,"WAVEMORPH");engine.setSynthEngine(1,"AY");
      assert(engine.loadSceneByName(project));
      for(int v=0;v<2;++v) {
        const auto got=engine.synthVoices_[v].getState();
        for(uint8_t p=0;p<4;++p) assert(std::fabs(got.params[p]-expected[v].params[p])<.0002f);
        assert(got.params[4]==0 && got.params[5]==0);
      }
      assert(std::fabs(engine.mainVolume()-.63f)<.0002f);
      std::puts("DSP-A legacy four-control TB303 JSON: PASS");
    }
    std::printf("DSP-A project Save/Load A+B engine=%s controls+levels: PASS\n",name);
  }
  fs::current_path(previous);fs::remove_all(root);
}
