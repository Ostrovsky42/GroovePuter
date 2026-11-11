// GENRE usability (2026-11-10 audit): what the page says about tempo, about
// the 4-bar path and about a choice that was never applied.
//
//  U1  the genre's tempo corridor is on the GENRE row; a project tempo outside
//      it is named on the status line together with the key that fixes it
//  U1  NEW TAKE + TEMPO names the tempo it will set, and sets exactly that
//  U2  with nothing else to report the status line points at MATERIAL
//  U3  leaving GENRE with an unapplied choice says which genre stays active,
//      and changes nothing
//  U3  MATERIAL names the active genre and the project tempo
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/miniacid_engine.h"
#include "src/generation/composition/generation_profile.h"
#include "src/input/performance_keyboard.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/genre_tempo_text.h"
#include "src/ui/miniacid_display.h"
#include "src/ui/workflow_mode.h"

SerialMock Serial;
SDMock SD;

namespace {

class TextGfx : public IGfx {
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
  int textWidth(const char* s) const override { return s ? static_cast<int>(std::strlen(s)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

bool shows(const TextGfx& gfx, const std::string& needle) {
  for (const std::string& text : gfx.texts) {
    if (text.find(needle) != std::string::npos) return true;
  }
  return false;
}

void expectShown(const TextGfx& gfx, const std::string& needle, const char* what) {
  if (shows(gfx, needle)) return;
  std::printf("FAIL %s: \"%s\" not on screen. Screen:\n", what, needle.c_str());
  for (const std::string& text : gfx.texts) std::printf("    | %s\n", text.c_str());
  ++failures;
}

UIEvent key(char c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = c;
  return e;
}

UIEvent nav(KeyScanCode scancode) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = scancode;
  return e;
}

void frame(MiniAcidDisplay& display, TextGfx& gfx) {
  gfx.texts.clear();
  display.update();
}

void press(MiniAcidDisplay& display, TextGfx& gfx, UIEvent event) {
  display.handleEvent(event);
  frame(display, gfx);
}

bool samePattern(const SynthPattern& a, const SynthPattern& b) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    if (a.steps[i].note != b.steps[i].note || a.steps[i].slide != b.steps[i].slide ||
        a.steps[i].accent != b.steps[i].accent) return false;
  }
  return true;
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_genre_page_usability";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("genreux", 0);
  PatternPagingService::setProjectName("genreux");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(44100, &storage);
  engine.init();
  engine.setSongMode(false);
  engine.setBpm(100.0f);
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre = GenreSettings{};  // Acid / BASE, APPLY = STYLE ONLY

  TextGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  display.goToPage(WorkflowPages::kGenre);
  frame(display, gfx);

  // --- U1: the corridor is on the row, the mismatch on the status line.
  expectShown(gfx, "Acid  118-150", "GENRE row carries the corridor");
  expectShown(gfx, "ACID 100 BPM (118-150) M:+TEMPO", "tempo outside the corridor is named");

  // Ten genres to the right of Acid is Techno (saved order, never reordered).
  for (int i = 0; i < 10; ++i) press(display, gfx, nav(GROOVEPUTER_RIGHT));
  expectShown(gfx, "Techno  124-146", "corridor follows the selected genre");
  expect(scene.genre.generativeMode == static_cast<uint8_t>(GenerativeMode::Acid),
         "browsing does not change the active genre");

  // G: a new TAKE, tempo untouched, and the page says the tempo is off.
  press(display, gfx, key('g'));
  frame(display, gfx);
  expect(scene.genre.generativeMode == static_cast<uint8_t>(GenerativeMode::Techno),
         "G applies the selected genre");
  expect(std::lround(engine.bpm()) == 100, "plain NEW TAKE leaves the tempo alone");
  expectShown(gfx, "TECHNO 100 BPM (124-146) M:+TEMPO", "after G the mismatch is still named");

  // M, M: STYLE ONLY -> NEW TAKE -> NEW TAKE + TEMPO; the row names the tempo.
  press(display, gfx, key('m'));
  press(display, gfx, key('m'));
  expectShown(gfx, "NEW TAKE + TEMPO 134", "APPLY row names the tempo it will set");
  expectShown(gfx, "TECHNO 100 BPM (124-146) G SETS 134", "status names what G will do");

  press(display, gfx, key('g'));
  frame(display, gfx);
  expect(std::lround(engine.bpm()) == 134, "NEW TAKE + TEMPO sets the suggested tempo");
  // --- U2: nothing to report, so the line points at the 4-bar path.
  expectShown(gfx, "G 1 BAR  ALT+8 MATERIAL 4 BARS", "idle status line points at MATERIAL");
  expect(!shows(gfx, "BPM (124-146)"), "no tempo warning inside the corridor");

  // An applied choice leaves quietly (checked first: toasts outlive a frame).
  display.goToPage(WorkflowPages::kPhrase);
  frame(display, gfx);
  expect(!shows(gfx, "NOT APPLIED"), "no notice when the selection is the active genre");
  display.goToPage(WorkflowPages::kGenre);
  frame(display, gfx);

  // --- U3: an unapplied choice is named on the way out and changes nothing.
  const int bank = engine.current303BankIndex(0);
  const int slot = engine.current303PatternIndex(0);
  const SynthPattern before = scene.synthABanks[bank].patterns[slot];
  press(display, gfx, nav(GROOVEPUTER_LEFT));  // House selected, not applied
  expectShown(gfx, "House  112-128", "House is selected");
  display.goToPage(WorkflowPages::kPhrase);
  frame(display, gfx);
  expectShown(gfx, "HOUSE NOT APPLIED: STILL TECHNO", "leaving names the active genre");
  expect(scene.genre.generativeMode == static_cast<uint8_t>(GenerativeMode::Techno),
         "leaving GENRE applies nothing");
  expect(samePattern(before, scene.synthABanks[bank].patterns[slot]),
         "leaving GENRE keeps the pattern");
  expect(std::lround(engine.bpm()) == 134, "leaving GENRE keeps the tempo");
  expectShown(gfx, "TECHNO 134 BPM (124-146)", "MATERIAL names the active genre and tempo");

  // --- helper contract: every genre and recipe has a corridor to show.
  for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
    const auto genre = static_cast<GenerativeMode>(mode);
    for (uint8_t i = 0; i < GroovePuterRhythm::availableRecipeCount(genre); ++i) {
      GenreRecipeId recipe = kBaseRecipeId;
      if (!GroovePuterRhythm::availableRecipeAt(genre, i, recipe)) continue;
      GenreSettings settings{};
      settings.generativeMode = static_cast<uint8_t>(mode);
      settings.recipe = recipe;
      const GenreTempoText::Info inside = GenreTempoText::infoFor(settings, 0.0f);
      expect(inside.fit == GenreTempoText::Fit::Below && inside.suggestedBpm >= inside.bpmMin &&
                 inside.suggestedBpm <= inside.bpmMax,
             "every profile has a corridor");
      expect(!GenreTempoText::outside(GenreTempoText::infoFor(
                 settings, static_cast<float>(inside.suggestedBpm))),
             "the suggested tempo is inside its own corridor");
      char row[40];
      GenreTempoText::formatGenreRow(settings, inside, row, sizeof(row));
      expect(std::strlen(row) <= 26, "GENRE row value fits the row");
      char status[56];
      GenreTempoText::formatStatus(settings, GenreTempoText::infoFor(settings, 300.0f),
                                   "G SETS 174", status, sizeof(status));
      expect(std::strlen(status) <= 39, "status line fits 240 px");
    }
  }

  std::printf(failures ? "FAILED\n" : "test_genre_page_usability OK\n");
  return failures ? 1 : 0;
}
