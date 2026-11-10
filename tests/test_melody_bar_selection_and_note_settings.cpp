// MELODY Ctrl+A bar selection, Alt+S slide toggle, and Alt+E NOTE SETTINGS window.
#include <algorithm>
#include <cassert>
#include <cstdio>
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
#include "src/phrase/runtime_phrase_edit.h"
#include "src/state/step_setting_marks.h"
#include "src/ui/miniacid_display.h"
#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/workflow_mode.h"

SerialMock Serial;
SDMock SD;

namespace {

class CaptureGfx : public IGfx {
 public:
  std::vector<std::string> texts;
  bool has(const char* v) const {
    for (const auto& t : texts) {
      if (t.find(v) != std::string::npos) return true;
    }
    return false;
  }
  void clearTexts() { texts.clear(); }
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* s) override {
    if (s && *s) texts.push_back(s);
  }
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

UIEvent keyEvent(char value, bool ctrl = false, bool alt = false) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = value;
  e.ctrl = ctrl;
  e.alt = alt;
  return e;
}

UIEvent scanEvent(KeyScanCode value, bool ctrl = false, bool alt = false) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = value;
  e.ctrl = ctrl;
  e.alt = alt;
  return e;
}

UIEvent undoEvent() {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = 'z';
  e.scancode = GROOVEPUTER_Z;
  e.ctrl = true;
  return e;
}

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

void addNote(Buffer& m, uint16_t tick, uint8_t note, uint8_t vel = 100,
             uint8_t prob = 100, uint8_t flags = 0) {
  auto& e = m.events[m.count++];
  e.startTick = tick;
  e.durationSubticks = 48u * PhraseRuntime::kSubticksPerTick;
  e.note = note;
  e.velocity = vel;
  e.probability = prob;
  e.flags = flags;
}

Buffer twoBarFixture() {
  Buffer m{};
  m.lengthTicks = PhraseRuntime::kTicksPerBar * 2;
  // Bar 1: three notes (C4, E4, G4)
  addNote(m, 0, 60);
  addNote(m, 48, 64);
  addNote(m, 96, 67);
  // Bar 2: one note (A4)
  addNote(m, PhraseRuntime::kTicksPerBar, 69);
  return m;
}

}  // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() / "gp_test_melody_bar_and_note_settings";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("melody_bar_note_settings", 0);
  PatternPagingService::setProjectName("melody_bar_note_settings");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  MiniAcid engine{44100.0f, &storage};
  engine.init();
  engine.setSongMode(false);
  engine.sceneManager().currentScene().generatorParams.scaleRoot = 0;
  engine.sceneManager().currentScene().generatorParams.scale = MAJOR;

  CaptureGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  display.goToPage(WorkflowPages::kSynthA);

  // Switch Synth A to an empty Melody, then load the 2-bar fixture.
  auto altN = keyEvent('n', false, true);
  assert(display.handleEvent(altN));
  const Buffer base = twoBarFixture();
  assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, base));

  // 1. Alt+S toggles slide on the focused note (tick 0, C4) with Undo.
  {
    auto altS = keyEvent('s', false, true);
    assert(display.handleEvent(altS));
    const auto& live = engine.currentPhraseBuffer(0);
    assert((live.events[0].flags & PhraseRuntime::kEventSlide) != 0);
    assert((live.events[1].flags & PhraseRuntime::kEventSlide) == 0);

    auto cz = undoEvent();
    assert(display.handleEvent(cz));
    assert((engine.currentPhraseBuffer(0).events[0].flags &
            PhraseRuntime::kEventSlide) == 0);
  }

  // 2. Ctrl+A selects all notes in bar 1; pitch/octave/length/accent/slide/delete
  //    act on bar 1 only and leave bar 2 untouched.
  {
    auto ctrlA = keyEvent('a', true, false);
    assert(display.handleEvent(ctrlA));
    gfx.clearTexts();
    display.update();
    assert(gfx.has("BAR 1/2 ALL"));
    assert(gfx.has("SELECTED BAR 1 (3 NOTES)"));

    // Octave up ('s') shifts bar 1 notes by +12 semitones, bar 2 stays at 69.
    auto octUp = keyEvent('s');
    assert(display.handleEvent(octUp));
    {
      const auto& live = engine.currentPhraseBuffer(0);
      assert(live.events[0].note == 72);
      assert(live.events[1].note == 76);
      assert(live.events[2].note == 79);
      assert(live.events[3].note == 69);
    }

    // Undo restores all three notes in bar 1 in one step.
    auto cz = undoEvent();
    assert(display.handleEvent(cz));
    {
      const auto& live = engine.currentPhraseBuffer(0);
      assert(live.events[0].note == 60);
      assert(live.events[1].note == 64);
      assert(live.events[2].note == 67);
    }

    // Alt+S uniformly sets slide on all 3 notes in bar 1, then toggles off.
    auto altS = keyEvent('s', false, true);
    assert(display.handleEvent(altS));
    for (int i = 0; i < 3; ++i) {
      assert((engine.currentPhraseBuffer(0).events[i].flags &
              PhraseRuntime::kEventSlide) != 0);
    }
    assert((engine.currentPhraseBuffer(0).events[3].flags &
            PhraseRuntime::kEventSlide) == 0);
    assert(display.handleEvent(altS));
    for (int i = 0; i < 3; ++i) {
      assert((engine.currentPhraseBuffer(0).events[i].flags &
              PhraseRuntime::kEventSlide) == 0);
    }

    // Alt+Left shortens all 3 notes in bar 1 by one grid unit (48 -> 24 ticks).
    auto altLeft = scanEvent(GROOVEPUTER_LEFT, false, true);
    assert(display.handleEvent(altLeft));
    for (int i = 0; i < 3; ++i) {
      assert(engine.currentPhraseBuffer(0).events[i].durationSubticks ==
             24u * PhraseRuntime::kSubticksPerTick);
    }
    assert(engine.currentPhraseBuffer(0).events[3].durationSubticks ==
           48u * PhraseRuntime::kSubticksPerTick);
    assert(display.handleEvent(cz));

    // Backspace deletes all notes in bar 1 as one Undo step and drops bar selection.
    auto bksp = keyEvent('\b');
    assert(display.handleEvent(bksp));
    assert(engine.currentPhraseBuffer(0).count == 1);
    assert(engine.currentPhraseBuffer(0).events[0].startTick ==
           PhraseRuntime::kTicksPerBar);
    assert(engine.currentPhraseBuffer(0).events[0].note == 69);

    // Ctrl+Z restores bar 1.
    assert(display.handleEvent(cz));
    assert(engine.currentPhraseBuffer(0).count == 4);
  }

  // 3. Alt+E NOTE SETTINGS on a single note: VEL, PROB, SLIDE, CHANCE, reset, Undo.
  {
    auto altE = keyEvent('e', false, true);
    assert(display.handleEvent(altE));
    gfx.clearTexts();
    display.update();
    assert(gfx.has("NOTE SETTINGS  B1 C4"));
    assert(gfx.has("VEL"));
    assert(gfx.has("PROB"));
    assert(gfx.has("SLIDE"));
    assert(gfx.has("CHANCE"));

    // Opt (TOGGLE_SOURCE) is ignored while NOTE SETTINGS is open.
    UIEvent optEv{};
    optEv.event_type = GROOVEPUTER_APPLICATION_EVENT;
    optEv.app_event_type = GROOVEPUTER_APP_EVENT_TOGGLE_SOURCE;
    assert(display.handleEvent(optEv));
    assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);

    // Field 0 (VEL): Left decreases 100 -> 95.
    auto left = scanEvent(GROOVEPUTER_LEFT);
    assert(display.handleEvent(left));
    assert(engine.currentPhraseBuffer(0).events[0].velocity == 95);

    // Field 1 (PROB): Down, then Left decreases 100 -> 95.
    auto down = scanEvent(GROOVEPUTER_DOWN);
    assert(display.handleEvent(down));
    assert(display.handleEvent(left));
    assert(engine.currentPhraseBuffer(0).events[0].probability == 95);

    // Field 2 (SLIDE): Down, then Right sets SLIDE ON.
    auto right = scanEvent(GROOVEPUTER_RIGHT);
    assert(display.handleEvent(down));
    assert(display.handleEvent(right));
    assert((engine.currentPhraseBuffer(0).events[0].flags &
            PhraseRuntime::kEventSlide) != 0);

    // Field 3 (CHANCE): Down, then Right sets CHANCE 80% (kEventGhost).
    assert(display.handleEvent(down));
    assert(display.handleEvent(right));
    assert((engine.currentPhraseBuffer(0).events[0].flags &
            PhraseRuntime::kEventGhost) != 0);

    gfx.clearTexts();
    display.update();
    // 95% * 80% = 76%
    assert(gfx.has("80% =76%"));

    // Backspace resets only the selected field (CHANCE -> 100%).
    auto bksp = keyEvent('\b');
    assert(display.handleEvent(bksp));
    assert((engine.currentPhraseBuffer(0).events[0].flags &
            PhraseRuntime::kEventGhost) == 0);
    assert((engine.currentPhraseBuffer(0).events[0].flags &
            PhraseRuntime::kEventSlide) != 0);

    // Ctrl+Backspace resets all fields of the note to defaults.
    auto ctrlBksp = keyEvent('\b', true, false);
    assert(display.handleEvent(ctrlBksp));
    {
      const auto& e0 = engine.currentPhraseBuffer(0).events[0];
      assert(e0.velocity == 100);
      assert(e0.probability == 100);
      assert((e0.flags & PhraseRuntime::kEventSlide) == 0);
      assert((e0.flags & PhraseRuntime::kEventGhost) == 0);
      assert(!GroovePuterUndo::StepSettingMarks::melodyNoteHasCustomSettings(e0));
    }

    // Esc closes NOTE SETTINGS.
    auto esc = scanEvent(GROOVEPUTER_ESCAPE);
    assert(display.handleEvent(esc));
    gfx.clearTexts();
    display.update();
    assert(!gfx.has("NOTE SETTINGS"));
  }

  // 4. Ctrl+A + Alt+E batch NOTE SETTINGS with MIXED detection.
  {
    // Give event 1 a different velocity (80) so bar 1 has mixed VEL.
    Buffer mixed = base;
    mixed.events[1].velocity = 80;
    assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, mixed));

    auto ctrlA = keyEvent('a', true, false);
    assert(display.handleEvent(ctrlA));
    auto altE = keyEvent('e', false, true);
    assert(display.handleEvent(altE));

    gfx.clearTexts();
    display.update();
    assert(gfx.has("NOTE SETTINGS  BAR 1 (3)"));
    assert(gfx.has("MIXED"));

    // Adjust VEL (field 0) Left: based on first note (100 -> 95), applied to all 3 notes in bar 1.
    auto left = scanEvent(GROOVEPUTER_LEFT);
    assert(display.handleEvent(left));
    for (int i = 0; i < 3; ++i) {
      assert(engine.currentPhraseBuffer(0).events[i].velocity == 95);
    }
    // Bar 2 note stays at 100.
    assert(engine.currentPhraseBuffer(0).events[3].velocity == 100);

    gfx.clearTexts();
    display.update();
    assert(!gfx.has("MIXED"));
  }

  std::puts("MELODY Ctrl+A bar selection, Alt+S slide, Alt+E NOTE SETTINGS: PASS");
  return 0;
}
