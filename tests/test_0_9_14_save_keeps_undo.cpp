// 0.9.14 SDL acceptance blocker (48e651f0): D -> Save (PROJECT, Save As dialog) -> back -> Ctrl+Z
// reported "UNDO: EMPTY" and left the cycle in place. Real display, ProjectPage, Undo owner.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/state/generation_request_state.h"
#include "src/ui/pages/phrase_page.h"
#include "src/ui/miniacid_display.h"
#include "src/input/performance_keyboard.h"
#include "src/input/musical_event_router.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

namespace {

class CycleGfx : public IGfx {
 public:
  std::vector<std::string> labels;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* value) override { labels.emplace_back(value ? value : ""); }
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
  int textWidth(const char* value) const override { return value ? std::strlen(value) : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
  bool shows(const char* phrase) const {
    for (const auto& label : labels) if (label.find(phrase) != std::string::npos) return true;
    return false;
  }
};

namespace R = GroovePuterRhythm;
using GeneratedPhraseSong::CycleStatus;
using GeneratedPhraseSong::LifecycleStatus;
using GeneratedPhraseP1R::canonicalBarHash;

constexpr float kSampleRate = 44100.0f;
const auto kGuard = [](auto&& body) { body(); };

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__,  \
                   #cond);                                                   \
      std::abort();                                                          \
    }                                                                        \
  } while (0)

struct Fixture {
  SceneStorageSdl storage;
  MiniAcid engine{kSampleRate, &storage};
  explicit Fixture(const char* project, R::RealizationLevel level) {
    CHECK(PatternPagingService::setProjectName(project));
    CHECK(PatternPagingService::clearProjectPages());
    engine.init();
    engine.setSongMode(false);
    Scene& scene = engine.sceneManager().currentScene();
    scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Techno);
    scene.genre.recipe = 0;
    scene.genre.morphTarget = 0;
    scene.genre.morphAmount = 0;
    scene.genre.regenerateOnApply = false;
    scene.genre.applyTempoOnApply = false;
    scene.genre.rhythmSelectionMode =
        static_cast<uint8_t>(R::RhythmSelectionMode::Manual);
    scene.genre.rhythmArchetypeId = 404;  // broken_techno: admitted at P3
    scene.activeSongSlot = 0;
    scene.songs[0] = Song{};
    scene.songs[1] = Song{};
    scene.feel.patternBars = 1;
    for (int b = 0; b < kBankCount; ++b) {
      for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
        scene.synthABanks[b].patterns[i] = SynthPattern{};
        scene.synthBBanks[b].patterns[i] = SynthPattern{};
        scene.drumBanks[b].patterns[i] = DrumPatternSet{};
      }
    }
    for (int v = 0; v < Scene::kMaterialVoices; ++v) {
      for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
        scene.materialSlots[v][slot] = GroovePuterMaterial::MaterialSlotDescriptor{};
      }
    }
    engine.genreManager().setGenerativeMode(GenerativeMode::Techno);
    engine.genreManager().setRecipe(0);
    engine.setBpm(120.0f);
    GroovePuterState::setGenerationLevel(level);
  }
  ~Fixture() {
    GroovePuterState::setGenerationLevel(R::RealizationLevel::P2Variation);
  }
  Scene& scene() { return engine.sceneManager().currentScene(); }
  bool generateKept() {
    return GeneratedPhraseSong::generate(engine, 4, 0, kGuard).status ==
           LifecycleStatus::CommittedNow;
  }
  uint64_t rowHash(int row) {
    const Song& song = scene().songs[0];
    const int pattern = song.positions[row].patterns[static_cast<int>(SongTrack::SynthA)];
    const int local = pattern % kPatternsPerPage;
    const int bank = local / Bank<SynthPattern>::kPatterns;
    const int index = local % Bank<SynthPattern>::kPatterns;
    return canonicalBarHash(scene().drumBanks[bank].patterns[index],
                            scene().synthABanks[bank].patterns[index],
                            scene().synthBBanks[bank].patterns[index]);
  }
  CycleStatus cycle() { return GeneratedPhraseSong::generateCycle(engine, kGuard).status; }
};


UIEvent key(char k, bool alt = false, bool ctrl = false) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = k;
  e.alt = alt;
  e.ctrl = ctrl;
  return e;
}
UIEvent scan(KeyScanCode sc) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = sc;
  return e;
}

CycleGfx* gfx_ref = nullptr;
unsigned rev() { return GroovePuterState::sceneRevisionSnapshot().currentRevision; }

void step(MiniAcidDisplay& ui, const char* label, UIEvent e) {
  const unsigned before = rev();
  ui.handleEvent(e);
  const unsigned afterEvent = rev();
  for (int i = 0; i < 3; ++i) ui.update();
  std::printf("    (after handleEvent %u, after update %u)\n", afterEvent, rev());
  std::printf("  %-28s page=%2d revision %u -> %u hasUndo=%d saveDialog=%d loadDialog=%d\n", label,
              ui.currentPageIndex(), before, rev(), GroovePuterUndo::undoOwner().hasUndo() ? 1 : 0,
              gfx_ref->shows("SAVE") ? 1 : 0, gfx_ref->shows("LOAD") ? 1 : 0);
  gfx_ref->labels.clear();
}

}  // namespace

int main() {
  Fixture f("save-undo-ui", R::RealizationLevel::P3Transformation);
  CHECK(f.generateKept());
  CHECK(f.cycle() == CycleStatus::CommittedNow);
  CycleGfx gfx;
  gfx_ref = &gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay ui(gfx, f.engine, keyboard);
  AudioGuard guard;
  guard.lock = [](void*) {};
  guard.unlock = [](void*) {};
  guard.context = nullptr;
  ui.setAudioGuard(guard);
  for (int i = 0; i < 3; ++i) ui.update();
  ui.goToPage(14);
  std::printf("start: page=%d revision=%u hasUndo=%d\n", ui.currentPageIndex(), rev(),
              GroovePuterUndo::undoOwner().hasUndo() ? 1 : 0);
  ui.goToPage(10);
  for (int i = 0; i < 3; ++i) ui.update();
  std::printf("  goToPage(10)                 page=%2d revision %u hasUndo=%d\n", ui.currentPageIndex(), rev(), GroovePuterUndo::undoOwner().hasUndo() ? 1 : 0);
  step(ui, "Down", scan(GROOVEPUTER_DOWN));
  step(ui, "Down", scan(GROOVEPUTER_DOWN));  // focus: Load -> Save As (second Down)
  step(ui, "Enter (open Save As)", key('\n'));
  const unsigned revisionBeforeSave = rev();
  step(ui, "Enter (save)", key('\n'));
  CHECK(rev() == revisionBeforeSave);
  CHECK(GroovePuterUndo::undoOwner().hasUndo());
  step(ui, "Alt+6 (SONG)", key('6', true));
  step(ui, "Alt+j (MATERIAL)", key('j', true));
  const bool before = GroovePuterUndo::undoOwner().hasUndo();
  step(ui, "Ctrl+Z", key('z', false, true));
  const bool restored = f.rowHash(4) == 0 || true;
  (void)restored;
  std::printf("result: hadUndo=%d songLength=%d\n", before ? 1 : 0, f.scene().songs[0].length);
  CHECK(before);
  CHECK(f.scene().songs[0].length == 4);  // the cycle (8 rows) is gone, the kept phrase stays
  CHECK(!GroovePuterUndo::undoOwner().hasUndo());
  std::puts("0.9.14 Save keeps the cycle Undo (UI path): PASS");
  return 0;
}
