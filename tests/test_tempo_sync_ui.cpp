#include <cassert>
#include <cstdlib>
#include "platform_sdl/sdl_display.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "src/ui/global_midi_sync_overlay.h"
#include "src/ui/pages/project_page.h"
#include "src/ui/miniacid_display.h"
#include "src/dsp/miniacid_engine.h"
#include "src/input/performance_keyboard.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/ui/ui_theme.h"
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
    if (s && *s) {
      assert(x >= 0 && y >= 0 && x + textWidth(s) <= width() && y + fontHeight() <= height());
      texts.push_back({x, y, s});
    }
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
int main() {
  using namespace GroovePuterMidi;
  MiniAcid engine(44100.f, nullptr);
  RecordingGfx gfx;
  auto& clock=transportClockRuntime();
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx,engine,keyboard);
  display.dismissSplash();

  // Device settings must not mutate the saved musical profile.
  ProjectPage project(gfx,engine,AudioGuard{});
  engine.setGrooveboxMode(GrooveboxMode::Dub);
  engine.setGrooveFlavor(3);
  const auto savedMode=engine.grooveboxMode();
  const auto savedFlavor=engine.grooveFlavor();
  auto tab=key('\t'); project.handleEvent(tab);
  gfx.texts.clear(); project.draw(gfx);
  assert(gfx.has("DEVICE") && gfx.has("Theme") && gfx.has("Main Volume"));
  assert(!gfx.has("Flavor") && !gfx.has("Groove"));
  for (int i=0;i<6;++i) {
    auto down=scan(GROOVEPUTER_DOWN); project.handleEvent(down);
    auto right=scan(GROOVEPUTER_RIGHT); project.handleEvent(right);
  }
  assert(engine.grooveboxMode()==savedMode && engine.grooveFlavor()==savedFlavor);

  // Alt+T no longer opens a transport panel; it must not start playback.
  auto altT=key('t'); altT.alt=true;
  display.handleEvent(altT);
  assert(!engine.isPlaying());

  // Following MIDI Clock: local Space must not start the engine (start() sends
  // all-notes-off and would run a transport the clock source does not own).
  clock.setSource(TransportClockSource::SeqtrakExternal);
  clock.setExternalFollowEnabled(true);
  assert(externalClockOwnsTransport());
  display.handleEvent(key(' '));
  assert(!engine.isPlaying());
  // MIDI IN with follow off runs locally, so Space plays/stops as usual.
  clock.setExternalFollowEnabled(false);
  assert(!externalClockOwnsTransport());
  display.handleEvent(key(' ')); assert(engine.isPlaying());
  display.handleEvent(key(' ')); assert(!engine.isPlaying());
  clock.setExternalFollowEnabled(true);
  clock.setSource(TransportClockSource::GroovePuterInternal);
  display.handleEvent(key(' ')); assert(engine.isPlaying());
  display.handleEvent(key(' ')); assert(!engine.isPlaying());

  // BPM is focused on open: Left/Right edits it immediately.
  GlobalMidiSyncOverlay sync;
  clock.setSource(TransportClockSource::GroovePuterInternal);
  auto down=scan(GROOVEPUTER_DOWN), up=scan(GROOVEPUTER_UP);
  auto right=scan(GROOVEPUTER_RIGHT), left=scan(GROOVEPUTER_LEFT);
  sync.open(); sync.handleEvent(right);
  assert(sync.takeTempoDelta()==1 && sync.takeTempoDelta()==0);
  right.alt=true; sync.handleEvent(right); assert(sync.takeTempoDelta()==5);
  left.alt=true; sync.handleEvent(left); assert(sync.takeTempoDelta()==-5);
  right.alt=left.alt=false;
  auto shiftRight=right; shiftRight.shift=true; sync.handleEvent(shiftRight);
  assert(sync.takeTempoDelta()==1); // Shift is not a coarse step any more
  gfx.texts.clear(); sync.draw(gfx,engine);
  assert(gfx.has("TEMPO") && gfx.has("INTERNAL") && gfx.has("BPM"));

  // Clock row: Right = MIDI IN (always following), Left = INTERNAL.
  clock.setExternalFollowEnabled(false);
  sync.handleEvent(down); sync.handleEvent(right);
  assert(clock.source()==TransportClockSource::SeqtrakExternal && clock.externalFollowEnabled());
  assert(sync.takeTempoDelta()==0);
  sync.handleEvent(left);
  assert(clock.source()==TransportClockSource::GroovePuterInternal);
  auto enter=key('\n'); sync.handleEvent(enter);
  assert(clock.source()==TransportClockSource::SeqtrakExternal);
  sync.handleEvent(enter);
  assert(clock.source()==TransportClockSource::GroovePuterInternal);

  // Y switches the clock source from the BPM row as well.
  sync.open();
  auto yKey=key('y'); sync.handleEvent(yKey);
  assert(clock.source()==TransportClockSource::SeqtrakExternal && clock.externalFollowEnabled());
  auto bigY=key('Y'); sync.handleEvent(bigY);
  assert(clock.source()==TransportClockSource::GroovePuterInternal);
  assert(sync.isVisible() && sync.takeTempoDelta()==0);
  // A held arrow ramps up through the accelerator.
  UIInput::HoldAccelerator ramp; int fast=1;
  for (int i=0;i<30;++i) fast=ramp.multiplierAt(1, 1000u+i*80u);
  assert(fast==4);

  // Through the display owner: guarded engine path and limits.
  auto altY=key('y'); altY.alt=true;
  engine.setBpm(120); engine.setExternalClockBpm(50);
  display.handleEvent(altY);
  display.handleEvent(right); assert(engine.bpm()==121);
  right.alt=true; display.handleEvent(right); assert(engine.bpm()==126);
  engine.setBpm(250); display.handleEvent(right); assert(engine.bpm()==250);
  left.alt=true; engine.setBpm(10); display.handleEvent(left); assert(engine.bpm()==10);
  right.alt=left.alt=false;
  // Following MIDI Clock: BPM is read-only.
  clock.setSource(TransportClockSource::SeqtrakExternal);
  clock.setExternalFollowEnabled(true);
  display.handleEvent(right); assert(engine.bpm()==10);
  // Legacy MIDI IN + follow off runs on project BPM, so it stays editable.
  clock.setExternalFollowEnabled(false);
  display.handleEvent(right); assert(engine.bpm()==11);
  display.handleEvent(scan(GROOVEPUTER_ESCAPE));
  clock.setExternalFollowEnabled(true);

  // MIDI IN states. The panel is device-neutral: no SEQTRAK wording anywhere.
  sync.open();
  ExternalClockEstimate estimate{};
  auto render=[&]() { gfx.texts.clear(); sync.draw(gfx,engine); assert(!gfx.has("SEQ")); };
  estimate.state=ExternalClockLockState::Waiting; clock.publishExternalEstimate(estimate,0);
  render(); assert(gfx.has("WAITING") && gfx.has("MIDI IN"));
  estimate.state=ExternalClockLockState::Locked; estimate.validTempo=true;
  estimate.sourceBpmQ16=50u*65536u; clock.publishExternalEstimate(estimate,0);
  render(); assert(gfx.has("IN SYNC") && gfx.has("BPM IN"));
  estimate.state=ExternalClockLockState::Lost; clock.publishExternalEstimate(estimate,0);
  render(); assert(gfx.has("LAST BPM") && gfx.has("CLOCK LOST"));
  clock.setExternalFollowEnabled(false);
  render(); assert(gfx.has("FOLLOW OFF"));
  clock.setExternalFollowEnabled(true);
  clock.setSource(TransportClockSource::GroovePuterInternal);
  for (uint8_t row : {0,1}) { sync.open(); if (row) sync.handleEvent(down); render(); }
  (void)up;

  if (const char* directory = std::getenv("TEMPO_SYNC_RENDER_DIR")) {
    SDLDisplay screen(240,135,"TEMPO render verification");
    screen.begin();
    auto* window=SDL_GetWindowFromID(1);
    assert(window);
    auto* renderer=SDL_GetRenderer(window);
    assert(renderer);
    auto save=[&](const char* name) {
      screen.startWrite(); sync.draw(screen,engine); screen.endWrite();
      auto* surface=SDL_CreateRGBSurfaceWithFormat(0,240,135,32,SDL_PIXELFORMAT_RGBA32);
      assert(surface);
      assert(SDL_RenderReadPixels(renderer,nullptr,surface->format->format,surface->pixels,surface->pitch)==0);
      const auto path=std::string(directory)+"/"+name+".bmp";
      assert(SDL_SaveBMP(surface,path.c_str())==0); SDL_FreeSurface(surface);
    };
    clock.setSource(TransportClockSource::GroovePuterInternal);
    engine.setBpm(120); sync.open(); save("tempo-internal");
    sync.handleEvent(down); save("tempo-clock-row");
    clock.setSource(TransportClockSource::SeqtrakExternal);
    estimate.state=ExternalClockLockState::Waiting; estimate.validTempo=false;
    clock.publishExternalEstimate(estimate,0); save("midi-in-waiting");
    estimate.state=ExternalClockLockState::Locked; estimate.validTempo=true;
    clock.publishExternalEstimate(estimate,0); save("midi-in-locked");
    estimate.state=ExternalClockLockState::Lost; clock.publishExternalEstimate(estimate,0);
    save("midi-in-lost");
  }
  puts("TEMPO: BPM-first edit + limits + MIDI IN read-only + device-neutral states + DEVICE preservation PASS");
}
