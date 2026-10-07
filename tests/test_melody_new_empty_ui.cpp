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
#include "src/phrase/runtime_phrase_edit.h"
#include "src/ui/phrase_chord_focus.h"
#include "src/ui/project_key.h"

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
  // A builds chord tones in the project key: C major here.
  engine.sceneManager().currentScene().generatorParams.scaleRoot = 0;
  engine.sceneManager().currentScene().generatorParams.scale = MAJOR;
  const int stepsBefore =
      stepNotes(engine.sceneManager().currentScene().synthABanks[0].patterns[3]);
  assert(stepsBefore > 0);  // the default scene fills every slot with steps

  NullGfx gfx;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx, engine, keyboard);
  display.dismissSplash();
  display.goToPage(WorkflowPages::kSynthA);
  assert(!display.repeatsAltVertical());  // STEPS: Alt+Up/Down edits values

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

  // Held Alt+Up/Down scrolls the Melody roll, so it repeats here; elsewhere
  // modified keys never repeat.
  assert(display.repeatsAltVertical());

  // ] on the Melody switches the workflow page, as everywhere; [ comes back.
  {
    const int before = display.currentPageIndex();
    UIEvent next{};
    next.event_type = GROOVEPUTER_KEY_DOWN;
    next.key = ']';
    display.handleEvent(next);
    assert(display.currentPageIndex() != before);
    UIEvent prev = next;
    prev.key = '[';
    display.handleEvent(prev);
    assert(display.currentPageIndex() == before);
    assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);
  }

  // Alt+H from the Melody opens the synth help at its MELODY block.
  auto altH = altKey('h');
  display.handleEvent(altH);
  gfx.texts.clear();
  display.update();
  assert(gfx.has("--- MELODY (Alt+R) ---"));
  auto altH2 = altKey('h');
  display.handleEvent(altH2);

  // Alt+N again on an empty unsaved Melody is harmless: still empty, no error.
  auto altN2 = altKey('n');
  display.handleEvent(altN2);
  assert(engine.currentPhraseBuffer(0).count == 0);

  // Chords: Enter adds a note, H builds the key's triad on it in one press,
  // C cycles its notes and Up edits the chosen one.
  auto key = [](char value) {
    UIEvent e{};
    e.event_type = GROOVEPUTER_KEY_DOWN;
    e.key = value;
    return e;
  };
  auto scan = [](KeyScanCode value) {
    UIEvent e{};
    e.event_type = GROOVEPUTER_KEY_DOWN;
    e.scancode = value;
    return e;
  };
  auto enter = key('\n');
  display.handleEvent(enter);
  const auto& melody = engine.currentPhraseBuffer(0);
  assert(melody.count == 1);
  const uint8_t rootNote = melody.events[0].note;
  auto left = scan(GROOVEPUTER_LEFT);
  display.handleEvent(left);
  auto h1 = key('h');
  display.handleEvent(h1);
  assert(melody.count == 3);
  for (uint16_t i = 0; i < 3; ++i) {
    assert(melody.events[i].startTick == melody.events[0].startTick);
  }
  {
    using namespace GroovePuterRhythm;
    const int third = PhraseChordFocus::chordToneAbove(rootNote, true, 0, kScaleMajor);
    const int fifth = PhraseChordFocus::chordToneAbove(third, false, 0, kScaleMajor);
    assert(melody.events[1].note == third && melody.events[2].note == fifth);
  }
  const int third = melody.events[1].note, fifth = melody.events[2].note;
  assert(RuntimePhraseEdit::hasOverlappingNotes(melody));

  // C from the just-added top note wraps to the lowest; Up raises only it,
  // to the next note of the key (C major: no sharps).
  auto c = key('c');
  display.handleEvent(c);
  auto up = scan(GROOVEPUTER_UP);
  display.handleEvent(up);
  const int rootUp = ProjectKey::step(rootNote, +1, 0, GroovePuterRhythm::kScaleMajor);
  assert(melody.events[0].note == rootUp);
  assert(ProjectKey::inScale(rootUp, 0, GroovePuterRhythm::kScaleMajor));
  assert(melody.events[1].note == third && melody.events[2].note == fifth);
  // C again: the middle note, and Up raises that one.
  auto c2 = key('c');
  display.handleEvent(c2);
  auto up2 = scan(GROOVEPUTER_UP);
  display.handleEvent(up2);
  const int thirdUp = ProjectKey::step(third, +1, 0, GroovePuterRhythm::kScaleMajor);
  assert(melody.events[1].note == thirdUp);
  assert(melody.events[0].note == rootUp && melody.events[2].note == fifth);
  // Ctrl+Up: exactly one semitone, out of the key if asked.
  UIEvent ctrlUp{};
  ctrlUp.event_type = GROOVEPUTER_KEY_DOWN;
  ctrlUp.scancode = GROOVEPUTER_UP;
  ctrlUp.ctrl = true;
  display.handleEvent(ctrlUp);
  assert(melody.events[1].note == thirdUp + 1);
  UIEvent ctrlDown = ctrlUp;
  ctrlDown.scancode = GROOVEPUTER_DOWN;
  display.handleEvent(ctrlDown);
  assert(melody.events[1].note == thirdUp);

  // The key is on screen; K moves its tonic, M its scale, notes stay.
  gfx.texts.clear();
  display.update();
  assert(gfx.has("KEY C MAJ"));
  auto k = key('k');
  display.handleEvent(k);
  auto& params = engine.sceneManager().currentScene().generatorParams;
  assert(params.scaleRoot == 1);
  auto m = key('m');
  display.handleEvent(m);
  assert(params.scale == DORIAN);
  assert(melody.events[1].note == thirdUp);
  gfx.texts.clear();
  display.update();
  assert(gfx.has("KEY C# DOR"));
  // One key for the project: KEYBOARD follows K/M, and its own scale change
  // comes back to the project (and so to the Melody and G).
  assert(keyboard.rootPitchClass() == 1 &&
         keyboard.scale() == PerformanceScale::Dorian);
  keyboard.cycleScale(+1);  // as the KEYBOARD page does: Dorian -> Phrygian
  display.update();
  assert(params.scale == PHRYGIAN && params.scaleRoot == 1);
  params.scaleRoot = 0;
  params.scale = MAJOR;

  // The STEPS letters work here too: A/Z a key note up/down, S/X an octave,
  // Alt+A accent, all on the chosen chord note.
  auto ka = key('a');
  display.handleEvent(ka);
  const int aUp = ProjectKey::step(thirdUp, +1, 0, GroovePuterRhythm::kScaleMajor);
  assert(melody.events[1].note == aUp);
  auto kz = key('z');
  display.handleEvent(kz);
  assert(melody.events[1].note == thirdUp);
  auto ks = key('s');
  display.handleEvent(ks);
  assert(melody.events[1].note == thirdUp + 12);
  auto kx = key('x');
  display.handleEvent(kx);
  assert(melody.events[1].note == thirdUp);
  UIEvent accent{};
  accent.event_type = GROOVEPUTER_KEY_DOWN;
  accent.key = 'a';
  accent.alt = true;
  display.handleEvent(accent);
  assert((melody.events[1].flags & PhraseRuntime::kEventAccent) != 0);
  assert((melody.events[0].flags & PhraseRuntime::kEventAccent) == 0);
  UIEvent accentOff = accent;
  display.handleEvent(accentOff);
  assert((melody.events[1].flags & PhraseRuntime::kEventAccent) == 0);

  // Keyboard recording: keys pressed together land as one chord on the cursor
  // cell; a key played later goes to the next cell.
  auto external = [](uint8_t note, uint8_t velocity) {
    UIEvent e{};
    e.event_type = GROOVEPUTER_APPLICATION_EVENT;
    e.app_event_type = GROOVEPUTER_APP_EVENT_EXTERNAL_NOTE;
    e.x = note;
    e.y = velocity;
    return e;
  };
  auto right = scan(GROOVEPUTER_RIGHT);
  display.handleEvent(right);  // an empty cell after the triad
  auto n60 = external(60, 100);
  assert(display.handleEvent(n60));
  auto n64 = external(64, 90);
  assert(display.handleEvent(n64));
  assert(melody.count == 5);
  const uint16_t chordStart = melody.events[3].startTick;
  assert(chordStart > melody.events[0].startTick);
  assert(melody.events[4].startTick == chordStart && melody.events[4].note == 64);
  assert(melody.events[4].velocity == 90);
  auto n64off = external(64, 0);
  display.handleEvent(n64off);
  delay(PhraseChordFocus::kChordWindowMs + 40);
  auto n67 = external(67, 100);
  assert(display.handleEvent(n67));
  assert(melody.count == 6);
  assert(melody.events[5].note == 67 && melody.events[5].startTick > chordStart);

  // Alt+C: a chord held for four steps becomes four arpeggio notes. H builds
  // the triad, Alt+Right lengthens its top note (the focused one).
  auto right2 = scan(GROOVEPUTER_RIGHT);
  display.handleEvent(right2);
  auto enter2 = key('\n');
  display.handleEvent(enter2);
  assert(melody.count == 7);
  auto h3 = key('h');
  display.handleEvent(h3);
  assert(melody.count == 9);
  for (int i = 0; i < 3; ++i) {
    UIEvent longer{};
    longer.event_type = GROOVEPUTER_KEY_DOWN;
    longer.scancode = GROOVEPUTER_RIGHT;
    longer.alt = true;
    display.handleEvent(longer);
  }
  UIEvent arpKey{};
  arpKey.event_type = GROOVEPUTER_KEY_DOWN;
  arpKey.key = 'c';
  arpKey.alt = true;
  assert(display.handleEvent(arpKey));
  assert(melody.count == 10);  // 9 - 3 chord notes + 4 arpeggio steps
  int arpNotes = 0;
  for (uint16_t i = 0; i < melody.count; ++i) {
    if (melody.events[i].startTick > melody.events[5].startTick) ++arpNotes;
  }
  assert(arpNotes >= 4);

  return 0;
}
