#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/ui/pages/tb303_params_page.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
SerialMock Serial;
SDMock SD;

constexpr int kGlyphAdvance = 6;
class RecordingGfx : public IGfx {
 public:
  struct TextEntry {
    int x;
    int y;
    std::string text;
  };
  std::vector<TextEntry> texts;

  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int x, int y, const char* text) override {
    if (text && text[0] != '\0') texts.push_back({x, y, text});
  }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor) override {}
  void drawCircle(int, int, int, IGfxColor) override {}
  void drawKnobFace(int, int, int, IGfxColor, IGfxColor) override {}
  void fillRect(int, int, int, int, IGfxColor) override {}
  void fillCircle(int, int, int, IGfxColor) override {}
  void drawLine(int32_t, int32_t, int32_t, int32_t, IGfxColor) override {}
  void setRotation(int) override {}
  void setTextColor(IGfxColor) override {}
  void setTextColor(uint16_t) override {}
  void setFont(GfxFont) override {}
  void startWrite() override {}
  void endWrite() override {}
  void flush() override {}
  int textWidth(const char* text) const override {
    return text ? static_cast<int>(std::strlen(text)) * kGlyphAdvance : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

void presetUiTest(MiniAcid& engine) {
  RecordingGfx gfx;
  TB303ParamsPage page(gfx,engine,AudioGuard{},0);
  page.setBoundaries(Rect{0,16,240,93});
  page.showMoreTab(true); page.draw(gfx);
  auto key=[&](int code, char ch=0, bool alt=false) {
    UIEvent e{};e.event_type=GROOVEPUTER_KEY_DOWN;e.scancode=static_cast<KeyScanCode>(code);e.key=ch;e.alt=alt;
    return page.handleEvent(e);
  };
  for(int i=0;i<5;++i) key(GROOVEPUTER_DOWN);
  const int before=engine.soundQualityPresetIndex(0);
  assert(key(0,'\r')); assert(page.preset_popup_.isOpen());
  gfx.texts.clear(); page.draw(gfx);
  for(const auto& t:gfx.texts) if(t.text=="Classic Acid" || t.text=="Deep Bass" || t.text=="Aggressive Acid")
    assert(t.x>=0 && t.x+gfx.textWidth(t.text.c_str())<=240);
  key(GROOVEPUTER_DOWN); key(0,0x1b);
  assert(!page.preset_popup_.isOpen()); assert(engine.soundQualityPresetIndex(0)==before);
  key(0,'\r'); key(GROOVEPUTER_DOWN); key(0,'\r');
  assert(engine.soundQualityPresetIndex(0)==1);
  gfx.texts.clear(); page.draw(gfx);
  bool found=false;for(const auto& t:gfx.texts) if(t.text=="Deep Bass") found=true;
  assert(found);
  engine.set303Parameter(TB303ParamId::Cutoff,1234,0);
  gfx.texts.clear();page.draw(gfx);found=false;
  for(const auto& t:gfx.texts) if(t.text=="CUSTOM") found=true;
  assert(found);
  const auto source=engine.currentSequencedSource(0);
  assert(key(GROOVEPUTER_R,'r',true));
  assert(engine.currentSequencedSource(0)!=source);
  assert(key(GROOVEPUTER_R,'r',true));
  assert(engine.currentSequencedSource(0)==source);
  puts("Preset UI: Enter, cancel, confirm, CUSTOM and Alt+R: PASS");
}

int main() {
  namespace fs=std::filesystem;
  const auto previous=fs::current_path();
  const auto root=fs::temp_directory_path()/"gp-dsp-a-project-roundtrip";
  fs::remove_all(root);fs::create_directories(root);fs::current_path(root);SD.setRoot(root);
  {
    MiniAcid engine(kSampleRate,nullptr); engine.init();
    engine.setSynthEngine(0,"TB303"); engine.setSynthEngine(1,"WAVEMORPH");
    const auto other=engine.synthVoices_[1].getState();
    const auto source=engine.currentSequencedSource(0);
    const float level=engine.getTrackVolume(VoiceId::SynthA);
    for(int i=0;i<3;++i) {
      assert(engine.applySoundQualityPreset(0,i));
      assert(engine.soundQualityPresetIndex(0)==i);
    }
    const auto after=engine.synthVoices_[1].getState();
    for(int i=0;i<other.paramCount;++i) assert(other.params[i]==after.params[i]);
    assert(engine.currentSequencedSource(0)==source);
    assert(engine.getTrackVolume(VoiceId::SynthA)==level);
    engine.set303Parameter(TB303ParamId::Cutoff,1234,0);
    assert(engine.soundQualityPresetIndex(0)==-1);
    for(int i=0;i<3;++i) {
      assert(engine.applySoundQualityPreset(1,i));
      assert(engine.soundQualityPresetIndex(1)==i);
    }
    engine.adjustSynthParameter(1,0,1);
    assert(engine.soundQualityPresetIndex(1)==-1);
    assert(!engine.applySoundQualityPreset(1,3));
    engine.setSynthEngine(1,"SID");
    assert(!engine.applySoundQualityPreset(1,0));
    assert(engine.soundQualityPresetIndex(1)==-1);
    puts("DSP preset application, CUSTOM and voice isolation: PASS");
    presetUiTest(engine);
  }
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
    if(std::string(name)=="TB303" || std::string(name)=="WAVEMORPH") {
      assert(engine.applySoundQualityPreset(0,0));
      assert(engine.saveSceneToStorage());
      assert(engine.applySoundQualityPreset(0,2));
      assert(engine.loadSceneByName(project));
      assert(engine.soundQualityPresetIndex(0)==0);
    }
    std::printf("DSP-A project Save/Load A+B engine=%s controls+levels: PASS\n",name);
  }
  fs::current_path(previous);fs::remove_all(root);
}
