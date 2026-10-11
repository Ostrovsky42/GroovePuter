// Listening render (analysis): G on MATERIAL -> 4-bar TAKE, looped twice to WAV (8 bars).
// Outrun, Darksynth and Rave (control), presses 1..3 with the same seeds as idiomprobe.
// Usage: takerender <out-dir> <tag> [lofi]   (lofi: LoFi at 72 BPM instead of the default three)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "src/dsp/miniacid_engine.h"
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
  std::vector<std::string> texts;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* s) override { if (s && *s) texts.push_back(s); }
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

static const char* kGenre[] = {"Acid", "Outrun", "Darksynth", "Electro", "Rave",
                               "Reggae", "TripHop", "Broken", "Chip", "House",
                               "Techno", "HipHop", "FunkSoul", "UkGarage", "DnB", "LoFi"};

static std::string nm(int n) {
  static const char* k[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (n < 0) return ".";
  return std::string(k[n % 12]) + std::to_string(n / 12 - 1);
}

static UIEvent key(char c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = c;
  return e;
}

// Steps of the pattern a Song cell points at (Pattern slots only).
static const SynthPattern* cellPattern(Scene& scene, int track, int cell) {
  if (cell < 0) return nullptr;
  const int bank = songPatternBank(cell);
  const int idx = songPatternIndexInBank(cell);
  if (bank < 0 || idx < 0) return nullptr;
  return track == 0 ? &scene.synthABanks[bank].patterns[idx]
                    : &scene.synthBBanks[bank].patterns[idx];
}


static void emit(const char* genre, int press, int reqBars, Scene& scene, MiniAcid& engine, int from, int to) {
  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  for (int track = 0; track < 2; ++track) {
    for (int row = from; row < to; ++row) {
      const int cell = scene.songs[songSlot].positions[row].patterns[track];
      const SynthPattern* p = cellPattern(scene, track, cell);
      std::printf("%s\t%d\t%s\t%d\t%d\t%d", genre, press, track ? "B" : "A", row - from, reqBars, to - from);
      for (int i = 0; i < 16; ++i) {
        if (!p || p->steps[i].note < 0) { std::printf("\t."); continue; }
        std::printf("\t%d%s%s", p->steps[i].note, p->steps[i].slide ? "~" : "", p->steps[i].accent ? "!" : "");
      }
      std::printf("\n");
    }
  }
}

static void writeWav(const std::string& path, const std::vector<int16_t>& pcm, uint32_t rate) {
  FILE* f = std::fopen(path.c_str(), "wb");
  const uint32_t data = static_cast<uint32_t>(pcm.size() * 2);
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
  u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
  std::fwrite("data", 1, 4, f); u32(data);
  std::fwrite(pcm.data(), 2, pcm.size(), f);
  std::fclose(f);
}

int main(int argc, char** argv) {
  const std::string out = std::filesystem::absolute(argc > 1 ? argv[1] : ".").string();
  const std::string tag = argc > 2 ? argv[2] : "x";
  struct G { int mode; const char* name; float bpm; };
  const std::vector<G> genres = argc > 3 && std::string(argv[3]) == "lofi"
      ? std::vector<G>{{15, "lofi", 72.0f}}
      : std::vector<G>{{1, "outrun", 108.0f}, {2, "darksynth", 120.0f}, {4, "rave", 136.0f}};
  for (const auto& gg : genres) {
    const auto root = std::filesystem::temp_directory_path() / ("gp_takerender_" + tag + std::to_string(gg.mode));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    SD.setRoot(root);
    GroovePuterPlatform::clearMaterialPublication("takerender", 0);
    PatternPagingService::setProjectName("takerender");
    SceneStorageSdl storage;
    storage.setCurrentSceneName("default");
    MiniAcid engine(static_cast<float>(kSampleRate), &storage);
    engine.init();
    engine.setSongMode(false);
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(gg.mode));
    engine.genreManager().setRecipe(0);
    engine.setBpm(gg.bpm);
    auto& scene = engine.sceneManager().currentScene();
    NullGfx gfx;
    MusicalEventRouter router;
    PerformanceKeyboard keyboard(router);
    MiniAcidDisplay display(gfx, engine, keyboard);
    display.dismissSplash();
    display.goToPage(WorkflowPages::kPhrase);
    display.update();
    const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
    const int base = scene.songs[songSlot].length;
    for (int press = 0; press < 3; ++press) {
      GroovePuterState::setRequestedPhraseBars(4);
      scene.songs[songSlot].length = base;
      display.update();
      const int before = scene.songs[songSlot].length;
      auto g = key('g');
      display.handleEvent(g);
      for (int i = 0; i < 3; ++i) display.update();
      int after = scene.songs[songSlot].length;
      if (after == before) {  // no room: R, Enter, G (as the user would)
        auto r = key('r'); display.handleEvent(r); display.update();
        auto en = key('\n'); display.handleEvent(en); display.update();
        display.handleEvent(g);
        for (int i = 0; i < 3; ++i) display.update();
        after = scene.songs[songSlot].length;
      }
      if (after == before) { std::printf("# %s press %d: not generated\n", gg.name, press); continue; }
      for (int track = 0; track < 2; ++track)
        for (int row = before; row < after; ++row) {
          const SynthPattern* p = cellPattern(scene, track, scene.songs[songSlot].positions[row].patterns[track]);
          std::printf("%s %s p%d %s bar%d:", tag.c_str(), gg.name, press + 1, track ? "B" : "A", row - before);
          for (int i = 0; i < 16; ++i)
            std::printf(" %s%s", p ? nm(p->steps[i].note).c_str() : ".", p && p->steps[i].slide ? "~" : "");
          std::printf("\n");
        }
      engine.setSongMode(true);
      engine.setLoopMode(true);
      engine.setLoopRange(before, after - 1);
      engine.setSongPosition(before);
      engine.start();
      const double seconds = 8 * 4 * 60.0 / gg.bpm;
      std::vector<int16_t> pcm(static_cast<size_t>(seconds * kSampleRate));
      std::set<int> seen;
      for (size_t at = 0; at < pcm.size(); at += 512) {
        const size_t n = std::min<size_t>(512, pcm.size() - at);
        engine.generateAudioBuffer(pcm.data() + at, n);
        seen.insert(engine.currentSongPosition());
      }
      engine.stop();
      engine.setSongMode(false);
      engine.setLoopMode(false);
      std::printf("# %s p%d rows %d..%d played:", gg.name, press + 1, before, after - 1);
      for (int s : seen) std::printf(" %d", s);
      std::printf("\n");
      writeWav(out + "/" + tag + "_" + gg.name + "_p" + std::to_string(press + 1) + ".wav", pcm, kSampleRate);
      UIEvent z = key('z');
      z.ctrl = true;
      display.handleEvent(z);
      for (int i = 0; i < 3; ++i) display.update();
    }
  }
  return 0;
}
