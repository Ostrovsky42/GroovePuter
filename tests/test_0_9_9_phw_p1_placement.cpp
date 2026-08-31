// 0.9.9-PHW-P1 own regression coverage for the generated-Phrase product
// placement workflow (see the PHW-P1 task spec: APPEND/EXPLICIT state
// machine, FREE/OCCUPIED/NO ROOM admissibility, page-independent Synth-A
// accepted-liveness predicate, candidate lifetime/resurrection, focus
// topology). This drives PhrasePage directly through its public
// draw()/handleEvent()/onEnter() surface -- placement_mode_/explicit_row_/
// product_focus_ are private, so every assertion here is a black-box check
// against exactly what the on-device screen and input handling would show
// a real user. It does not duplicate PMB-P1's/P1R's/I1's/D2's/E0a's
// coverage of the underlying frozen generator.

#include "arduino_compat.h"

#include "../display.h"
#include "../src/audio/audio_config.h"
#include "../src/dsp/generated_phrase_song.h"
#include "../src/dsp/miniacid_engine.h"
#include "../src/state/generated_phrase_product_state.h"
#include "../src/state/generation_request_state.h"
#include "../src/state/phrase_generation_request_state.h"
#include "../src/ui/pages/phrase_page.h"
#include "../src/ui/key_normalize.h"
#include "../src/ui/ui_core.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

SerialMock Serial;
SDMock SD;

using namespace GroovePuterRhythm;

namespace {

// ---------------------------------------------------------------------
// Headless IGfx: records every drawText() call so assertions can treat the
// rendered screen as the black-box surface under test, the same thing a
// real user reads on hardware.
// ---------------------------------------------------------------------
class FakeGfx : public IGfx {
 public:
  struct TextEntry {
    int x;
    int y;
    std::string text;
  };
  std::vector<TextEntry> texts;

  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int x, int y, const char* text) override {
    texts.push_back({x, y, text ? text : ""});
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
  // Deliberately tiny: callers like Widgets::drawClippedText compare this
  // against a pixel budget and silently truncate/ellipsize the string
  // (with a "..." that would otherwise defeat this file's substring
  // assertions) when it looks too wide. This test cares about which full
  // strings PhrasePage hands to IGfx, not real font metrics/layout.
  int textWidth(const char* text) const override {
    return text ? static_cast<int>(std::strlen(text)) : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

bool has(const FakeGfx& gfx, const char* needle) {
  for (const auto& entry : gfx.texts) {
    if (entry.text.find(needle) != std::string::npos) return true;
  }
  return false;
}

UIEvent keyEvent(char key) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = key;
  return event;
}

UIEvent navEvent(KeyScanCode code) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.scancode = code;
  return event;
}

void redraw(PhrasePage& page, FakeGfx& gfx) {
  gfx.texts.clear();
  page.draw(gfx);
}

// IPage::handleEvent takes UIEvent& (non-const) -- bind the temporary to a
// named local here so call sites can stay one-liners.
void press(PhrasePage& page, UIEvent event) {
  page.handleEvent(event);
}

// Same known-good family PMB-P1's own tests use: LoFi/ClassicChill always
// has an admissible bars value at a fresh destination, and it never hits
// the typed P1R length rejection that Rave/kBaseRecipeId can (see
// tests/test_0_9_9_phrase_pmb_p1_bounded_prepare_commit.cpp).
void configureKnownGoodFamily(MiniAcid& engine) {
  Scene& scene = engine.sceneManager().currentScene();
  scene.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
  scene.genre.recipe = kClassicChillRecipeId;
  scene.genre.morphTarget = 0;
  scene.genre.morphAmount = 0;
  scene.genre.regenerateOnApply = false;
  scene.genre.applyTempoOnApply = false;
  scene.activeSongSlot = 0;
  scene.songs[0] = Song{};
  scene.feel.patternBars = 1;
  for (int bank = 0; bank < kBankCount; ++bank) {
    for (int slot = 0; slot < Bank<SynthPattern>::kPatterns; ++slot) {
      scene.synthABanks[bank].patterns[slot] = SynthPattern{};
      scene.synthBBanks[bank].patterns[slot] = SynthPattern{};
      scene.drumBanks[bank].patterns[slot] = DrumPatternSet{};
    }
  }
}

uint8_t firstAdmissibleBars(MiniAcid& engine) {
  for (uint8_t bars : std::array<uint8_t, 4>{1, 2, 4, 8}) {
    GeneratedPhraseSong::PreparedPhraseArrangement probe{};
    if (GeneratedPhraseSong::prepare(engine, bars, 0, probe)) return bars;
  }
  assert(false && "no admissible phrase length found for this family");
  return 0;
}

void resetProductSessionState() {
  GroovePuterState::resetGeneratedPhraseProductState();
  GroovePuterState::setRequestedPhraseBars(4);
  GroovePuterState::setGenerationLevel(
      GroovePuterRhythm::RealizationLevel::P2Variation);
}

// ---------------------------------------------------------------------
// (1) Normal entry always resolves APPEND against the current authoritative
// Song logical end -- never a cached destination, never transport position.
// ---------------------------------------------------------------------
void testNormalEntryResolvesAppend() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  engine.sceneManager().currentScene().songs[0].length = 5;

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);
  redraw(page, gfx);

  assert(has(gfx, "APPEND A6"));
  assert(!has(gfx, "EXPLICIT"));
  std::puts("PHW-P1 T1 normal entry resolves APPEND from Song length: OK");
}

// ---------------------------------------------------------------------
// (2) SONG cursor -> PHRASE one-shot handoff produces EXPLICIT at exactly
// that row; a later normal entry (no fresh handoff) discards it.
// ---------------------------------------------------------------------
void testHandoffExplicitThenDiscarded() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  engine.sceneManager().currentScene().songs[0].length = 5;

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);

  page.onEnter(24 + 1);  // SONG cursor row 24 (0-indexed) handed off
  redraw(page, gfx);
  assert(has(gfx, "EXPLICIT A25"));

  page.onEnter(0);  // returning to PHRASE normally, no fresh handoff
  redraw(page, gfx);
  assert(has(gfx, "APPEND A6"));
  assert(!has(gfx, "EXPLICIT"));
  std::puts("PHW-P1 T2 SONG handoff -> EXPLICIT, discarded on next normal entry: OK");
}

// ---------------------------------------------------------------------
// (3) APPEND re-resolves every frame against the current Song logical end;
// a manual Song edit between frames changes the displayed/resolved row.
// ---------------------------------------------------------------------
void testAppendResolvesDynamically() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  Scene& scene = engine.sceneManager().currentScene();
  scene.songs[0].length = 5;

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);
  redraw(page, gfx);
  assert(has(gfx, "APPEND A6"));

  scene.songs[0].length = 10;  // simulate an ordinary SONG-page edit
  redraw(page, gfx);
  assert(has(gfx, "APPEND A11"));
  std::puts("PHW-P1 T3 APPEND re-resolves against current Song length: OK");
}

// ---------------------------------------------------------------------
// (4)+(5) TO focus state machine: APPEND+RIGHT -> EXPLICIT(resolvedAppend+1);
// EXPLICIT+LEFT/RIGHT moves by 1; ENTER on EXPLICIT -> APPEND; bounds fail
// closed (no wraparound) at the top of the valid row range.
// ---------------------------------------------------------------------
void testToPlacementStateMachine() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  Scene& scene = engine.sceneManager().currentScene();
  scene.songs[0].length = 5;

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);

  // Focus starts at LENGTH; two DOWN presses reach TO (no live accepted
  // candidate exists, so the cycle is exactly LENGTH -> DEPTH -> TO).
  press(page, navEvent(GROOVEPUTER_DOWN));
  press(page, navEvent(GROOVEPUTER_DOWN));

  press(page, navEvent(GROOVEPUTER_RIGHT));
  redraw(page, gfx);
  assert(has(gfx, "EXPLICIT A7"));  // resolvedAppendRow (5) + 1 -> row 6 -> A7

  press(page, navEvent(GROOVEPUTER_RIGHT));
  redraw(page, gfx);
  assert(has(gfx, "EXPLICIT A8"));

  press(page, navEvent(GROOVEPUTER_LEFT));
  redraw(page, gfx);
  assert(has(gfx, "EXPLICIT A7"));

  UIEvent enter = keyEvent('\n');
  press(page, enter);
  redraw(page, gfx);
  assert(has(gfx, "APPEND A6"));
  assert(!has(gfx, "EXPLICIT"));

  // ENTER while already APPEND: no semantic change.
  press(page, enter);
  redraw(page, gfx);
  assert(has(gfx, "APPEND A6"));

  // Bounds fail closed: driving LEFT far past row 0 clamps at row 0
  // (A1), it must never wrap to the top of the valid range.
  for (int i = 0; i < 20; ++i) press(page, navEvent(GROOVEPUTER_LEFT));
  redraw(page, gfx);
  assert(has(gfx, "EXPLICIT A1"));
  assert(!has(gfx, "A0"));
  std::puts("PHW-P1 T4/T5 TO APPEND/EXPLICIT state machine, ENTER, clamped bounds: OK");
}

// ---------------------------------------------------------------------
// (6) FREE/OCCUPIED/NO ROOM admissibility mirrors the exact generation
// predicate (PhraseGenerator::songRowsAreAvailable + Song::kMaxPositions).
// ---------------------------------------------------------------------
void testAdmissibilityStates() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  Scene& scene = engine.sceneManager().currentScene();
  scene.songs[0].length = 5;
  GroovePuterState::setRequestedPhraseBars(4);

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);
  redraw(page, gfx);
  assert(has(gfx, "FREE"));

  scene.songs[0].positions[6].patterns[static_cast<int>(SongTrack::SynthA)] = 0;
  redraw(page, gfx);
  assert(has(gfx, "OCCUPIED"));
  scene.songs[0].positions[6].patterns[static_cast<int>(SongTrack::SynthA)] = -1;

  page.onEnter(Song::kMaxPositions - 2 + 1);  // explicit row with < 4 rows left
  redraw(page, gfx);
  assert(has(gfx, "NO ROOM"));
  std::puts("PHW-P1 T6 FREE/OCCUPIED/NO ROOM admissibility: OK");
}

// ---------------------------------------------------------------------
// (7) Focus topology: LENGTH -> DEPTH -> TO -> LENGTH when no live accepted
// candidate exists (3-state cycle) -- checked behaviorally by observing
// which field responds to LEFT/RIGHT after each DOWN, since focus itself
// is private UI state.
// ---------------------------------------------------------------------
void testFocusTopologyWithoutLiveBar() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  engine.sceneManager().currentScene().songs[0].length = 5;
  GroovePuterState::setRequestedPhraseBars(4);

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);

  // Starting focus: LENGTH. RIGHT cycles the length domain (1/2/4/8).
  press(page, navEvent(GROOVEPUTER_RIGHT));
  redraw(page, gfx);
  assert(has(gfx, "8B"));
  assert(GroovePuterState::requestedPhraseBars() == 8);

  // DOWN once -> DEPTH. RIGHT must change DEPTH, not LENGTH or TO.
  press(page, navEvent(GROOVEPUTER_DOWN));
  const auto beforeDepth = GroovePuterState::currentGenerationLevel();
  press(page, navEvent(GROOVEPUTER_RIGHT));
  assert(GroovePuterState::currentGenerationLevel() != beforeDepth);
  assert(GroovePuterState::requestedPhraseBars() == 8);  // LENGTH untouched

  // DOWN again -> TO (two DOWN presses from LENGTH always reach TO).
  press(page, navEvent(GROOVEPUTER_DOWN));
  press(page, navEvent(GROOVEPUTER_RIGHT));
  redraw(page, gfx);
  assert(has(gfx, "EXPLICIT"));  // TO moved, confirming focus reached TO

  // DOWN a third time wraps back to LENGTH (3-state cycle, no live BAR).
  press(page, navEvent(GROOVEPUTER_DOWN));
  press(page, navEvent(GROOVEPUTER_LEFT));
  redraw(page, gfx);
  assert(has(gfx, "4B"));  // LENGTH cycled back down from 8B
  std::puts("PHW-P1 T7 focus topology LENGTH->DEPTH->TO->LENGTH (no live BAR): OK");
}

// ---------------------------------------------------------------------
// (8) Successful G: candidate becomes live immediately, placement returns
// to APPEND, and BAR is appended to the focus cycle only while live.
// ---------------------------------------------------------------------
void testSuccessfulGenerateGoesLiveAndReturnsAppend() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  const uint8_t bars = firstAdmissibleBars(engine);
  GroovePuterState::setRequestedPhraseBars(bars);

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  // EXPLICIT row 0 -- exactly the destination firstAdmissibleBars() proved
  // admissible above (it probes at songStart=0); APPEND's resolved row
  // would be >=1 here since MiniAcid::songLength() clamps to a minimum of
  // 1 even for a fully empty Song.
  page.onEnter(1);
  redraw(page, gfx);
  assert(has(gfx, "EXPLICIT A1"));
  assert(has(gfx, "FREE"));

  press(page, keyEvent('g'));
  const auto& accepted = GroovePuterState::generatedPhraseProductState().accepted;
  assert(accepted.valid);

  redraw(page, gfx);
  assert(has(gfx, "APPEND"));   // spec section 7/1: successful G -> APPEND
  assert(!has(gfx, "EXPLICIT"));
  assert(has(gfx, "LAST "));
  assert(!has(gfx, "LAST  --"));  // candidate must render as live, not stale

  std::puts("PHW-P1 T8 successful G: candidate live, placement returns to APPEND: OK");
}

// ---------------------------------------------------------------------
// (9) Accepted liveness is a page-independent structural predicate keyed
// only on the Synth A anchor: Synth B/Drums Song-ref changes and in-pattern
// note edits never affect it; clearing Synth A does; the candidate survives
// (non-live, not deleted) and can resurrect once the anchor is restored.
// ---------------------------------------------------------------------
void testAcceptedLivenessSynthAAnchorOnly() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  const uint8_t bars = firstAdmissibleBars(engine);
  GroovePuterState::setRequestedPhraseBars(bars);

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(1);  // EXPLICIT row 0, matching firstAdmissibleBars()'s probe
  press(page, keyEvent('g'));
  auto accepted = GroovePuterState::generatedPhraseProductState().accepted;
  assert(accepted.valid);

  Scene& scene = engine.sceneManager().currentScene();
  Song& song = scene.songs[accepted.songSlot];
  const int row = accepted.songStart;
  const int synthAIdx = static_cast<int>(SongTrack::SynthA);
  const int synthBIdx = static_cast<int>(SongTrack::SynthB);
  const int drumsIdx = static_cast<int>(SongTrack::Drums);
  const int16_t expectedAnchor = song.positions[row].patterns[synthAIdx];

  redraw(page, gfx);
  assert(has(gfx, "LAST ") && !has(gfx, "LAST  --"));

  // Change only Synth B ref -> remains live.
  const int16_t savedB = song.positions[row].patterns[synthBIdx];
  song.positions[row].patterns[synthBIdx] = -1;
  redraw(page, gfx);
  assert(!has(gfx, "LAST  --"));
  song.positions[row].patterns[synthBIdx] = savedB;

  // Change only Drums ref -> remains live.
  const int16_t savedD = song.positions[row].patterns[drumsIdx];
  song.positions[row].patterns[drumsIdx] = -1;
  redraw(page, gfx);
  assert(!has(gfx, "LAST  --"));
  song.positions[row].patterns[drumsIdx] = savedD;

  // Switching the currently-loaded pattern page must never affect liveness
  // (page-independence, PHW-P1 spec section 14) -- no currentPageIndex()
  // dependency in the predicate.
  engine.setCurrentPage(static_cast<int8_t>((accepted.pageIndex + 1) % kMaxPages));
  redraw(page, gfx);
  assert(!has(gfx, "LAST  --"));
  engine.setCurrentPage(static_cast<int8_t>(accepted.pageIndex));

  // Clear the Synth A anchor -> non-live. Candidate itself is preserved
  // (still stored, not deleted) -- GeneratedPhraseProductState still
  // reports the same accepted.valid record.
  song.positions[row].patterns[synthAIdx] = -1;
  redraw(page, gfx);
  assert(has(gfx, "LAST  --"));
  assert(GroovePuterState::generatedPhraseProductState().accepted.valid);

  // Restore the exact expected Synth A anchor manually (simulating an
  // ordinary Song edit that happens to reconstruct it) -> live again, same
  // candidate (intentional resurrection, PHW-P1 spec section 17).
  song.positions[row].patterns[synthAIdx] = expectedAnchor;
  redraw(page, gfx);
  assert(!has(gfx, "LAST  --"));
  std::puts("PHW-P1 T9 accepted liveness: Synth A anchor only, page-independent, resurrects: OK");
}

// ---------------------------------------------------------------------
// (10) Scene-replacement reset: resetGeneratedPhraseProductState() (the
// hook installed at SceneManager::loadSceneEventedWithReader's single
// success chokepoint) clears the candidate; ordinary Song mutation alone
// must not.
// ---------------------------------------------------------------------
void testSceneReplacementResetsCandidate() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  const uint8_t bars = firstAdmissibleBars(engine);
  GroovePuterState::setRequestedPhraseBars(bars);

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(1);  // EXPLICIT row 0, matching firstAdmissibleBars()'s probe
  press(page, keyEvent('g'));
  assert(GroovePuterState::generatedPhraseProductState().accepted.valid);

  // Ordinary Song mutation alone must not reset the candidate.
  engine.sceneManager().currentScene().songs[0].length += 1;
  assert(GroovePuterState::generatedPhraseProductState().accepted.valid);

  GroovePuterState::resetGeneratedPhraseProductState();
  assert(!GroovePuterState::generatedPhraseProductState().accepted.valid);
  std::puts("PHW-P1 T10 scene-replacement reset clears candidate, Song mutation alone does not: OK");
}

// ---------------------------------------------------------------------
// (11) PHRASE CORE is a distinct instance from PHRASE (product): a
// generated candidate published via one page's G is visible on the other
// (shared global product state), but placement/focus state is per-instance
// -- core_mode_ is fixed at construction, never a runtime toggle.
// ---------------------------------------------------------------------
void testCoreModeIsSeparateInstance() {
  resetProductSessionState();
  MiniAcid engine(kSampleRate, nullptr);
  configureKnownGoodFamily(engine);
  engine.sceneManager().currentScene().songs[0].length = 5;

  FakeGfx gfx;
  PhrasePage product(gfx, engine, AudioGuard{}, false);
  PhrasePage core(gfx, engine, AudioGuard{}, true);

  product.onEnter(0);
  gfx.texts.clear();
  product.draw(gfx);
  assert(has(gfx, "PHRASE") && !has(gfx, "PHRASE CORE"));

  gfx.texts.clear();
  core.draw(gfx);
  assert(has(gfx, "PHRASE CORE"));
  assert(!has(gfx, "APPEND") && !has(gfx, "EXPLICIT"));  // no product placement UI at all
  std::puts("PHW-P1 T11 PHRASE and PHRASE CORE are separate page instances: OK");
}

}  // namespace

int main() {
  testNormalEntryResolvesAppend();
  testHandoffExplicitThenDiscarded();
  testAppendResolvesDynamically();
  testToPlacementStateMachine();
  testAdmissibilityStates();
  testFocusTopologyWithoutLiveBar();
  testSuccessfulGenerateGoesLiveAndReturnsAppend();
  testAcceptedLivenessSynthAAnchorOnly();
  testSceneReplacementResetsCandidate();
  testCoreModeIsSeparateInstance();
  std::puts("0.9.9-PHW-P1 placement/liveness: PASS");
  return 0;
}
