// SONG page Opt on a cell: refuses an empty or Pattern cell with the Song
// unchanged; on a Melody cell it flips only that cell's bit and toasts.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/ui/pages/song_page.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/state/undo_receipts.h"
#include "src/ui/ui_common.h"
#include "song_cell_source_common.h"

SerialMock Serial;
SDMock SD;

namespace {
class RecordingGfx : public IGfx {
 public:
  std::vector<std::string> texts;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* text) override { if (text) texts.push_back(text); }
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
  int textWidth(const char* t) const override { return t ? (int)std::strlen(t) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

std::string toastAfterOpt(SongPage& page) {
  UIEvent e;
  e.event_type = GROOVEPUTER_APPLICATION_EVENT;
  e.app_event_type = GROOVEPUTER_APP_EVENT_TOGGLE_SOURCE;
  UI::showToast("", 0);  // clear
  assert(page.handleEvent(e));
  RecordingGfx gfx;
  UI::drawToast(gfx);
  return gfx.texts.empty() ? std::string() : gfx.texts.back();
}
}  // namespace

int main() {
  using namespace cellsrc;
  const std::string proj = "cell_src_opt";
  setupProject("gp_test_cell_src_opt", proj);
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  constexpr int kX = 0, kY = 1;

  MiniAcid engine{44100.0f, &storage};
  engine.init();
  engine.setSongMode(false);
  writePattern(engine, kX, 40);
  bindPattern(engine, kX);
  acceptMelody(engine, kY, makeMelody(60));
  engine.set303PatternIndex(0, kX);
  engine.releaseSavedWorkingMelody_(0);
  assert(engine.rebuildPatternRuntimeEventBank());
  // rows: 0 = Pattern X, 1 = empty, 2 = empty, 3 = empty, 4 = Melody Y, 5 = Melody Y
  engine.sceneManager().currentScene().songs[engine.sceneManager().activeSongSlot()] = Song{};
  engine.setSongPattern(0, SongTrack::SynthA, kX);
  engine.setSongPattern(4, SongTrack::SynthA, kY);
  engine.setSongPattern(5, SongTrack::SynthA, kY);
  engine.setSongLength(6);

  RecordingGfx gfx;
  SongPage page(gfx, engine, AudioGuard{});
  page.onEnter(0);
  bool valid = false;
  page.cursor_track_ = 0;
  assert(page.trackForColumn(page.cursor_track_, valid) == SongTrack::SynthA && valid);

  auto song = [&]() -> const Song& {
    return engine.sceneManager().currentScene().songs[engine.sceneManager().activeSongSlot()];
  };
  const Song before = song();

  page.cursor_row_ = 1;  // empty cell
  assert(toastAfterOpt(page) == "A: EMPTY CELL");
  assert(GroovePuterUndo::sameSong(before, song()));

  page.cursor_row_ = 0;  // Pattern cell
  assert(toastAfterOpt(page) == "A A1: NO MELODY (ALT+N ON STEPS)");
  assert(GroovePuterUndo::sameSong(before, song()));

  page.cursor_row_ = 4;  // Melody cell -> STEPS
  assert(toastAfterOpt(page) == "ROW 05 A: STEPS");
  assert(song().stepsOverrideAt(0, 4));
  for (int r = 0; r < Song::kMaxPositions; ++r) {
    assert(song().stepsOverrideAt(1, r) == false);
    if (r != 4) assert(song().stepsOverrideAt(0, r) == false);
  }
  assert(engine.songCellMaterialKind(0, kY, 4) == MiniAcid::SongCellMaterialKind::Pattern);
  assert(engine.songCellMaterialKind(0, kY, 5) == MiniAcid::SongCellMaterialKind::Melody);

  assert(toastAfterOpt(page) == "ROW 05 A: MELODY");  // flips back
  assert(GroovePuterUndo::sameSong(before, song()));

  assert(toastAfterOpt(page) == "ROW 05 A: STEPS");
  // Undo restores the Song.
  assert(page.undoPreparedSongState());
  assert(GroovePuterUndo::sameSong(before, song()));

  std::puts("test_song_cell_source_opt PASS");
  return 0;
}
