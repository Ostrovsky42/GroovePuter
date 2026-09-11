#include "arduino_compat.h"

#include "../display.h"
#include "../src/audio/audio_config.h"
#include "../src/dsp/generated_phrase_song.h"
#include "../src/dsp/miniacid_engine.h"
#include "../src/state/generated_phrase_product_state.h"
#include "../src/state/generation_request_state.h"
#include "../src/state/phrase_generation_request_state.h"
#include "../src/ui/pages/phrase_page.h"
#include "../src/ui/ui_core.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

SerialMock Serial;
SDMock SD;

namespace {

class FakeGfx : public IGfx {
 public:
  std::vector<std::string> texts;
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* text) override {
    texts.emplace_back(text ? text : "");
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
  int textWidth(const char* text) const override {
    return text ? static_cast<int>(std::strlen(text)) : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

UIEvent keyEvent(char key) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = key;
  return event;
}

bool hasText(const FakeGfx& gfx, const char* needle) {
  for (const auto& text : gfx.texts) {
    if (text.find(needle) != std::string::npos) return true;
  }
  return false;
}

void resetProductState() {
  GroovePuterState::resetGeneratedPhraseProductState();
  GroovePuterState::setRequestedPhraseBars(4);
  GroovePuterState::setGenerationLevel(
      GroovePuterRhythm::RealizationLevel::P2Variation);
}

void loadDefaultRuntime(MiniAcid& engine) {
  engine.sceneManager().loadDefaultScene();
  engine.setCurrentPage(0);
  // Mirror the persisted default Scene into runtime only through public
  // commands. This is the same ownership boundary the device UI can cross;
  // the characterization must not depend on MiniAcid's private load helper.
  engine.setSongMode(true);
  engine.setSongPlaybackSlot(0);
  engine.setSongPosition(0);
}

void testDefaultSceneStoppedPhraseG() {
  resetProductState();
  MiniAcid engine(kSampleRate, nullptr);
  loadDefaultRuntime(engine);

  const int expectedAppendRow = engine.sceneManager().currentScene().songs[0].length;
  assert(expectedAppendRow == 8);

  GeneratedPhraseSong::PreparedPhraseArrangement probe{};
  const bool preflight = GeneratedPhraseSong::prepare(
      engine, GroovePuterState::requestedPhraseBars(), expectedAppendRow, probe);
  if (!preflight) {
    std::printf("DEFAULT PHRASE PREFLIGHT: %s\n",
                PhraseGenerator::errorText(probe.result.error));
  }
  assert(preflight && "default scene must admit a stopped 4-bar Phrase at APPEND");

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);
  page.draw(gfx);
  assert(hasText(gfx, "APPEND A9"));
  assert(hasText(gfx, "FREE"));

  UIEvent g = keyEvent('g');
  assert(page.handleEvent(g));
  const auto& accepted = GroovePuterState::generatedPhraseProductState().accepted;
  assert(accepted.valid && "plain G on default PHRASE surface must publish a candidate");
  assert(!accepted.pendingNextBar);
  assert(accepted.songStart == expectedAppendRow);
  assert(accepted.bars == 4);
  std::puts("C0 H3 default-scene stopped PHRASE G: PASS");
}

void testDefaultScenePlayingPhraseG() {
  resetProductState();
  MiniAcid engine(kSampleRate, nullptr);
  loadDefaultRuntime(engine);
  assert(engine.songModeEnabled());
  assert(engine.songPlaybackSlot() == 0);
  engine.start();
  assert(engine.isPlaying());

  const int expectedAppendRow = engine.sceneManager().currentScene().songs[0].length;
  assert(expectedAppendRow == 8);

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);
  page.draw(gfx);
  assert(hasText(gfx, "APPEND A9"));
  assert(hasText(gfx, "FREE"));

  UIEvent g = keyEvent('g');
  assert(page.handleEvent(g));
  const auto& state = GroovePuterState::generatedPhraseProductState();
  if (!state.accepted.valid) {
    std::printf("LIVE PHRASE G OUTCOME: %s\n",
                GroovePuterState::generatedPhraseOutcomeName(state.lastOutcome));
  }
  assert(state.accepted.valid &&
         "plain G while the default Song is playing must publish a pending candidate");
  assert(state.accepted.pendingNextBar &&
         "live Phrase generation must be explicit about next-bar activation");
  assert(state.accepted.songStart == expectedAppendRow);
  assert(state.accepted.bars == 4);
  engine.stop();
  std::puts("C0 H3 default-scene playing PHRASE G: PASS");
}

void testPatternCapacityIsVisibleBeforeG() {
  resetProductState();
  MiniAcid engine(kSampleRate, nullptr);
  loadDefaultRuntime(engine);
  Scene& scene = engine.sceneManager().currentScene();

  // Default Song references local slots 0..7. Occupy the otherwise free
  // backing slots 8..15 without touching APPEND Song rows 8..11. Song-space
  // alone still looks free, but the generator cannot safely materialize 4 bars.
  for (int local = 8; local < kPatternsPerPage; ++local) {
    const int bank = local / Bank<SynthPattern>::kPatterns;
    const int index = local % Bank<SynthPattern>::kPatterns;
    scene.synthABanks[bank].patterns[index].steps[0].note = 60;
  }
  const int appendRow = scene.songs[0].length;
  assert(PhraseGenerator::songRowsAreAvailable(scene.songs[0], appendRow, 4));
  assert(PhraseGenerator::findSafeContiguousEmptySlots(scene, 0, 4) < 0);

  FakeGfx gfx;
  PhrasePage page(gfx, engine, AudioGuard{}, false);
  page.onEnter(0);
  page.draw(gfx);
  assert(!hasText(gfx, "FREE") &&
         "PHRASE must not promise FREE when backing Pattern capacity is exhausted");
  assert(hasText(gfx, "NO SLOTS") &&
         "PHRASE must name the hidden backing-capacity blocker before G is pressed");

  UIEvent g = keyEvent('g');
  assert(page.handleEvent(g));
  assert(!GroovePuterState::generatedPhraseProductState().accepted.valid);
  std::puts("C0 H3 Pattern backing capacity truth: PASS");
}

}  // namespace

int main() {
  testDefaultSceneStoppedPhraseG();
  testDefaultScenePlayingPhraseG();
  testPatternCapacityIsVisibleBeforeG();
  return 0;
}
