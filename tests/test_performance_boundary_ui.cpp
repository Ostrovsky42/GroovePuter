// PROJECT -> MIDI: the USB role row. L/R previews, ENTER saves (the running role does not change),
// a second ENTER restarts; a project with unsaved changes is never restarted from here.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/platform/cardputer_usb_role_runtime.h"
#include "src/state/scene_revision.h"
#include "src/ui/pages/genre_page.h"
#include "src/ui/pages/tb303_params_page.h"
#include "src/ui/miniacid_display.h"
#include "src/input/performance_keyboard.h"
#include "src/input/internal_synth_output.h"
#include "src/generation/migration/genre_harmony_selection.h"
#include "src/output/output_ownership.h"
#include "src/ui/workflow_mode.h"
#include <vector>
#include <string>
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

namespace {
class NullGfx : public IGfx {
 public:
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  std::vector<std::string> text;
  void drawText(int, int, const char* s) override { if(s) text.emplace_back(s); }
  bool has(const char* s) const { for(const auto& t:text) if(t.find(s)!=std::string::npos) return true; return false; }
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
  int flushes = 0;
  bool overlayAtFlush = false;
  void flush() override {
    ++flushes;
    overlayAtFlush = !text.empty() && text.back() == "DIAG TEST";
  }
  int textWidth(const char* t) const override { return t ? static_cast<int>(std::strlen(t)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

UIEvent key(char k) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = k;
  return e;
}
UIEvent scan(KeyScanCode c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = c;
  return e;
}
NullGfx* overlayGfx = nullptr;
void diagnosticOverlay() {
  overlayGfx->drawText(0, 55, "DIAG TEST");
}
}  // namespace

int main() {
  MiniAcid engine(22050.0f, nullptr);
  engine.init();
  NullGfx gfx;
  auto& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = 0;
  scene.genre.recipe = 7;
  scene.genre.regenerateOnApply = false;
  scene.generatorParams.scale = CHROMATIC;
  scene.generatorParams.scaleRoot = 0;
  GenrePage page(gfx, engine, AudioGuard{});
  for(int i=0;i<4;++i) { auto e=scan(GROOVEPUTER_DOWN); assert(page.handleEvent(e)); }
  page.draw(gfx);
  assert(gfx.has("KEY C CHR"));
  assert(gfx.has("GENRE"));
  auto m=key('m'); assert(page.handleEvent(m));
  assert(scene.generatorParams.scale == MINOR);
  gfx.text.clear(); UI::drawToast(gfx); assert(gfx.has("KEY C MIN"));
  gfx.text.clear(); page.draw(gfx);
  assert(gfx.has("KEY C MIN"));
  auto right=scan(GROOVEPUTER_RIGHT); assert(page.handleEvent(right));
  gfx.text.clear(); page.draw(gfx); assert(gfx.has("KEY C MIN / AUTO"));
  auto enter=key('\n'); assert(page.handleEvent(enter));
  assert(scene.genre.harmonicStrategy == 1);
  auto tab=key('\t'); assert(page.handleEvent(tab));
  assert(page.handleEvent(m));
  assert(scene.generatorParams.scale == MINOR);
  TB303ParamsPage sound(gfx, engine, AudioGuard{}, 1);
  auto reset=key('a'); reset.ctrl=true; assert(sound.handleEvent(reset));
  gfx.text.clear(); UI::drawToast(gfx);
  assert(gfx.has("B FILTER 800Hz"));
  engine.setSynthEngine(1, "SID");
  assert(engine.currentSynthEngineName(1) == "SID");
  assert(sound.handleEvent(reset));
  gfx.text.clear(); UI::drawToast(gfx);
  assert(gfx.has("FILTER RESET: TB303 ONLY"));
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  overlayGfx = &gfx;
  gfx.flushes = 0;
  for (int frame = 0; frame < 2; ++frame) {
    gfx.text.clear();
    display.update(diagnosticOverlay);
    assert(gfx.overlayAtFlush);
    assert(gfx.flushes == frame + 1);
  }
  gfx.text.clear();
  display.update();
  assert(!gfx.has("DIAG TEST") && !gfx.overlayAtFlush && gfx.flushes == 3);
  AudioMutationGate gate;
  InternalSynthOutput internal(engine, gate);
  assert(router.addSink(internal));
  GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA,
                            GroovePuterOutput::Mode::Internal);
  engine.setDeviceMasterVolume(0.5f);
  engine.setTrackVolume(VoiceId::SynthA, 1.0f);
  engine.setMute303(0, false);
  display.goToPage(WorkflowPages::kPerform);
  display.update();
  auto note=key('a'); assert(display.handleEvent(note));
  assert(keyboard.heldCount() == 1 && engine.liveNote(0) >= 0);
  int16_t audio[1024]{};
  engine.generateAudioBuffer(audio, 1024);
  int peak = 0;
  for (int16_t sample : audio) {
    const int magnitude = sample < 0 ? -static_cast<int>(sample) : sample;
    if (magnitude > peak) peak = magnitude;
  }
  std::printf("PERFORM INTERNAL MONO STOP: PCM peak=%d live=%d\n", peak, engine.liveNote(0));
  assert(peak > 32);
  assert(keyboard.keyUp('a') && engine.liveNote(0) == -1);
  // Exercise real internal chord projection and DSP release, rather than
  // merely proving that the MIDI sink balances its events.
  for (int voice = 0; voice < 2; ++voice) {
    engine.setSynthEngine(voice, "TB303");
    engine.setMute303(voice, false);
    engine.setTrackVolume(voice == 0 ? VoiceId::SynthA : VoiceId::SynthB, 1.0f);
    GroovePuterOutput::setMode(voice == 0 ? GroovePuterOutput::Track::SynthA
                                        : GroovePuterOutput::Track::SynthB,
                              GroovePuterOutput::Mode::Internal);
    keyboard.setTarget(voice == 0 ? MusicalEventTarget::SynthA
                                  : MusicalEventTarget::SynthB);
    for (PerformanceVoiceMode mode : {PerformanceVoiceMode::Mono,
                                      PerformanceVoiceMode::Poly}) {
      for (bool switchWhileHeld : {false, true}) {
        keyboard.panic();
        keyboard.setVoiceMode(mode);
        keyboard.setChordMode(PerformanceChordMode::Off);
        if (!switchWhileHeld) keyboard.setChordMode(PerformanceChordMode::Major);
        assert(keyboard.keyDown('a'));
        if (switchWhileHeld) keyboard.setChordMode(PerformanceChordMode::Major);
        assert(keyboard.keyDown('s'));
        assert(engine.liveNote(voice) >= 0);
        engine.generateAudioBuffer(audio, 1024);
        assert(keyboard.keyUp('s'));
        assert(keyboard.keyUp('a'));
        assert(keyboard.heldCount() == 0 && engine.liveNote(voice) == -1);
        // Let both voices and the default FX tails settle for about 3 seconds.
        for (int block = 0; block < 256; ++block)
          engine.generateAudioBuffer(audio, 1024);
        int releasedPeak = 0;
        for (int16_t sample : audio) {
          const int magnitude = sample < 0 ? -static_cast<int>(sample) : sample;
          if (magnitude > releasedPeak) releasedPeak = magnitude;
        }
        std::printf("CHORD INTERNAL %c mode=%d switchHeld=%d releasePeak=%d live=%d\n",
                    'A' + voice, static_cast<int>(mode), switchWhileHeld,
                    releasedPeak, engine.liveNote(voice));
        assert(releasedPeak <= 4);
      }
    }
  }
  // Reproduce EUCLID ON -> OFF while stopped, then play and release a
  // direct chord. Continue clock and audio after release to expose delayed
  // attacks or an internal gate left high despite liveNote == -1.
  uint32_t now = 0;
  for (PerformanceVoiceMode mode : {PerformanceVoiceMode::Mono,
                                    PerformanceVoiceMode::Poly}) {
    for (bool disableWhileHeld : {false, true}) {
      keyboard.panic();
      keyboard.setTarget(MusicalEventTarget::SynthA);
      keyboard.setVoiceMode(mode);
      keyboard.setChordMode(PerformanceChordMode::Major);
      keyboard.setLatchEnabled(false);
      keyboard.setArpeggiatorEnabled(false);
      while (keyboard.euclideanPulses() != 0) keyboard.cycleEuclideanPulses(-1);
      keyboard.cycleEuclideanPulses(1);
      assert(keyboard.keyDown('a'));
      for (int block = 0; block < 32; ++block) {
        now += 10000; keyboard.service(now);
        engine.generateAudioBuffer(audio, 1024);
      }
      if (!disableWhileHeld) assert(keyboard.keyUp('a'));
      keyboard.cycleEuclideanPulses(-1);
      assert(keyboard.euclideanPulses() == 0);
      if (!disableWhileHeld) assert(keyboard.keyDown('a'));
      for (int block = 0; block < 32; ++block) {
        now += 10000; keyboard.service(now);
        engine.generateAudioBuffer(audio, 1024);
      }
      assert(keyboard.keyUp('a'));
      for (int block = 0; block < 256; ++block) {
        now += 10000; keyboard.service(now);
        engine.generateAudioBuffer(audio, 1024);
      }
      int releasePeak = 0;
      for (int16_t sample : audio)
        releasePeak = std::max(releasePeak, std::abs(static_cast<int>(sample)));
      std::printf("EUCLID OFF mode=%d disableHeld=%d h=%zu live=%d peak=%d\n",
                  static_cast<int>(mode), disableWhileHeld, keyboard.heldCount(),
                  engine.liveNote(0), releasePeak);
      assert(keyboard.heldCount() == 0 && engine.liveNote(0) == -1);
      assert(releasePeak <= 4);
    }
  }
  static_assert(sizeof(GenreSettings) == 12, "AUTO must use existing padding");
  // Resolver honors actual catalog/capability bounds and old explicit source.
  GenreSettings autoSettings{};
  assert(!autoSettings.harmonicAuto && autoSettings.harmonicStrategy == 0);
  autoSettings.harmonicAuto = true;
  assert(GroovePuterRhythm::resolveGenreHarmony(autoSettings, 0).settings.harmonicStrategy == 1);
  autoSettings.generativeMode = 2;
  assert(GroovePuterRhythm::resolveGenreHarmony(autoSettings, 0).settings.harmonicStrategy == 0);
  autoSettings.generativeMode = 0;
  assert(GroovePuterRhythm::resolveGenreHarmony(autoSettings, 9).settings.harmonicStrategy == 0);
  autoSettings.harmonicAuto = false;
  assert(GroovePuterRhythm::resolveGenreHarmony(autoSettings, 0).settings.harmonicStrategy == 0);
  autoSettings.harmonicStrategy = 1;
  assert(GroovePuterRhythm::resolveGenreHarmony(autoSettings, 9).settings.harmonicStrategy == 1);

  // AUTO is a real, persisted next-Phrase preference. Existing Legacy0 stays
  // default; unsupported scale falls back to genre generation, not whole-Harmony N/A.
  auto& autoScene = engine.sceneManager().currentScene();
  autoScene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  autoScene.genre.recipe = 0;
  autoScene.genre.harmonicStrategy = 0;
  autoScene.genre.harmonicAuto = false;
  autoScene.genre.regenerateOnApply = false;
  autoScene.generatorParams.scale = MINOR;
  engine.genreManager().setGenerativeMode(GenerativeMode::LoFi);
  engine.genreManager().setRecipe(0);
  GenrePage autoPage(gfx, engine, AudioGuard{});
  UI::UiShellFrameModel shellFrame{};
  for (int i = 0; i < 4; ++i) { auto down = scan(GROOVEPUTER_DOWN); assert(autoPage.handleEvent(down)); }
  auto autoRight = scan(GROOVEPUTER_RIGHT);
  assert(autoPage.handleEvent(autoRight));
  UI::beginShellFrameModel(shellFrame);
  gfx.text.clear(); autoPage.draw(gfx);
  UI::endShellFrameModel();
  assert(gfx.has("KEY C MIN / AUTO"));
  auto apply = key('\n'); assert(autoPage.handleEvent(apply));
  const auto& appliedScene = engine.sceneManager().currentScene();
  assert(appliedScene.genre.harmonicStrategy == 1);
  const auto appliedAuto = GroovePuterRhythm::resolveGenreHarmony(
      appliedScene.genre, static_cast<uint8_t>(appliedScene.generatorParams.scale));
  assert(appliedAuto.settings.harmonicAuto &&
         appliedAuto.settings.harmonicStrategy == 1);
  std::string savedJson;
  assert(engine.sceneManager().writeSceneJson(savedJson));
  assert(savedJson.find("\"hma\":true") != std::string::npos);
  SceneManager domLoaded;
  assert(domLoaded.loadSceneJson(savedJson));
  assert(domLoaded.currentScene().genre.harmonicAuto &&
         domLoaded.currentScene().genre.harmonicStrategy == 1);
  struct JsonReader {
    const std::string& text;
    size_t position = 0;
    int read() { return position < text.size() ? static_cast<unsigned char>(text[position++]) : -1; }
  } eventedReader{savedJson};
  SceneManager evented;
  assert(evented.loadSceneEvented(eventedReader));
  assert(evented.currentScene().genre.harmonicAuto &&
         evented.currentScene().genre.harmonicStrategy == 1);

  engine.sceneManager().currentScene().generatorParams.scale = CHROMATIC;
  UI::beginShellFrameModel(shellFrame);
  gfx.text.clear(); autoPage.draw(gfx);
  UI::endShellFrameModel();
  assert(gfx.has("KEY C CHR / AUTO") && !gfx.has("N/A:") &&
         std::string(shellFrame.footer.right) == "AUTO>GENRE SCALE M:SCALE");
  assert(autoPage.handleEvent(apply));
  assert(engine.sceneManager().currentScene().genre.harmonicStrategy == 0 &&
         engine.sceneManager().currentScene().genre.harmonicAuto);
  // Explicit GRAPH is still refused on unsupported scale, with a specific reason.
  assert(autoPage.handleEvent(autoRight));
  gfx.text.clear(); UI::drawToast(gfx); assert(gfx.has("GRAPH: SCALE"));
  // Unknown/missing persisted Auto preference defaults to ordinary Legacy.
  std::string legacyJson = savedJson;
  const size_t autoField = legacyJson.find(",\"hma\":true");
  assert(autoField != std::string::npos);
  legacyJson.erase(autoField, std::strlen(",\"hma\":true"));
  const size_t graphStrategy = legacyJson.find("\"hsm\":1");
  assert(graphStrategy != std::string::npos);
  legacyJson.replace(graphStrategy, std::strlen("\"hsm\":1"), "\"hsm\":0");
  assert(engine.sceneManager().loadSceneJson(legacyJson));
  assert(engine.sceneManager().currentScene().genre.harmonicStrategy == 0 &&
         !engine.sceneManager().currentScene().genre.harmonicAuto);
  std::puts("GENRE project-scale and SOUND action feedback: PASS");
}
