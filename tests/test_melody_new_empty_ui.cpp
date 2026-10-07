// Alt+N on SYNTH A NOTES: a new, empty Melody in the current slot (0.9.17).
// Every slot holds steps (the default scene fills them), so Alt+R alone always
// starts from a copy of them; Alt+N starts from nothing. The slot's steps stay
// until the Melody is saved, and each step has its own Ctrl+Z.
#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "platform_sdl/sdl_display.h"
#include "src/audio/pattern_paging.h"
#include "src/input/performance_keyboard.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/miniacid_display.h"
#include "src/ui/workflow_mode.h"

SerialMock Serial;
SDMock SD;

namespace {

class NullGfx : public IGfx {
 public:
  std::vector<std::string> texts;
  bool has(const char* v) const {
    for (const auto& t : texts) if (t.find(v) != std::string::npos) return true;
    return false;
  }
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

UIEvent altKey(char value) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = value;
  e.alt = true;
  return e;
}

int stepNotes(const SynthPattern& pattern) {
  int notes = 0;
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    if (pattern.steps[i].note >= 0) ++notes;
  }
  return notes;
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_test_new_empty_melody";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("new_empty", 0);
  PatternPagingService::setProjectName("new_empty");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  MiniAcid engine{44100.0f, &storage};
  engine.init();
  engine.setSongMode(false);
  engine.set303PatternIndex(0, 3);  // A4
  const int stepsBefore =
      stepNotes(engine.sceneManager().currentScene().synthABanks[0].patterns[3]);
  assert(stepsBefore > 0);  // the default scene fills every slot with steps

  NullGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  display.goToPage(WorkflowPages::kSynthA);

  auto altN = altKey('n');
  assert(display.handleEvent(altN));
  assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);
  assert(engine.currentPhraseBuffer(0).count == 0);
  // Nothing is replaced until Alt+Enter: the slot still holds its steps.
  assert(!engine.isMelodySlot(0, 0, 3));
  assert(stepNotes(engine.sceneManager().currentScene().synthABanks[0].patterns[3]) ==
         stepsBefore);

  // The editor says which Melody this is and that it is not saved yet; the
  // status line says MEL, not the internal PHR.
  gfx.texts.clear();
  display.update();
  assert(gfx.has("MEL A4*"));
  assert(gfx.has("S-A MEL "));
  assert(!gfx.has("MATERIAL"));

  // Alt+N again on an empty unsaved Melody is harmless: still empty, no error.
  auto altN2 = altKey('n');
  display.handleEvent(altN2);
  assert(engine.currentPhraseBuffer(0).count == 0);

  return 0;
}
