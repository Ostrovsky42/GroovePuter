// MELODY list view: the selected row is the note every edit acts on, also
// when two notes start on the same tick (a cell cannot tell them apart).
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <utility>
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
#include "src/ui/pages/synth_sequencer_page.h"
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

UIEvent keyEvent(char value) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = value;
  return e;
}

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
  e.durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  e.note = note;
  e.velocity = 90;
  e.probability = 100;
}

// Four notes; array order differs from row order, and two start on tick 0.
Buffer fourNotes() {
  Buffer m{};
  m.lengthTicks = PhraseRuntime::kTicksPerBar * 2;
  addNote(m, 96, 72);  // array 0, row 3
  addNote(m, 0, 60);   // array 1, row 0
  addNote(m, 48, 64);  // array 2, row 2
  addNote(m, 0, 67);   // array 3, row 1 (same tick as array 1)
  return m;
}

using Pair = std::pair<int, int>;
std::vector<Pair> pairs(const Buffer& m) {
  std::vector<Pair> v;
  for (uint16_t i = 0; i < m.count; ++i) v.emplace_back(m.events[i].startTick, m.events[i].note);
  std::sort(v.begin(), v.end());
  return v;
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_test_melody_list_edit_target";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("list_target", 0);
  PatternPagingService::setProjectName("list_target");
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
  auto altN = UIEvent{};
  altN.event_type = GROOVEPUTER_KEY_DOWN;
  altN.key = 'n';
  altN.alt = true;
  assert(display.handleEvent(altN));

  const Buffer original = fourNotes();
  // Row order, as the list sorts it: start tick, then array index.
  const uint16_t order[4] = {1, 3, 2, 0};

  auto v = keyEvent('v');
  assert(display.handleEvent(v));  // list view

  for (int row = 0; row < 4; ++row) {
    assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, original));
    // Hold at the top, then walk down to the row.
    for (int i = 0; i < 6; ++i) { auto up = scanEvent(GROOVEPUTER_UP); display.handleEvent(up); }
    for (int i = 0; i < row; ++i) { auto dn = scanEvent(GROOVEPUTER_DOWN); display.handleEvent(dn); }

    const auto before = pairs(engine.currentPhraseBuffer(0));
    auto a = keyEvent('a');  // one note up along the key
    assert(display.handleEvent(a));
    const auto after = pairs(engine.currentPhraseBuffer(0));

    const Pair target{original.events[order[row]].startTick,
                      original.events[order[row]].note};
    auto expected = before;
    bool found = false;
    for (auto& p : expected) {
      if (p == target) {
        p.second += (p.second == 64 || p.second == 71) ? 1 : 2;  // C major steps
        found = true;
        break;
      }
    }
    assert(found);
    std::sort(expected.begin(), expected.end());
    if (after != expected) {
      std::printf("row %d: edited the wrong note\n", row);
      return 1;
    }
  }

  std::puts("MELODY list edit target: PASS");
  return 0;
}
