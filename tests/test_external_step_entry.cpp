// External keyboard step entry on the MELODY notes tab: a played note is written at the cursor with
// its pitch and velocity, the cursor moves on, the sound is auditioned and released with the key.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/ui_common.h"

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
  int textWidth(const char* t) const override { return t ? static_cast<int>(std::strlen(t)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

UIEvent external(uint8_t note, uint8_t velocity) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_APPLICATION_EVENT;
  e.app_event_type = GROOVEPUTER_APP_EVENT_EXTERNAL_NOTE;
  e.x = note;
  e.y = velocity;
  return e;
}
}  // namespace

int main() {
  SceneStorageSdl storage;
  MiniAcid engine(44100.0f, &storage);
  assert(PatternPagingService::setProjectName("external-step-entry"));
  assert(PatternPagingService::clearProjectPages());
  engine.init();
  engine.setSongMode(false);
  NullGfx gfx;
  SynthSequencerPage page(gfx, engine, AudioGuard{}, 0);
  page.onEnter(0);

  // Not a melody source yet (pattern steps): the page declines, PERFORM keeps the note.
  UIEvent declined = external(62, 90);
  assert(!page.handleEvent(declined));

  // Make a melody from a one-note pattern (what Alt+R does), then enter notes with the keyboard.
  {
    Scene& scene = engine.sceneManager().currentScene();
    scene.synthABanks[0].patterns[0].steps[0].note = 60;
    assert(engine.makePhrase(0));
  }
  assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);
  const auto& phrase = engine.currentPhraseBuffer(0);
  const uint16_t before = phrase.count;
  assert(before >= 1);

  UIEvent first = external(62, 90);
  assert(page.handleEvent(first));
  assert(phrase.count == before + 1);
  bool foundFirst = false;
  uint16_t firstTick = 0;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].note == 62) {
      foundFirst = true;
      firstTick = phrase.events[i].startTick;
      assert(phrase.events[i].velocity == 90);        // the key dynamics are kept
    }
  }
  assert(foundFirst);

  // The cursor moved on: the next note lands on a later cell, with its own pitch and velocity.
  // (Step entry releases the key first; keys held together become a chord, 0.9.17.)
  UIEvent firstOff = external(62, 0);
  assert(page.handleEvent(firstOff));
  UIEvent second = external(65, 40);
  assert(page.handleEvent(second));
  assert(phrase.count == before + 2);
  bool foundSecond = false;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].note == 65) {
      foundSecond = true;
      assert(phrase.events[i].startTick > firstTick);
      assert(phrase.events[i].velocity == 40);
    }
  }
  assert(foundSecond);

  // Releasing the key releases the audition; a key the page never auditioned is not its business.
  UIEvent offAudition = external(65, 0);
  assert(page.handleEvent(offAudition));
  UIEvent offUnknown = external(99, 0);
  assert(!page.handleEvent(offUnknown));

  // The pitch buttons move the cursor one cell on this tab (same path as the arrow keys), so the
  // next note lands one cell further / back; elsewhere the page declines and PERFORM takes it.
  {
    UIEvent back{};
    back.event_type = GROOVEPUTER_APPLICATION_EVENT;
    back.app_event_type = GROOVEPUTER_APP_EVENT_EXTERNAL_NUDGE;
    back.x = -1;
    assert(page.handleEvent(back));
    UIEvent forward = back;
    forward.x = 1;
    assert(page.handleEvent(forward));
    engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
    assert(!page.handleEvent(back));              // not a melody voice: PERFORM keeps the nudge
  }

  // Mod deletes the sound under the cursor (Backspace path, so Undo applies); nothing there: no-op.
  {
    UIEvent mod{};
    mod.event_type = GROOVEPUTER_APPLICATION_EVENT;
    mod.app_event_type = GROOVEPUTER_APP_EVENT_EXTERNAL_MOD;
    engine.makePhrase(0);                                   // already a melody: stays as is
    const uint16_t beforeDelete = engine.currentPhraseBuffer(0).count;
    UIEvent enter = external(70, 100);                      // one more sound so the cursor sits on it
    assert(page.handleEvent(enter));
    UIEvent enterOff = external(70, 0);
    assert(page.handleEvent(enterOff));
    assert(engine.currentPhraseBuffer(0).count == beforeDelete + 1);
    assert(page.handleEvent(mod));
    assert(engine.currentPhraseBuffer(0).count == beforeDelete);   // the sound under the cursor is gone
    engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
    assert(!page.handleEvent(mod));                         // not a melody voice: the page declines
    engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  }

  // Clearing the whole melody: the external gesture (CLEAR event) and Ctrl+Backspace both remove
  // every sound and keep the length; an empty melody is a harmless no-op; Undo is one step.
  {
    UIEvent clear{};
    clear.event_type = GROOVEPUTER_APPLICATION_EVENT;
    clear.app_event_type = GROOVEPUTER_APP_EVENT_EXTERNAL_CLEAR;
    UIEvent a = external(60, 100);
    UIEvent aOff = external(60, 0);
    assert(page.handleEvent(a));
    assert(page.handleEvent(aOff));
    assert(engine.currentPhraseBuffer(0).count >= 1);
    const uint16_t lengthBefore = engine.currentPhraseBuffer(0).lengthTicks;
    assert(page.handleEvent(clear));
    assert(engine.currentPhraseBuffer(0).count == 0);
    assert(engine.currentPhraseBuffer(0).lengthTicks == lengthBefore);
    assert(page.handleEvent(clear));                         // empty: toast only, still consumed
    assert(engine.currentPhraseBuffer(0).count == 0);

    // Undo restores everything in one step.
    UIEvent undo{};
    undo.event_type = GROOVEPUTER_APPLICATION_EVENT;   // what Ctrl+Z becomes in the display
    undo.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
    assert(page.handleEvent(undo));
    assert(engine.currentPhraseBuffer(0).count >= 1);

    // Ctrl+Backspace on the Cardputer keyboard does the same.
    UIEvent ctrlBackspace{};
    ctrlBackspace.event_type = GROOVEPUTER_KEY_DOWN;
    ctrlBackspace.key = '\b';
    ctrlBackspace.ctrl = true;
    assert(page.handleEvent(ctrlBackspace));
    assert(engine.currentPhraseBuffer(0).count == 0);

    engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
    assert(!page.handleEvent(clear));                        // not a melody voice
  }

  std::puts("external keyboard step entry on the MELODY notes tab: PASS");
  return 0;
}
