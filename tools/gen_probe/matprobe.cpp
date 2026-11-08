// MATERIAL path probe (analysis): G -> TAKE in Song rows, then D -> develop.
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

static void report(const char* genre, const char* phase, MiniAcid& engine, int from, int to,
                   bool print) {
  auto& scene = engine.sceneManager().currentScene();
  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  for (int track = 0; track < 2; ++track) {
    std::vector<int> all;
    std::set<int> pitches, cells;
    int notes = 0, slides = 0, accents = 0, kinds[4] = {0, 0, 0, 0};
    std::vector<std::vector<int>> bars;
    for (int row = from; row < to; ++row) {
      const int cell = scene.songs[songSlot].positions[row].patterns[track];
      cells.insert(cell);
      const auto kind = engine.songCellMaterialKind(track, static_cast<int16_t>(cell));
      kinds[static_cast<int>(kind) & 3]++;
      std::vector<int> bar(16, -1);
      if (const SynthPattern* p = cellPattern(scene, track, cell)) {
        for (int i = 0; i < 16; ++i) {
          bar[i] = p->steps[i].note;
          if (p->steps[i].note >= 0) {
            ++notes; pitches.insert(p->steps[i].note);
            slides += p->steps[i].slide; accents += p->steps[i].accent;
          }
        }
      }
      bars.push_back(bar);
    }
    int differ = 0;
    for (size_t b = 1; b < bars.size(); ++b) differ += bars[b] != bars[0];
    const int lo = pitches.empty() ? 0 : *pitches.begin();
    const int hi = pitches.empty() ? 0 : *pitches.rbegin();
    std::printf("%s\t%s\t%s\tbars=%d\tcells=%zu\tkinds(P/M/?/-)=%d/%d/%d/%d\tnotes/bar=%.1f\tdistinct=%zu\trange=%d\tslides=%d\taccents=%d\tbarsDifferFromBar1=%d\n",
                genre, phase, track ? "B" : "A", to - from, cells.size(), kinds[1], kinds[2], kinds[0],
                kinds[3], bars.empty() ? 0.0 : double(notes) / bars.size(), pitches.size(),
                hi - lo, slides, accents, differ);
    if (print) {
      for (size_t b = 0; b < bars.size() && b < 8; ++b) {
        std::printf("#   %s %s bar%zu:", phase, track ? "B" : "A", b + 1);
        for (int n : bars[b]) std::printf(" %s", nm(n).c_str());
        std::printf("\n");
      }
    }
  }
}

int main(int argc, char** argv) {
  const bool print = argc > 1;
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    const auto root = std::filesystem::temp_directory_path() / ("gp_matprobe_" + std::to_string(mode));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    SD.setRoot(root);
    GroovePuterPlatform::clearMaterialPublication("matprobe", 0);
    PatternPagingService::setProjectName("matprobe");
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
    const int before = scene.songs[songSlot].length;
    auto g = key('g');
    display.handleEvent(g);
    for (int i = 0; i < 3; ++i) display.update();
    const int afterG = scene.songs[songSlot].length;
    report(kGenre[mode], "TAKE", engine, before, afterG, print && mode < 3);
    gfx.texts.clear();
    auto d = key('d');
    display.handleEvent(d);
    for (int i = 0; i < 3; ++i) display.update();
    std::printf("%s\tD-screen\tstyle=%u", kGenre[mode], static_cast<unsigned>(GroovePuterState::currentGenerationLevel()));
    for (const auto& t : gfx.texts)
      if (t.find("DEV") != std::string::npos || t.find("GROW") != std::string::npos ||
          t.find("CYCLE") != std::string::npos || t.find("BREAK") != std::string::npos ||
          t.find("NO ") != std::string::npos || t.find("STYLE") != std::string::npos ||
          t.find("REFUS") != std::string::npos || t.find("CAN") != std::string::npos)
        std::printf(" | %s", t.c_str());
    std::printf("\n");
    const int afterD = scene.songs[songSlot].length;
    if (afterD > afterG) report(kGenre[mode], "DEVELOP", engine, afterG, afterD, print && mode < 3);
    else std::printf("%s\tDEVELOP\t-\tno new rows (length %d)\n", kGenre[mode], afterD);
  }
  return 0;
}
