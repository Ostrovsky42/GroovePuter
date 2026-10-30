// MELODY roll: Left/Right step from note to note, so a note off the grid is
// reachable without changing the grid; with no note ahead, the grid move stays.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/ui/miniacid_display.h"
#include "src/ui/pages/synth_sequencer_page.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "platform_sdl/sdl_display.h"
#include "src/audio/pattern_paging.h"
#include "src/input/performance_keyboard.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/workflow_mode.h"
#include "src/phrase/runtime_phrase_edit.h"

SerialMock Serial;
SDMock SD;

namespace {

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

UIEvent scanEvent(KeyScanCode value) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = value;
  return e;
}

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

void addNote(Buffer& m, uint16_t tick, uint8_t note) {
  auto& e = m.events[m.count++];
  e.startTick = tick;
  e.durationSubticks = 4u * PhraseRuntime::kSubticksPerTick;
  e.note = note;
  e.velocity = 90;
  e.probability = 100;
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_test_melody_note_step";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("note_step", 0);
  PatternPagingService::setProjectName("note_step");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  MiniAcid engine{44100.0f, &storage};
  engine.init();
  engine.setSongMode(false);

  NullGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  display.goToPage(WorkflowPages::kSynthA);
  UIEvent altN{};
  altN.event_type = GROOVEPUTER_KEY_DOWN;
  altN.key = 'n';
  altN.alt = true;
  assert(display.handleEvent(altN));

  SynthSequencerPage* page = nullptr;
  for (auto& p : display.pages_) {
    if (auto* s = dynamic_cast<SynthSequencerPage*>(p.get())) {
      if (s->voice_index_ == 0) page = s;
    }
  }
  assert(page != nullptr);

  // Ticks 0, 5 (off the 1/8 grid of 48 ticks), 16, and a second note at 16.
  Buffer m{};
  m.lengthTicks = PhraseRuntime::kTicksPerBar * 2;
  addNote(m, 0, 60);   // 0
  addNote(m, 5, 62);   // 1  off grid
  addNote(m, 16, 64);  // 2
  addNote(m, 16, 67);  // 3  same tick as 2
  assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, m));

  page->phrase_view_ = SynthSequencerPage::PhraseView::Roll;
  page->phrase_cursor_.cell = 0;
  page->phrase_selection_ = PhraseSelectionState::at(engine.currentPhraseBuffer(0), 0);
  const uint16_t cellTicks = PhraseNotesCursor::quantumTicks(page->phrase_cursor_.grid);
  assert(cellTicks > 16);  // all four notes share the first cell

  auto right = scanEvent(GROOVEPUTER_RIGHT);
  auto left = scanEvent(GROOVEPUTER_LEFT);

  assert(display.handleEvent(right));
  assert(page->phrase_selection_.active && page->phrase_selection_.eventIndex == 1);
  assert(page->phrase_selection_.startTick == 5);
  assert(page->phrase_cursor_.cell == 0);

  assert(display.handleEvent(right));
  assert(page->phrase_selection_.eventIndex == 2);  // first of the two at 16
  assert(display.handleEvent(right));
  assert(page->phrase_selection_.eventIndex == 3);  // then the second, array order
  assert(page->phrase_cursor_.cell == 0);

  // No note ahead: the grid move, exactly as before.
  assert(display.handleEvent(right));
  assert(page->phrase_cursor_.cell == 1);

  // Left from the empty cell goes back to the last note.
  assert(display.handleEvent(left));
  assert(page->phrase_selection_.eventIndex == 3);
  assert(display.handleEvent(left));
  assert(page->phrase_selection_.eventIndex == 2);
  assert(display.handleEvent(left));
  assert(page->phrase_selection_.eventIndex == 1);
  assert(display.handleEvent(left));
  assert(page->phrase_selection_.eventIndex == 0);
  // Nothing behind the first note: grid move holds at cell 0.
  assert(display.handleEvent(left));
  assert(page->phrase_cursor_.cell == 0);
  assert(page->phrase_selection_.eventIndex == 0);

  std::puts("MELODY note step: PASS");
  return 0;
}
