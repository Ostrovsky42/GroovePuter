// MELODY D varies the focused bar in place:
//   1 ANSWER, 2 SEQUENCE, 3 SYNCOPATE, 4 CONTOUR, 5 OCTAVE, 6 ORIGINAL;
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

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

void addNote(Buffer& m, uint16_t tick, uint8_t note, uint16_t durTicks = 4u) {
  auto& e = m.events[m.count++];
  e.startTick = tick;
  e.durationSubticks = durTicks * PhraseRuntime::kSubticksPerTick;
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
  engine.sceneManager().currentScene().generatorParams.scaleRoot = 0;
  engine.sceneManager().currentScene().generatorParams.scale = MAJOR;

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

  constexpr int kTotalPresses = MelodyBarVary::kVariants;
  static_assert(kTotalPresses == 6, "5 variations + 1 return to Original");

  Buffer states[kTotalPresses + 1];
  states[0] = engine.currentPhraseBuffer(0);
  for (int press = 1; press <= kTotalPresses; ++press) {
    assert(display.handleEvent(keyD()));
    states[press] = engine.currentPhraseBuffer(0);
    assert(RuntimePhraseEdit::validate(states[press]));
    assert(outsideSame(original, states[press], 1));
    if (press < kTotalPresses) {
      assert(!RuntimePhraseEdit::same(states[press], states[press - 1]));
      assert(!RuntimePhraseEdit::same(states[press], original));
    }
  }
  assert(RuntimePhraseEdit::same(states[kTotalPresses], original));

  // 1. ANSWER: opening stays intact, last note moves two scale degrees.
  assert(states[1].events[4].note == original.events[4].note);
  assert(states[1].events[7].note != original.events[7].note);

  // 2. SEQUENCE: first half of bar (events 4, 5) stays intact; second half
  //    (events 6, 7) shifts along the key.
  assert(states[2].events[4].note == original.events[4].note);
  assert(states[2].events[5].note == original.events[5].note);
  assert(states[2].events[6].note != original.events[6].note);
  assert(states[2].events[7].note != original.events[7].note);

  // 3. SYNCOPATE: beat 1 (event 4) and beat 3 (event 6) stay anchored;
  //    weak beats 2 and 4 (events 5, 7) anticipate by one 16th (-24 ticks)
  //    without wrapping around the bar boundary.
  assert(states[3].events[4].startTick == original.events[4].startTick);
  assert(states[3].events[5].startTick == original.events[5].startTick - 24);
  assert(states[3].events[6].startTick == original.events[6].startTick);
  assert(states[3].events[7].startTick == original.events[7].startTick - 24);

  // 4. CONTOUR: first note stays anchored; inverted notes stay within a
  //    bounded musical corridor around the pivot.
  assert(states[4].events[4].note == original.events[4].note);
  for (int i = 5; i <= 7; ++i) {
    const int diff = static_cast<int>(states[4].events[i].note) -
                     static_cast<int>(original.events[4].note);
    assert(diff >= -9 && diff <= 9);
  }

  // 5. OCTAVE: alternating onsets (events 5, 7) jump an octave with accent;
  //    approach onsets (events 4, 6) get slide.
  assert(states[5].events[4].note == original.events[4].note);
  assert((states[5].events[4].flags & PhraseRuntime::kEventSlide) != 0);
  assert(states[5].events[5].note == original.events[5].note + 12);
  assert((states[5].events[5].flags & PhraseRuntime::kEventAccent) != 0);
  assert(states[5].events[7].note == original.events[7].note + 12);
  assert((states[5].events[7].flags & PhraseRuntime::kEventAccent) != 0);

  // Repeated-note pedal bar: CONTOUR weaves upper and lower scale neighbours
  // instead of leaving a flat line, and SYNCOPATE trims overlapping durations.
  {
    Buffer pedal{};
    pedal.lengthTicks = PhraseRuntime::kTicksPerBar;
    for (int i = 0; i < 4; ++i) addNote(pedal, static_cast<uint16_t>(i * 96), 60, 96);
    const Buffer c = MelodyBarVary::varyBar(
        pedal, 0, MelodyBarVary::Contour, 0,
        static_cast<ProjectKey::ScaleTypeValue>(MAJOR));
    assert(c.events[0].note == 60);
    assert(c.events[1].note == 62);  // +1 degree (D4)
    assert(c.events[2].note == 59);  // -1 degree (B3)
    assert(c.events[3].note == 62);  // +1 degree (D4)

    const Buffer s = MelodyBarVary::varyBar(
        pedal, 0, MelodyBarVary::Syncopate, 0,
        static_cast<ProjectKey::ScaleTypeValue>(MAJOR));
    assert(RuntimePhraseEdit::validate(s));
    assert(!RuntimePhraseEdit::hasOverlappingNotes(s));
  }

  std::puts("MELODY vary bar: cycle PASS");

  // One Ctrl+Z after any press returns one step.
  for (int press = 1; press <= kTotalPresses; ++press) {
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
