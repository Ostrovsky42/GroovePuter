#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "src/dsp/miniacid_engine.h"
#include "src/dsp/dual_osc_presets.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/ui/pages/tb303_params_page.h"

SerialMock Serial;
SDMock SD;

class RecordingGfx : public IGfx {
 public:
  std::vector<std::string> texts;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* text) override { if (text) texts.emplace_back(text); }
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


static UIEvent character(char key) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = key;
  return event;
}
static UIEvent navigation(KeyScanCode code) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.scancode = code;
  return event;
}
static bool drawn(const RecordingGfx& gfx, const char* label) {
  for (const auto& text : gfx.texts) if (text == label) return true;
  return false;
}

int main() {
  namespace fs = std::filesystem;
  const auto previous = fs::current_path();
  const auto root = fs::temp_directory_path() / ("gp-dual-osc-ui-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  fs::current_path(root);
  SD.setRoot(root);
  {
    SceneStorageSdl storage;
    assert(storage.setCurrentSceneName("preset-integration"));
    MiniAcid engine(22050.0f, &storage);
    engine.init();
    engine.setSongMode(false);
    for (int voice = 0; voice < 2; ++voice) {
      engine.setSynthEngine(voice, "DUAL OSC");
      assert(engine.currentSynthEngineName(voice) == "DUAL OSC");
      engine.adjustSynthParameter(voice, 0, -100);
      RecordingGfx gfx;
      TB303ParamsPage page(gfx, engine, AudioGuard{}, voice);
      page.setBoundaries(Rect{0, 16, 240, 93});
      auto draw = [&]() { gfx.texts.clear(); page.draw(gfx); };
      auto send = [&](UIEvent event) { assert(page.handleEvent(event)); };
      auto preset = [&]() { return engine.synthParameter(voice, 0).optionIndex(); };
      draw();
      // TYPE uses the same modal interaction, while L/R remains a stepper.
      page.showMoreTab(true);
      draw();
      const auto revisionBeforeType = GroovePuterState::sceneRevisionSnapshot();
      auto enterType = character('\r');
      (void)page.handleEvent(enterType);
      draw();
      assert(drawn(gfx, "SYNTH TYPE"));
      send(navigation(GROOVEPUTER_DOWN));  // DUAL OSC -> first item, TB303.
      const char* types[] = {"TB303", "SID", "AY", "SH101", "SN76489", "WAVEMORPH", "DUAL OSC"};
      for (int index = 0; index < 7; ++index) {
        draw();
        assert(drawn(gfx, types[index]));
        assert(engine.currentSynthEngineName(voice) == "DUAL OSC");
        if (index < 6) send(navigation(GROOVEPUTER_DOWN));
      }
      send(character(0x1b));
      assert(engine.currentSynthEngineName(voice) == "DUAL OSC");
      assert(GroovePuterState::sceneRevisionSnapshot().currentRevision == revisionBeforeType.currentRevision);
      const auto otherEngine = engine.currentSynthEngineName(1 - voice);
      send(character('\r'));
      send(navigation(GROOVEPUTER_DOWN));
      send(character('\r'));
      assert(engine.currentSynthEngineName(voice) == "TB303");
      assert(engine.currentSynthEngineName(1 - voice) == otherEngine);
      draw();
      send(navigation(GROOVEPUTER_LEFT));
      assert(engine.currentSynthEngineName(voice) == "DUAL OSC");
      send(navigation(GROOVEPUTER_RIGHT));
      assert(engine.currentSynthEngineName(voice) == "TB303");
      send(character('\r'));
      send(navigation(GROOVEPUTER_UP));
      send(character('\r'));
      assert(engine.currentSynthEngineName(voice) == "DUAL OSC");
      page.showMoreTab(false);
      draw();
      send(character('\r'));
      for (int index = 0; index < 13; ++index) {
        draw();
        assert(drawn(gfx, "DUAL OSC PRESET"));
        assert(drawn(gfx, kDualOscPresetNames[index]));
        assert(preset() == 0);  // Navigation previews until Enter.
        if (index < 12) send(navigation(GROOVEPUTER_DOWN));
      }
      send(character('\r'));
      assert(preset() == 12);
      send(character('\r'));
      send(navigation(GROOVEPUTER_DOWN));
      send(character(0x1b));
      assert(preset() == 12);  // Cancel preserves the applied preset.

      engine.adjustSynthParameter(voice, 0, -100);
      send(character('a'));
      std::printf("DUAL_OSC_UI voice=%d after_a=%d expected=1\n", voice, preset());
      std::fflush(stdout);
      assert(preset() == 1);
      send(character('z'));
      assert(preset() == 0);
      engine.adjustSynthParameter(voice, 0, 6);
      send(character('z'));
      assert(preset() == 5);
      send(character('a'));
      assert(preset() == 6);
      engine.adjustSynthParameter(voice, 0, -100);
      send(navigation(GROOVEPUTER_UP));
      assert(preset() == 1);
      send(navigation(GROOVEPUTER_DOWN));
      assert(preset() == 0);

      page.showMoreTab(true);
      draw();
      for (int row = 0; row < 5; ++row) send(navigation(GROOVEPUTER_DOWN));
      send(navigation(GROOVEPUTER_LEFT));
      assert(preset() == 12);
      send(navigation(GROOVEPUTER_RIGHT));
      assert(preset() == 0);
      send(character('\r'));
      draw();
      assert(drawn(gfx, "DUAL OSC PRESET"));
      for (int index = 0; index < 12; ++index) send(navigation(GROOVEPUTER_DOWN));
      send(character('\r'));
      assert(preset() == 12);

      std::vector<float> parameters;
      for (uint8_t parameter = 0; parameter < engine.synthParameterCount(voice); ++parameter)
        parameters.push_back(engine.synthParameter(voice, parameter).normalized());
      assert(engine.saveSceneAs("preset-integration"));
      engine.setSynthEngine(voice, "TB303");
      assert(engine.loadSceneByName("preset-integration"));
      assert(engine.currentSynthEngineName(voice) == "DUAL OSC");
      assert(preset() == 12);
      for (uint8_t parameter = 0; parameter < parameters.size(); ++parameter)
        assert(std::fabs(engine.synthParameter(voice, parameter).normalized() - parameters[parameter]) < 0.0002f);
    }
  }
  fs::current_path(previous);
  fs::remove_all(root);
  std::puts("DUAL OSC: both-voice popup13, cancel, main/MORE controls, Save/Load: PASS");
}
