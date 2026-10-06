#include <cassert>
#include <cstdlib>
#include "platform_sdl/sdl_display.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "src/ui/play_rec_overlay.h"
#include "src/ui/miniacid_display.h"
#include "src/dsp/miniacid_engine.h"
#include "src/input/performance_keyboard.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/midi/smf_player_service.h"
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


class Player : public GroovePuterMidi::ISmfPlayerService {
public:
  GroovePuterMidi::SmfPlayerSnapshot value{};
  int toggles=0, pauses=0;
  bool requestLoad(const char*) override { return true; }
  bool togglePlayPause() override { ++toggles; return true; }
  bool pause() override { ++pauses; return true; }
  bool restart(GroovePuterMidi::SmfPlayerRestartOrigin) override { return true; }
  bool stop() override { return true; }
  bool panic() override { return true; }
  bool seekBars(int) override { return true; }
  bool toggleRouting() override { return true; }
  bool toggleTempoMode() override { return true; }
  bool adjustTempoBpm(int) override { return true; }
  bool resetTempo() override { return true; }
  bool cycleVelocityBoost() override { return true; }
  GroovePuterMidi::SmfPlayerSnapshot snapshot() const override { return value; }
  GroovePuterMidi::SmfChannelInspectorSnapshot channelInspector() const override { return {}; }
};
int main() {
  using namespace GroovePuterMidi;
  MiniAcid engine(44100.f, nullptr);
  RecordingGfx gfx;
  PlayRecOverlay panel;
  auto& clock=transportClockRuntime();
  clock.setSource(TransportClockSource::GroovePuterInternal);
  panel.open(false); panel.draw(gfx,engine);
  assert(gfx.has("STOPPED") && gfx.has("SPACE: PLAY"));
  assert(gfx.has("MANUAL ON SEQTRAK"));
  assert(!engine.isPlaying());
  panel.handleEvent(scan(GROOVEPUTER_RIGHT));
  assert(panel.takeAction()==PlayRecOverlay::Action::None);
  panel.handleEvent(key(' '));
  assert(panel.takeAction()==PlayRecOverlay::Action::MidiTransport);
  assert(panel.takeAction()==PlayRecOverlay::Action::None);
  panel.handleEvent(key('y'));
  assert(panel.takeAction()==PlayRecOverlay::Action::Sync);
  panel.close(); assert(!panel.isVisible());

  Player player; registerSmfPlayerService(&player);
  for (auto source : {TransportClockSource::GroovePuterInternal, TransportClockSource::SeqtrakExternal}) {
    clock.setSource(source);
    for (bool follow : {false,true}) {
      clock.setExternalFollowEnabled(follow);
      for (auto state : {SmfPlayerState::Unloaded, SmfPlayerState::Loading, SmfPlayerState::Stopped,
                        SmfPlayerState::Armed, SmfPlayerState::Playing, SmfPlayerState::Paused, SmfPlayerState::Error}) {
        player.value.state=state;
        for (auto mode : {SmfTempoMode::Original, SmfTempoMode::Project}) {
          player.value.tempoMode=mode;
          for (bool midi : {false,true}) {
            panel.open(midi); gfx.texts.clear(); panel.draw(gfx,engine);
          }
        }
      }
    }
  }
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  MiniAcidDisplay display(gfx,engine,keyboard);
  display.dismissSplash();
  auto shortcut=key('t'); shortcut.alt=true;
  clock.setSource(TransportClockSource::GroovePuterInternal);
  display.handleEvent(shortcut);
  assert(!engine.isPlaying());
  display.handleEvent(key(' ')); assert(engine.isPlaying());
  display.handleEvent(scan(GROOVEPUTER_RIGHT));
  player.value.state=SmfPlayerState::Stopped; player.value.tempoMode=SmfTempoMode::Original;
  display.handleEvent(key(' '));
  assert(player.toggles==1 && engine.isPlaying()); // MIDI does not toggle the Groove engine.
  display.handleEvent(scan(GROOVEPUTER_LEFT));
  display.handleEvent(key(' ')); assert(!engine.isPlaying());
  clock.setSource(TransportClockSource::SeqtrakExternal);
  clock.setExternalFollowEnabled(false);
  display.handleEvent(key(' '));
  assert(clock.externalFollowEnabled() && !engine.isPlaying()); // waits for SEQ Start
  display.handleEvent(key(' ')); assert(!clock.externalFollowEnabled());
  display.handleEvent(scan(GROOVEPUTER_ESCAPE));
  if (const char* directory = std::getenv("PLAY_REC_RENDER_DIR")) {
    SDLDisplay screen(240,135,"PLAY / REC render verification");
    screen.begin();
    auto* window=SDL_GetWindowFromID(1);
    assert(window);
    auto* renderer=SDL_GetRenderer(window);
    assert(renderer);
    auto save=[&](const char* name) {
      screen.startWrite(); panel.draw(screen,engine); screen.endWrite();
      auto* surface=SDL_CreateRGBSurfaceWithFormat(0,240,135,32,SDL_PIXELFORMAT_RGBA32);
      assert(surface);
      assert(SDL_RenderReadPixels(renderer,nullptr,surface->format->format,surface->pixels,surface->pitch)==0);
      const auto path=std::string(directory)+"/"+name+".bmp";
      assert(SDL_SaveBMP(surface,path.c_str())==0);
      SDL_FreeSurface(surface);
    };
    clock.setSource(TransportClockSource::GroovePuterInternal);
    panel.open(false); save("groove-stopped");
    clock.setSource(TransportClockSource::SeqtrakExternal);
    clock.setExternalFollowEnabled(true); save("seq-follow");
    clock.setExternalFollowEnabled(false); save("seq-follow-off");
    panel.open(true); player.value.state=SmfPlayerState::Armed;
    player.value.tempoMode=SmfTempoMode::Project;
    clock.setExternalFollowEnabled(true); save("midi-armed");
  }
  registerSmfPlayerService(nullptr);
  puts("PLAY/REC: routing, safe selection, external follow, 112 render states PASS");
}
