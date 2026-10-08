// 0.9.14 PML-E: MATERIAL -> ALLOW REPLACEMENT (UI). Real PhrasePage, real engine, real events.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/dsp/slot_reuse.h"
#include "src/state/generation_request_state.h"
#include "src/state/phrase_generation_request_state.h"
#include "src/ui/pages/phrase_page.h"
#include "src/ui/ui_common.h"
#include "src/ui/ui_shell_frame.h"

SerialMock Serial;
SDMock SD;

namespace {

class UiGfx : public IGfx {
 public:
  std::vector<std::string> labels;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* value) override { labels.emplace_back(value ? value : ""); }
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
  int textWidth(const char* value) const override { return value ? static_cast<int>(std::strlen(value)) : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
  bool shows(const char* phrase) const {
    for (const auto& label : labels) if (label.find(phrase) != std::string::npos) return true;
    return false;
  }
  int count(const char* exact) const {
    int n = 0;
    for (const auto& label : labels) n += label == exact ? 1 : 0;
    return n;
  }
};

namespace R = GroovePuterRhythm;
using GeneratedPhraseSong::CycleStatus;
using GeneratedPhraseSong::LifecycleStatus;
using SlotReuse::MarkResult;

const auto kGuard = [](auto&& body) { body(); };

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__,  \
                   #cond);                                                   \
      std::abort();                                                          \
    }                                                                        \
  } while (0)

struct Fixture {
  SceneStorageSdl storage;
  MiniAcid engine{44100.0f, &storage};
  Fixture(const char* project, GenerativeMode mode, uint16_t archetype) {
    CHECK(PatternPagingService::setProjectName(project));
    CHECK(PatternPagingService::clearProjectPages());
    engine.init();
    engine.setSongMode(false);
    Scene& scene = engine.sceneManager().currentScene();
    scene.genre.generativeMode = static_cast<uint8_t>(mode);
    scene.genre.recipe = 0;
    scene.genre.rhythmSelectionMode = static_cast<uint8_t>(
        archetype ? R::RhythmSelectionMode::Manual : R::RhythmSelectionMode::Auto);
    scene.genre.rhythmArchetypeId = archetype;
    scene.activeSongSlot = 0;
    scene.songs[0] = Song{};
    scene.songs[1] = Song{};
    scene.feel.patternBars = 1;
    for (int b = 0; b < kBankCount; ++b)
      for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
        scene.synthABanks[b].patterns[i] = SynthPattern{};
        scene.synthBBanks[b].patterns[i] = SynthPattern{};
        scene.drumBanks[b].patterns[i] = DrumPatternSet{};
      }
    for (int v = 0; v < Scene::kMaterialVoices; ++v)
      for (int s = 0; s < Scene::kMaterialSlotsPerVoice; ++s)
        scene.materialSlots[v][s] = GroovePuterMaterial::MaterialSlotDescriptor{};
    engine.genreManager().setGenerativeMode(mode);
    engine.genreManager().setRecipe(0);
    engine.setBpm(124.0f);
    GroovePuterState::setGenerationLevel(R::RealizationLevel::P3Transformation);
    GroovePuterState::setRequestedPhraseBars(4);
    for (int i = 0; i < 3; ++i) engine.sceneManager().setCurrentBankIndex(i, 1);
  }
  ~Fixture() { GroovePuterState::setGenerationLevel(R::RealizationLevel::P2Variation); }
  Scene& scene() { return engine.sceneManager().currentScene(); }
  Song& song() { return scene().songs[0]; }
  bool take(int bars, int row) {
    return GeneratedPhraseSong::generate(engine, static_cast<uint8_t>(bars), row, kGuard).status ==
           LifecycleStatus::CommittedNow;
  }
  void clearRows() {
    for (int r = 0; r < Song::kMaxPositions; ++r)
      for (int t = 0; t < SongPosition::kTrackCount; ++t) song().positions[r].patterns[t] = -1;
    song().length = 1;
  }
};

// Four 4B TAKEs fill the page; the rows are deleted. Slots 0-11 are unreferenced, slots 12-15 belong
// to the live Undo receipt, slot 8 is also CURRENT (bank 1 selected, pattern 0).
void buildOrphans(Fixture& f) {
  for (int i = 0; i < 4; ++i) {
    CHECK(f.take(4, i * 4));
    if (i < 3) f.clearRows();
  }
  f.clearRows();
}

UIEvent key(char c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = c;
  return e;
}
UIEvent arrow(int scancode) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = static_cast<KeyScanCode>(scancode);
  return e;
}

bool press(PhrasePage& page, UIEvent event) { return page.handleEvent(event); }

// Esc as the emulator and the device normalisation deliver it: a scancode, key 0 (not key 0x1B).
UIEvent escapeScancode() {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = GROOVEPUTER_ESCAPE;
  return e;
}

std::string toastText(PhrasePage& page, UiGfx& gfx) {
  gfx.labels.clear();
  UI::drawToast(gfx);
  (void)page;
  return gfx.labels.empty() ? std::string() : gfx.labels.back();
}

std::string g_footerLeft, g_footerRight;

// The footer is published into the shell frame model, not drawn by the page: bind one, as the shell does.
void drawView(PhrasePage& page, UiGfx& gfx) {
  gfx.labels.clear();
  UI::UiShellFrameModel model;
  UI::beginShellFrameModel(model);
  page.draw(gfx);
  UI::endShellFrameModel();
  g_footerLeft = model.footer.left;
  g_footerRight = model.footer.right;
}

// R opens the MAKE ROOM question; S on it opens the slot-by-slot grid.
void openGrid(PhrasePage& page) {
  CHECK(press(page, key('r')));
  CHECK(press(page, key('s')));
}

void testOpenAndKeys() {
  Fixture f("pmle-keys", GenerativeMode::Techno, 404);
  UiGfx gfx;
  PhrasePage page(gfx, f.engine, AudioGuard{}, false);
  drawView(page, gfx);
  CHECK(g_footerRight.find("R:ROOM") != std::string::npos);      // the footer names the action

  UIEvent alt = key('r'); alt.alt = true;              // Alt+R belongs to the global handlers
  CHECK(!page.handleEvent(alt));
  UIEvent ctrl = key('r'); ctrl.ctrl = true;
  CHECK(!page.handleEvent(ctrl));
  UIEvent meta = key('r'); meta.meta = true;
  CHECK(!page.handleEvent(meta));
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH") && !gfx.shows("NOTHING TO REUSE"));   // none of them opened the view

  CHECK(press(page, key('r')));                   // plain R opens the question
  drawView(page, gfx);
  CHECK(gfx.shows("NOTHING TO REUSE") && !gfx.shows("LENGTH"));   // fresh page: nothing generated here
  CHECK(g_footerLeft.find("[S]SLOTS") != std::string::npos);
  CHECK(press(page, key('R')));                   // and plain R leaves it
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH") && !gfx.shows("NOTHING TO REUSE"));
  CHECK(press(page, key('r')));
  CHECK(press(page, key(0x1B)));                  // ESC (key 0x1B) leaves it too
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH") && !gfx.shows("NOTHING TO REUSE"));
  CHECK(press(page, key('r')));
  CHECK(press(page, key('S')));                   // S opens the slot-by-slot grid
  drawView(page, gfx);
  CHECK(gfx.shows("SLOT SPACE") && !gfx.shows("LENGTH"));
  CHECK(g_footerLeft.find("[ENTER]ALLOW") != std::string::npos);
  CHECK(press(page, escapeScancode()));           // Esc as a scancode leaves the grid
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH") && !gfx.shows("SLOT SPACE"));
  CHECK(press(page, key('r')));
  CHECK(press(page, escapeScancode()));           // and the question
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH") && !gfx.shows("NOTHING TO REUSE"));
  CHECK(press(page, key('r')));
  CHECK(press(page, key('s')));
  CHECK(press(page, key('R')));                   // R leaves the grid
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH"));
  std::puts("PML-E: plain R opens/leaves MAKE ROOM, S opens the grid; Alt/Ctrl/Meta+R do not; footer names R: PASS");
}

void testGridHoldersAndResults() {
  Fixture f("pmle-grid", GenerativeMode::Techno, 404);
  buildOrphans(f);
  UiGfx gfx;
  PhrasePage page(gfx, f.engine, AudioGuard{}, false);

  // product view: no run, nothing allowed yet
  drawView(page, gfx);
  CHECK(gfx.shows("NO SLOTS: R REUSE 12"));             // the product view says what R can do

  openGrid(page);
  drawView(page, gfx);
  CHECK(gfx.count("C") == 1);                          // slot 13: CURRENT (first slot of the last TAKE)
  CHECK(gfx.count("U") == 3);                          // slots 14-16: the rest of the live Undo receipt
  CHECK(gfx.count("~") == 12);                         // slots 1-12: unused orphans that may be allowed
  CHECK(gfx.count("*") == 0);
  CHECK(gfx.shows("SLOT SPACE"));                     // the results are labelled as space, not as a GROW promise
  CHECK(gfx.shows("TAKE 4B  NO / NO"));
  CHECK(gfx.shows("GROW 4B  NO / NO     8B  NO / NO"));
  CHECK(gfx.shows("BLOCKED BY"));
  CHECK(gfx.shows("LIVE UNDO 3"));
  CHECK(gfx.shows("SLOT 1: UNUSED  ENTER: ALLOW"));
  std::puts("PML-E: grid shows holders (Undo, CURRENT) and unused slots; results start at NO/NO: PASS");
}

void testConfirmAndMark() {
  Fixture f("pmle-mark", GenerativeMode::Techno, 404);
  buildOrphans(f);
  UiGfx gfx;
  PhrasePage page(gfx, f.engine, AudioGuard{}, false);
  openGrid(page);

  // ENTER on an unused slot asks first; nothing is marked yet
  CHECK(press(page, key('\n')));
  drawView(page, gfx);
  CHECK(gfx.shows("AFTER REPLACEMENT UNDO WILL NOT"));
  CHECK(gfx.shows("RESTORE THE OLD CONTENT"));
  CHECK(gfx.shows("NOTHING IS ERASED NOW"));
  CHECK(f.engine.reuseMarks().count() == 0);

  // Esc (as a scancode) cancels
  CHECK(press(page, escapeScancode()));
  drawView(page, gfx);
  CHECK(!gfx.shows("RESTORE THE OLD CONTENT"));
  CHECK(f.engine.reuseMarks().count() == 0);

  // ENTER, ENTER allows slot 1; the next slots need no second confirmation
  CHECK(press(page, key('\n')));
  CHECK(press(page, key('\n')));
  CHECK(f.engine.reuseMarks().marked(0));
  CHECK(press(page, arrow(GROOVEPUTER_RIGHT)));
  CHECK(press(page, key('\n')));
  CHECK(f.engine.reuseMarks().marked(1));
  CHECK(press(page, arrow(GROOVEPUTER_RIGHT)));
  CHECK(press(page, key('\n')));
  CHECK(press(page, arrow(GROOVEPUTER_RIGHT)));
  CHECK(press(page, key('\n')));
  CHECK(f.engine.reuseMarks().count() == 4);
  CHECK(toastText(page, gfx).find("ALLOWED") != std::string::npos);

  // the two results differ: permission set is not generation possible
  drawView(page, gfx);
  CHECK(gfx.count("*") == 4);
  CHECK(gfx.shows("TAKE 4B  NO / YES"));
  CHECK(gfx.shows("GROW 4B  NO / YES     8B  NO / NO"));

  // ENTER on an allowed slot cancels the permission
  CHECK(press(page, key('\n')));
  CHECK(!f.engine.reuseMarks().marked(3));
  CHECK(f.engine.reuseMarks().count() == 3);
  CHECK(press(page, key('\n')));                  // and allows it again (already confirmed)
  CHECK(f.engine.reuseMarks().marked(3));

  // a held slot cannot be allowed: Undo receipt slot 13
  for (int i = 0; i < 12; ++i) CHECK(press(page, arrow(GROOVEPUTER_RIGHT)));
  drawView(page, gfx);
  CHECK(gfx.shows("SLOT 16: HELD BY LIVE UNDO") || gfx.shows("SLOT 13: HELD BY LIVE UNDO"));
  CHECK(press(page, key('\n')));
  CHECK(toastText(page, gfx).find("HELD BY LIVE UNDO") != std::string::npos);
  CHECK(f.engine.reuseMarks().count() == 4);
  std::puts("PML-E: confirmation wording, cancel, allow/cancel per slot, two results, held slot refused: PASS");
}

void testAdmissibilityAndGenerate() {
  Fixture f("pmle-gen", GenerativeMode::Techno, 404);
  buildOrphans(f);
  UiGfx gfx;
  PhrasePage page(gfx, f.engine, AudioGuard{}, false);
  // G fails for lack of room and says where to go
  CHECK(press(page, key('g')));
  CHECK(toastText(page, gfx).find("R MAKES ROOM") != std::string::npos);
  for (int s = 0; s < 4; ++s) CHECK(SlotReuse::mark(f.engine, s) == MarkResult::Marked);
  drawView(page, gfx);
  CHECK(gfx.shows("REPLACES ALLOWED"));                 // product view: G is possible, and says it replaces
  const uint64_t before = GroovePuterMaterial::slotContentToken(f.scene(), 0);
  CHECK(press(page, key('g')));
  CHECK(f.engine.generatedPhraseRecipe() != nullptr && f.engine.generatedPhraseRecipe()->firstLocalSlot == 0);
  CHECK(GroovePuterMaterial::slotContentToken(f.scene(), 0) != before);
  CHECK(f.engine.reuseMarks().count() == 0);             // consumed
  std::puts("PML-E: G refusal points to R; allowed slots make the product view say so; G uses them: PASS");
}

void testDistinctGrowMessages() {
  {
    Fixture f("pmle-msg-house", GenerativeMode::House, 0);
    UiGfx gfx;
    PhrasePage page(gfx, f.engine, AudioGuard{}, false);
    CHECK(f.take(4, 0));
    CHECK(press(page, key('d')));
    // NotAdmitted: another TAKE in House is refused the same way, so the
    // message names the genre and points to where it can be changed.
    CHECK(toastText(page, gfx) == "HOUSE CAN'T GROW: FN+M GENRE");
  }
  {
    Fixture f("pmle-msg-edit", GenerativeMode::Techno, 404);
    UiGfx gfx;
    PhrasePage page(gfx, f.engine, AudioGuard{}, false);
    CHECK(f.take(4, 0));
    f.scene().synthABanks[0].patterns[1].steps[3].velocity ^= 0x21;
    CHECK(press(page, key('d')));
    CHECK(toastText(page, gfx) == "EDITED TAKE: PRESS G");           // EditedSinceGeneration
  }
  {
    Fixture f("pmle-msg-again", GenerativeMode::Techno, 404);
    UiGfx gfx;
    PhrasePage page(gfx, f.engine, AudioGuard{}, false);
    CHECK(f.take(4, 0));
    CHECK(press(page, key('d')));
    CHECK(press(page, key('d')));
    const std::string message = toastText(page, gfx);
    CHECK(message == "ALREADY GROWN" || message == "NOTHING TO ADD: PRESS G");   // CycleAlreadyPublished
    CHECK(message == "ALREADY GROWN");
  }
  {
    Fixture f("pmle-msg-room", GenerativeMode::Techno, 404);
    UiGfx gfx;
    PhrasePage page(gfx, f.engine, AudioGuard{}, false);
    for (int i = 0; i < 4; ++i) { CHECK(f.take(4, i * 4)); }       // the page is full, the last TAKE is kept
    CHECK(press(page, key('d')));
    CHECK(toastText(page, gfx) == "NO ROOM: R MAKES ROOM");      // NoSafeSlots
  }
  std::puts("PML-E: NotAdmitted, Edited, AlreadyGrown and NoSafeSlots each give a different message: PASS");
}

void testMakeRoomQuestion() {
  Fixture f("pmle-room", GenerativeMode::Techno, 404);
  buildOrphans(f);
  UiGfx gfx;
  PhrasePage page(gfx, f.engine, AudioGuard{}, false);

  // a hand edit: that slot must not be offered
  f.scene().synthABanks[0].patterns[2].steps[5].velocity ^= 0x11;

  CHECK(press(page, key('r')));
  drawView(page, gfx);
  CHECK(gfx.shows("REUSE 11 UNUSED TAKES?"));
  CHECK(gfx.shows("NOT IN SONG, NOT EDITED BY YOU."));
  CHECK(gfx.shows("AFTER THAT UNDO WILL NOT"));
  CHECK(gfx.shows("RESTORE THEIR OLD CONTENT."));
  CHECK(g_footerLeft.find("[ENTER]YES") != std::string::npos);
  CHECK(f.engine.reuseMarks().count() == 0);              // asking changes nothing

  CHECK(press(page, escapeScancode()));                    // NO (Esc as a scancode)
  CHECK(f.engine.reuseMarks().count() == 0);
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH"));

  CHECK(press(page, key('r')));
  CHECK(press(page, key('\n')));                          // YES
  CHECK(f.engine.reuseMarks().count() == 11);
  CHECK(!f.engine.reuseMarks().marked(2));                // the edited slot stays out
  CHECK(toastText(page, gfx) == "ROOM FOR 4B: PRESS G");
  drawView(page, gfx);
  CHECK(gfx.shows("REPLACES ALLOWED"));
  CHECK(gfx.shows("LENGTH"));                             // back on the product view

  // G now works and uses them; the untouched remainder stays
  CHECK(press(page, key('g')));
  CHECK(f.engine.generatedPhraseRecipe() != nullptr);
  std::puts("PML-E: MAKE ROOM asks once, offers only unedited takes, marks them on YES, then G works: PASS");
}

void testMakeRoomNothingToReuse() {
  Fixture f("pmle-room-none", GenerativeMode::Techno, 404);
  buildOrphans(f);
  f.engine.clearReuseMarks();                             // older material: a scene load drops the ledger
  UiGfx gfx;
  PhrasePage page(gfx, f.engine, AudioGuard{}, false);
  drawView(page, gfx);
  CHECK(gfx.shows("NO SLOTS: R") && !gfx.shows("REUSE"));
  CHECK(press(page, key('r')));
  drawView(page, gfx);
  CHECK(gfx.shows("NOTHING TO REUSE"));
  CHECK(gfx.shows("OLDER MATERIAL IS NEVER"));
  CHECK(press(page, key('\n')));                          // ENTER does nothing harmful
  CHECK(toastText(page, gfx) == "NOTHING TO REUSE");
  CHECK(f.engine.reuseMarks().count() == 0);
  CHECK(press(page, key('s')));                           // the grid is still reachable for older material
  drawView(page, gfx);
  CHECK(gfx.shows("SLOT SPACE"));
  CHECK(gfx.count("~") == 12);
  std::puts("PML-E: with no session-generated unedited takes the screen says so and the grid stays reachable: PASS");
}

void testPageEntryResets() {
  Fixture f("pmle-enter", GenerativeMode::Techno, 404);
  UiGfx gfx;
  PhrasePage page(gfx, f.engine, AudioGuard{}, false);
  CHECK(press(page, key('r')));
  page.onEnter(0);
  drawView(page, gfx);
  CHECK(gfx.shows("LENGTH") && !gfx.shows("SLOT SPACE"));   // entering the page starts on the product view
  std::puts("PML-E: entering the page resets the view: PASS");
}

}  // namespace

int main() {
  testOpenAndKeys();
  testGridHoldersAndResults();
  testConfirmAndMark();
  testMakeRoomQuestion();
  testMakeRoomNothingToReuse();
  testAdmissibilityAndGenerate();
  testDistinctGrowMessages();
  testPageEntryResets();
  std::puts("0.9.14 PML-E UI: PASS");
  return 0;
}
