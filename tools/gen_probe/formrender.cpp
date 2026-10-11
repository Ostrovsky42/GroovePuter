// Listening render for SONG -> FORM. Produces long BUILD/DROP WAVs from one
// generated TAKE per genre so the energy-curve arrangement can be auditioned.
// Usage: formrender <out-dir>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
#include "src/state/energy_curve.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/input/performance_keyboard.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/miniacid_display.h"
#include "src/ui/workflow_mode.h"
#include "src/state/generation_request_state.h"
#include "src/state/phrase_generation_request_state.h"
#include "src/audio/audio_config.h"

SerialMock Serial;
SDMock SD;

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
  int textWidth(const char* s) const override { return s ? std::strlen(s) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

static UIEvent key(char c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = c;
  return e;
}

static void writeWav(const std::string& path, const std::vector<int16_t>& pcm) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  const uint32_t bytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
  auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
  w32(16); w16(1); w16(1); w32(kSampleRate); w32(kSampleRate * 2); w16(2); w16(16);
  std::fwrite("data", 1, 4, f); w32(bytes);
  std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), f);
  std::fclose(f);
}

int main(int argc, char** argv) {
  const std::filesystem::path output = std::filesystem::absolute(argc > 1 ? argv[1] : ".");
  std::filesystem::create_directories(output);
  struct Genre { int mode; const char* name; float bpm; };
  const Genre genres[] = {{4, "rave", 136.0f}, {1, "synthwave", 108.0f}, {15, "lofi", 72.0f}};
  const EnergyCurve::Preset presets[] = {EnergyCurve::Preset::Build, EnergyCurve::Preset::Drop};

  for (const Genre& genre : genres) {
    const std::string project = std::string("formrender-") + genre.name;
    const auto root = std::filesystem::temp_directory_path() / project;
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    SD.setRoot(root);
    GroovePuterPlatform::clearMaterialPublication(project.c_str(), 0);
    PatternPagingService::setProjectName(project.c_str());
    SceneStorageSdl storage;
    storage.setCurrentSceneName("default");
    MiniAcid engine(static_cast<float>(kSampleRate), &storage);
    engine.init();
    engine.setSongMode(false);
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(genre.mode));
    engine.genreManager().setRecipe(0);
    engine.setBpm(genre.bpm);

    auto& scene = engine.sceneManager().currentScene();
    NullGfx gfx;
    MusicalEventRouter router;
    PerformanceKeyboard keyboard(router);
    MiniAcidDisplay display(gfx, engine, keyboard);
    display.dismissSplash();
    display.goToPage(WorkflowPages::kPhrase);
    display.update();
    GroovePuterState::setRequestedPhraseBars(4);
    const int slot = std::clamp(scene.activeSongSlot, 0, 1);
    const int sourceFirst = scene.songs[slot].length;
    auto generate = key('g');
    display.handleEvent(generate);
    for (int i = 0; i < 3; ++i) display.update();
    const int sourceEnd = scene.songs[slot].length;
    const int sourceCount = sourceEnd - sourceFirst;
    if (sourceCount < 1 || sourceCount > EnergyCurve::kMaxSourceRows) {
      std::printf("SKIP %s: TAKE rows=%d\n", project.c_str(), sourceCount);
      continue;
    }

    const Song sourceSong = scene.songs[slot];
    for (const EnergyCurve::Preset preset : presets) {
      scene.songs[slot] = sourceSong;
      EnergyCurve::Curve curve = EnergyCurve::presetCurve(preset);
      for (uint8_t i = 0; i < curve.count; ++i) curve.sections[i].bars = 8;
      const EnergyCurve::Status status = EnergyCurve::apply(
          scene.songs[slot], curve, sourceFirst, sourceCount, sourceEnd);
      if (status != EnergyCurve::Status::Ok) {
        std::printf("SKIP %s %s: FORM %s\n", project.c_str(),
                    EnergyCurve::presetName(preset), EnergyCurve::statusText(status));
        continue;
      }

      engine.rebuildPatternRuntimeEventBank();
      engine.setSongMode(true);
      engine.setSongPlaybackSlot(slot);
      engine.setSongPosition(sourceEnd);
      engine.start();
      const size_t bars = EnergyCurve::totalBars(curve);
      const size_t samples = static_cast<size_t>(
          (bars * 4.0 * 60.0 / genre.bpm + 1.5) * kSampleRate);
      std::vector<int16_t> pcm(samples, 0);
      for (size_t at = 0; at < samples; at += 256) {
        const size_t count = std::min<size_t>(256, samples - at);
        engine.generateAudioBuffer(pcm.data() + at, count);
      }
      engine.stop();
      engine.setSongMode(false);
      const std::string file = (output / (project + EnergyCurve::presetName(preset) + ".wav")).string();
      writeWav(file, pcm);
      std::printf("WROTE %s bars=%zu bpm=%.0f seconds=%.1f rows=%d..%d\n",
                  file.c_str(), bars, genre.bpm,
                  static_cast<double>(samples) / kSampleRate,
                  sourceEnd, sourceEnd + static_cast<int>(bars) - 1);
    }
  }
  return 0;
}
