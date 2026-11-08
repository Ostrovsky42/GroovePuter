#include <bits/stdc++.h>
#include "src/dsp/miniacid_engine.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/ui/pages/song_page.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/state/generation_request_state.h"
SerialMock Serial; SDMock SD;
class NullGfx : public IGfx {
 public:
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char*) override {}
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
  int textWidth(const char* t) const override { return t ? static_cast<int>(std::strlen(t)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

static UIEvent nav(KeyScanCode code,bool shift=false){UIEvent e{};e.event_type=GROOVEPUTER_KEY_DOWN;e.scancode=code;e.shift=shift;return e;}
static UIEvent app(ApplicationEventType code){UIEvent e{};e.event_type=GROOVEPUTER_APPLICATION_EVENT;e.app_event_type=code;return e;}
int main(int argc,char**argv){
 if(argc!=4)return 64;
 std::setvbuf(stdout,nullptr,_IOLBF,0);
 int genre=std::stoi(argv[1]);uint32_t seed=std::stoul(argv[2]);
 auto root=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(root);std::filesystem::current_path(root);SD.setRoot(root);
 SceneStorageSdl storage;const std::string name="g2-case";if(!storage.setCurrentSceneName(name))return 65;
 MiniAcid engine(kSampleRate,&storage);engine.init();if(!engine.createNewSceneWithName(name))return 66;
 engine.setSongMode(false);engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(genre));engine.genreManager().setRecipe(0);engine.modeManager().setGenerationSeed(seed);
 engine.syncGrooveModeToGenre();
 engine.setBpm(genre==10?124:82);
 GroovePuterState::setGenerationLevel(GroovePuterRhythm::RealizationLevel::P3Transformation);
 AudioGuard guard{};
 int first=engine.songLength();auto take=GeneratedPhraseSong::generate(engine,4,first,guard);
 if (take) {
  const auto* recipe=engine.generatedPhraseRecipe();
  GroovePuterRhythm::PhraseExecutionScratch scratch{};
  GroovePuterRhythm::PreparedPhraseExecution execution{};
  auto prepared=GroovePuterRhythm::preparePhraseExecution(recipe->genre,recipe->materialization,recipe->phraseGenerationIdentity,4,scratch,execution);
  auto id=execution.selection.composition.rhythmArchetypeId;
  auto* definition=GroovePuterRhythm::ReferenceVocabulary::definitionForId(id);
  auto develop=GroovePuterRhythm::applyPhraseLawToExecution(execution,GroovePuterRhythm::PhraseEvolutionLawId::DevelopReturn);
  auto sparse=GroovePuterRhythm::applyPhraseLawToExecution(execution,GroovePuterRhythm::PhraseEvolutionLawId::SparseDrift);
  std::printf("G2_ADMISSION prepare=%d archetype=%u enabled=%d develop=%d sparse=%d\n",int(prepared),unsigned(id),definition?GroovePuterRhythm::ReferenceVocabulary::phraseEvolutionEnabled(definition->key):false,int(develop),int(sparse));
 }
 GeneratedPhraseSong::CycleResult cycle{};bool copied=false,pasted=false,returned=false;
 if(take){cycle=GeneratedPhraseSong::generateCycle(engine,guard);}
 int after=engine.songLength();
 if(cycle&&cycle.bars==8&&!cycle.developSkipped&&!cycle.breakSkipped){
  engine.setSongPosition(first);NullGfx gfx;SongPage page(gfx,engine,guard);page.setBoundaries(Rect{0,16,240,93});
  for(int i=0;i<2;++i){auto e=nav(GROOVEPUTER_RIGHT,true);page.handleEvent(e);}
  for(int i=0;i<3;++i){auto e=nav(GROOVEPUTER_DOWN,true);page.handleEvent(e);}
  auto copy=app(GROOVEPUTER_APP_EVENT_COPY);copied=page.handleEvent(copy);
  // COPY locks the whole selection frame: move its top-left anchor to the append row.
  for(int row=first;row<after;++row){auto e=nav(GROOVEPUTER_DOWN);page.handleEvent(e);}
  auto paste=app(GROOVEPUTER_APP_EVENT_PASTE);pasted=page.handleEvent(paste);
  returned=true;for(int bar=0;bar<4;++bar)for(int track=0;track<3;++track)
   returned=returned&&(engine.song().positions[first+bar].patterns[track]==engine.song().positions[after+bar].patterns[track]);
 }

 if(returned){
  engine.setSongMode(true);engine.setLoopMode(false);engine.setSongPosition(first);
  engine.setSynthEngine(0,"TB303");engine.setSynthEngine(1,"TB303");
  for(int voice=0;voice<2;++voice){engine.set303DelayEnabled(voice,false);engine.set303DistortionEnabled(voice,false);}
  engine.start();engine.serviceSongMaterial();
  const size_t frames=static_cast<size_t>(std::llround(16.0*4.0*60.0/engine.bpm()*kSampleRate));
  std::ofstream pcm("preview.pcm",std::ios::binary);std::ofstream trace("playback.csv");trace<<"frame,song_row\n";
  int last=-1;int16_t samples[256];
  for(size_t offset=0;offset<frames;){
   engine.serviceSongMaterial();size_t count=std::min(size_t(256),frames-offset);engine.generateAudioBuffer(samples,count);
   int row=engine.songPlayheadPosition();if(row!=last){trace<<offset<<","<<row<<"\n";last=row;}
   pcm.write(reinterpret_cast<const char*>(samples),count*sizeof(int16_t));offset+=count;
  }
  engine.stop();pcm.close();std::printf("G2_AUDIO frames=%zu last_row=%d sr=%d\n",frames,last,int(kSampleRate));
 }
 bool saved=engine.saveSceneAs(name);
 std::printf("G2_LAYOUT genre=%d seed=%u take=%d take_status=%d first=%d cycle=%d cycle_status=%d develop_skipped=%d break_skipped=%d cycle_bars=%u after_cycle=%d copy=%d paste=%d return_exact=%d total_bars=%d saved=%d\n",genre,seed,bool(take),int(take.status),first,bool(cycle),int(cycle.status),cycle.developSkipped,cycle.breakSkipped,cycle.bars,after,copied,pasted,returned,engine.songLength()-first,saved);
 return 0;
}
