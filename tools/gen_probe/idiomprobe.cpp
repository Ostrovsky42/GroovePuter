// Corpus-comparison probe: G on MATERIAL, N presses per genre, requested TAKE length 8B
// (falls back to 4B when the genre does not admit an 8-bar phrase law).
// Usage: idiomprobe [presses=10] [all] [bars=8]   (TSV on stdout, diagnostics "#" lines)
// Row: genre press voice bar requestedBars gotBars step0..15 ("note" or "note~" slide/tie, "." rest, "!" accent suffix)
// Consumed by tools/gen_probe/idiom_metrics.py
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

int main(int argc, char** argv) {
  const int presses = argc > 1 ? std::atoi(argv[1]) : 10;
  const bool all = argc > 2 && std::string(argv[2]) == "all";
  const int wantBars = argc > 3 ? std::atoi(argv[3]) : 8;
  const int sel[] = {9, 1, 2, 12, 10, 4};  // House Outrun Darksynth FunkSoul Techno Rave
  std::printf("# genre\tpress\tvoice\tbar\treqBars\tgotBars\tstep0..15\n");
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    if (!all && std::find(std::begin(sel), std::end(sel), mode) == std::end(sel)) continue;
    const auto root = std::filesystem::temp_directory_path() / ("gp_idiomprobe_" + std::to_string(mode));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    SD.setRoot(root);
    GroovePuterPlatform::clearMaterialPublication("idiomprobe", 0);
    PatternPagingService::setProjectName("idiomprobe");
    SceneStorageSdl storage;
    storage.setCurrentSceneName("default");
    MiniAcid engine(44100, &storage);
    engine.init();
    engine.setSongMode(false);
    engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
    engine.genreManager().setRecipe(0);
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
    int req = wantBars;
    for (int press = 0; press < presses; ++press) {
      gfx.texts.clear();
      GroovePuterState::setRequestedPhraseBars(static_cast<uint8_t>(req));
      scene.songs[songSlot].length = base;
      display.update();
      const int before = scene.songs[songSlot].length;
      auto g = key('g');
      display.handleEvent(g);
      for (int i = 0; i < 3; ++i) display.update();
      int after = scene.songs[songSlot].length;
      if (after == before && press > 0) {
        std::printf("# %s press %d screen:", kGenre[mode], press);
        for (const auto& t : gfx.texts) std::printf(" | %s", t.c_str());
        std::printf("\n");
        gfx.texts.clear();
      }
      if (after == before) {  // no room: R, Enter, G (as the user would)
        auto r = key('r'); display.handleEvent(r); display.update();
        auto en = key('\n'); display.handleEvent(en); display.update();
        display.handleEvent(g);
        for (int i = 0; i < 3; ++i) display.update();
        after = scene.songs[songSlot].length;
      }
      if (after == before && req > 4) {
        std::printf("# %s press %d: %dB refused, trying 4B\n", kGenre[mode], press, req);
        req = 4;
        --press;
        continue;
      }
      if (after == before) { std::printf("# %s press %d: not generated even at %dB\n", kGenre[mode], press, req); continue; }
      emit(kGenre[mode], press, req, scene, engine, before, after);
      {  // Ctrl+Z removes the TAKE in one step, as a user would
        UIEvent z = key('z');
        z.ctrl = true;
        display.handleEvent(z);
        for (int i = 0; i < 3; ++i) display.update();
        if (scene.songs[songSlot].length != before)
          std::printf("# %s press %d: undo left length %d (was %d)\n", kGenre[mode], press,
                      scene.songs[songSlot].length, before);
      }
    }
  }
  return 0;
}
