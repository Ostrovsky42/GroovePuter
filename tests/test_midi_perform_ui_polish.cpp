#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/platform/cardputer_midi_settings_session.h"
#include "src/platform/cardputer_usb_role_runtime.h"
#include "src/input/performance_keyboard.h"
#include "src/ui/pages/project_page.h"
#include "src/ui/pages/perform_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

class RecordingGfx : public IGfx {
 public:
  struct Text { int x, y; std::string value; };
  std::vector<Text> texts;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int x, int y, const char* s) override {
    if (s && *s) texts.push_back({x, y, s});
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
  bool has(const char* value) const {
    for (const auto& t : texts) if (t.value.find(value) != std::string::npos) return true;
    return false;
  }
  void bodyFits() const {
    for (const auto& t : texts) {
      if (t.y < 16 || t.y >= 119) continue;
      assert(t.y + 8 <= 109);
      assert(t.x >= 0 && t.x + static_cast<int>(t.value.size()) * 6 <= 240);
    }
  }
};

UIEvent key(char value) {
  UIEvent e{}; e.event_type = GROOVEPUTER_KEY_DOWN; e.key = value; return e;
}
UIEvent scan(KeyScanCode value) {
  UIEvent e{}; e.event_type = GROOVEPUTER_KEY_DOWN; e.scancode = value; return e;
}

class Events : public IMusicalEventSink {
 public:
  std::vector<MusicalEvent> notes;
  void handleMusicalEvent(const MusicalEvent& e) override { notes.push_back(e); }
};

int main() {
  MiniAcid engine(44100.f, nullptr);
  RecordingGfx gfx;
  ProjectPage project(gfx, engine, AudioGuard{});
  project.onEnter(0);
  auto projectSend = [&](UIEvent e) { project.handleEvent(e); };
  for (int i = 0; i < 3; ++i) projectSend(key('\t'));
  project.draw(gfx);
  // A role must be readable before applying it, not hidden behind an ellipsis.
  assert(gfx.has("COMPUTER"));
  gfx.bodyFits();
  projectSend(scan(GROOVEPUTER_RIGHT));
  projectSend(key('\n'));
  gfx.texts.clear(); project.draw(gfx);
  assert(gfx.has("KEYBOARD"));
  assert(gfx.has("RESTART"));
  assert(!gfx.has("ACTIVE"));
  gfx.bodyFits();

  project.onEnter(0);
  projectSend(key('\t')); projectSend(key('\t'));
  engine.sceneManager().currentScene().led.brightness = 255;
  gfx.texts.clear(); project.draw(gfx);
  assert(gfx.has("100%"));
  gfx.bodyFits();

  MusicalEventRouter router;
  Events events;
  router.addSink(events);
  PerformanceKeyboard keyboard(router);
  PerformPage perform(gfx, engine, keyboard);
  auto send = [&](UIEvent e) { perform.handleEvent(e); };
  for (int i = 0; i < 5; ++i) send(key('\t'));
  gfx.texts.clear(); perform.drawContent(gfx);
  assert(gfx.has("[IN]"));
  gfx.bodyFits();
  // PERFORM edits the same persistent input configuration as PROJECT.
  send(scan(GROOVEPUTER_RIGHT));
  assert(GroovePuterPlatform::cardputerMidiInputRoutingConfig().enabled);
  send(scan(GROOVEPUTER_DOWN)); send(scan(GROOVEPUTER_DOWN));
  for (int i = 0; i < 3; ++i) send(scan(GROOVEPUTER_RIGHT));
  assert(GroovePuterPlatform::cardputerMidiInputRoutingConfig().target ==
         GroovePuterMidi::MidiInputTarget::Perform);
  gfx.texts.clear(); perform.drawContent(gfx);
  assert(gfx.has("PERFORM"));
  gfx.bodyFits();

  send(scan(GROOVEPUTER_DOWN));  // IN: ROUTE -> TARGET
  assert(keyboard.externalNoteOn(60, 100));
  events.notes.clear();
  send(scan(GROOVEPUTER_RIGHT));  // TARGET A -> B, clears old held notes
  assert(keyboard.target() == MusicalEventTarget::SynthB);
  assert(keyboard.heldCount() == 0);
  bool cleanupA = false;
  for (const auto& e : events.notes)
    if (e.target == MusicalEventTarget::SynthA &&
        (e.type == MusicalEventType::NoteOff || e.type == MusicalEventType::AllNotesOff)) cleanupA = true;
  assert(cleanupA);
  events.notes.clear();
  assert(keyboard.externalNoteOn(64, 100));
  bool startedB = false;
  for (const auto& e : events.notes)
    if (e.target == MusicalEventTarget::SynthB && e.type == MusicalEventType::NoteOn) startedB = true;
  assert(startedB);
  send(scan(GROOVEPUTER_RIGHT)); // B -> DX
  assert(keyboard.target() == MusicalEventTarget::Dx);
  assert(keyboard.heldCount() == 0);
  events.notes.clear();
  assert(keyboard.externalNoteOn(67, 100));
  bool startedDx = false;
  for (const auto& e : events.notes)
    if (e.target == MusicalEventTarget::Dx && e.type == MusicalEventType::NoteOn) startedDx = true;
  assert(startedDx);
  // Direct routes stay independent: TARGET cannot silently retarget their notes.
  send(scan(GROOVEPUTER_UP)); // TARGET -> ROUTE
  send(scan(GROOVEPUTER_RIGHT)); // PERFORM -> direct SYN A
  assert(GroovePuterPlatform::cardputerMidiInputRoutingConfig().target ==
         GroovePuterMidi::MidiInputTarget::SynthA);
  send(scan(GROOVEPUTER_DOWN));
  send(scan(GROOVEPUTER_RIGHT));
  assert(keyboard.target() == MusicalEventTarget::Dx);
  gfx.texts.clear(); perform.drawContent(gfx);
  assert(gfx.has("N/A"));
  gfx.bodyFits();
  std::puts("MIDI/PERFORM UI polish: PASS");
}
