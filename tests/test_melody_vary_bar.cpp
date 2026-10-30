// MELODY D varies the focused bar in place: ANSWER, PUSH, MIRROR, ORIGINAL;
// only that bar changes and each press is one Ctrl+Z step.
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
#include "src/phrase/melody_bar_vary.h"
#include "src/state/undo_owner.h"

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

UIEvent keyD() {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = 'd';
  return e;
}

UIEvent undoEvent() {
  UIEvent e{};
  e.event_type = GROOVEPUTER_APPLICATION_EVENT;
  e.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
  return e;
}

// Events whose start is outside bar `bar` must match exactly.
bool outsideSame(const Buffer& a, const Buffer& b, int bar) {
  if (a.count != b.count || a.lengthTicks != b.lengthTicks) return false;
  const uint32_t lo = bar * PhraseRuntime::kTicksPerBar;
  const uint32_t hi = lo + PhraseRuntime::kTicksPerBar;
  for (uint16_t i = 0; i < a.count; ++i) {
    if (a.events[i].startTick >= lo && a.events[i].startTick < hi) continue;
    if (std::memcmp(&a.events[i], &b.events[i], sizeof(a.events[i])) != 0) return false;
  }
  return true;
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_test_melody_vary_bar";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("vary_bar", 0);
  PatternPagingService::setProjectName("vary_bar");
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

  Buffer m{};
  m.lengthTicks = PhraseRuntime::kTicksPerBar * 2;
  const uint8_t bar0[4] = {60, 62, 64, 67};
  const uint8_t bar1[4] = {69, 67, 65, 64};
  for (int i = 0; i < 4; ++i) addNote(m, static_cast<uint16_t>(i * 96), bar0[i]);
  for (int i = 0; i < 4; ++i) {
    addNote(m, static_cast<uint16_t>(PhraseRuntime::kTicksPerBar + i * 96), bar1[i]);
  }
  assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, m));
  const Buffer original = engine.currentPhraseBuffer(0);

  page->phrase_view_ = SynthSequencerPage::PhraseView::Roll;
  page->phrase_cursor_.grid = RuntimePhraseEdit::Grid::Sixteenth;
  page->phrase_cursor_.cell = static_cast<uint8_t>(
      PhraseRuntime::kTicksPerBar / PhraseNotesCursor::quantumTicks(page->phrase_cursor_.grid));
  assert(PhraseNotesCursor::focusBar(page->phrase_cursor_) == 1);
  page->phrase_selection_ = PhraseSelectionState::at(engine.currentPhraseBuffer(0), 0);

  Buffer states[5];
  states[0] = engine.currentPhraseBuffer(0);
  for (int press = 1; press <= 4; ++press) {
    assert(display.handleEvent(keyD()));
    states[press] = engine.currentPhraseBuffer(0);
    assert(outsideSame(original, states[press], 1));
    if (press < 4) {
      assert(!RuntimePhraseEdit::same(states[press], states[press - 1]));
    }
  }
  assert(RuntimePhraseEdit::same(states[4], original));
  // Bar 1 only: ANSWER moved the last note two degrees, MIRROR kept the first.
  assert(states[1].events[7].note != original.events[7].note);
  assert(states[3].events[4].note == original.events[4].note);
  // PUSH: one 16th later; the note on beat 1 stays.
  assert(states[2].events[4].startTick == original.events[4].startTick);
  assert(states[2].events[5].startTick == original.events[5].startTick + 24);
  std::puts("MELODY vary bar: cycle PASS");

  // One Ctrl+Z after any press returns one step.
  for (int press = 1; press <= 4; ++press) {
    // Start each round from the original with a fresh cycle.
    assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, original));
    page->vary_original_.reset();
    page->vary_bar_ = -1;
    for (int i = 0; i < press; ++i) assert(display.handleEvent(keyD()));
    assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(0), states[press]));
    assert(display.handleEvent(undoEvent()));
    assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(0), states[press - 1]));
    // Press again after the undo: the cycle continues from the bar on screen.
    assert(display.handleEvent(keyD()));
    assert(RuntimePhraseEdit::same(engine.currentPhraseBuffer(0), states[press]));
  }
  std::puts("MELODY vary bar: undo PASS");

  // Focus on bar 0: its own cycle, bar 1 untouched.
  assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, original));
  page->phrase_cursor_.cell = 0;
  assert(display.handleEvent(keyD()));
  assert(page->vary_bar_ == 0 && page->vary_stage_ == 1);
  assert(outsideSame(original, engine.currentPhraseBuffer(0), 0));
  assert(!RuntimePhraseEdit::same(engine.currentPhraseBuffer(0), original));

  std::puts("MELODY vary bar: PASS");
  return 0;
}
