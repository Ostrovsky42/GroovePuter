#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/ui/pages/pattern_edit_page.h"
#include "src/ui/pages/synth_sequencer_page.h"
#include "src/ui/pages/drum_sequencer_page.h"
#include "src/ui/pages/phrase_page.h"
#include "src/ui/ui_common.h"
#include "src/generation/migration/quantized_generation_commit.h"
SerialMock Serial;
SDMock SD;
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
  int textWidth(const char* value) const override { return value ? static_cast<int>(std::strlen(value)) * 6 : 0; }
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

void assertRuntimeMatches(MiniAcid& engine, int voice) {
  const auto beforeRefresh = engine.activePatternRuntimeEvents(voice);
  const auto& scene = engine.sceneManager().currentScene();
  const auto bank = engine.current303BankIndex(voice);
  const auto slot = engine.current303PatternIndex(voice);
  const auto& pattern = voice == 0 ? scene.synthABanks[bank].patterns[slot]
                                   : scene.synthBBanks[bank].patterns[slot];
  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = voice;
  settings.gateLengthRatio = engine.genreManager().getGrooveRecipe().gateLengthRatio;
  settings.swingPercent = std::clamp(static_cast<int>(scene.feel.swingPct), 50, 75);
  const auto id = voice == 0 ? VoiceId::SynthA : VoiceId::SynthB;
  settings.swingEnabled = (scene.feel.swingMask & (1u << static_cast<int>(id))) != 0;
  PhraseRuntime::RuntimePatternEventBank expected{};
  assert(expected.refresh(voice, bank, slot, pattern, settings) ==
         PhraseRuntime::PatternBankRefreshStatus::Ready);
  const auto& afterRefresh = expected.select(voice, bank, slot);
  assert(beforeRefresh.count == afterRefresh.count);
  assert(beforeRefresh.onsetMask == afterRefresh.onsetMask);
  for (unsigned i = 0; i < beforeRefresh.count; ++i) {
    const auto& a = beforeRefresh.events[i];
    const auto& b = afterRefresh.events[i];
    assert(a.startTick == b.startTick && a.durationSubticks == b.durationSubticks);
    assert(a.note == b.note && a.velocity == b.velocity);
    assert(a.probability == b.probability && a.flags == b.flags);
    assert(a.fx == b.fx && a.fxParam == b.fxParam);
  }
}

UIEvent letter(char value, KeyScanCode scan = GROOVEPUTER_NO_SCANCODE) {
  UIEvent e{}; e.event_type = GROOVEPUTER_KEY_DOWN; e.key = value; e.scancode = scan; return e;
}

void testSharedSelector(MiniAcid& engine, UiGfx& gfx) {
  for (int voice = 0; voice < 2; ++voice) {
    SynthSequencerPage page(gfx, engine, AudioGuard{}, voice);
    const auto original = engine.sceneManager().currentScene();
    GroovePuterState::setGenerationLevel(R::RealizationLevel::P1Canonical);
    const auto revision = GroovePuterUndo::undoOwner().committedRevision();
    for (const auto expected : {R::RealizationLevel::P2Variation,
                               R::RealizationLevel::P3Transformation,
                               R::RealizationLevel::P1Canonical}) {
      auto p = letter('p');
      assert(page.handleEvent(p));
      assert(GroovePuterState::currentGenerationLevel() == expected);
      gfx.labels.clear(); UI::drawToast(gfx);
      assert(gfx.shows(GroovePuterState::generationStyleName(expected)));
      assert(GroovePuterUndo::undoOwner().committedRevision() == revision);
      const auto& now = engine.sceneManager().currentScene();
      assert(GroovePuterUndo::PatternEdit::samePattern(original.synthABanks[0].patterns[0], now.synthABanks[0].patterns[0]));
      assert(GroovePuterUndo::PatternEdit::samePattern(original.synthBBanks[0].patterns[0], now.synthBBanks[0].patterns[0]));
    }
    auto p = letter(0, GROOVEPUTER_P);
    assert(page.handleEvent(p));
    assert(GroovePuterState::currentGenerationLevel() == R::RealizationLevel::P2Variation);
    for (int modifier = 0; modifier < 3; ++modifier) {
      auto modified = letter('p');
      modified.alt = modifier == 0; modified.ctrl = modifier == 1; modified.meta = modifier == 2;
      page.handleEvent(modified);
      assert(GroovePuterState::currentGenerationLevel() == R::RealizationLevel::P2Variation);
    }
    auto n = letter('n'); assert(page.handleEvent(n));
    for (auto note : {letter('p'), letter(0, GROOVEPUTER_P),
                      letter('g'), letter(0, GROOVEPUTER_G)}) {
      assert(page.handleEvent(note));
      assert(GroovePuterState::currentGenerationLevel() == R::RealizationLevel::P2Variation);
      assert(GroovePuterUndo::undoOwner().kind() == GroovePuterUndo::UndoKind::Pattern);
      const auto& current = engine.sceneManager().currentScene();
      const auto& pattern = voice == 0 ? current.synthABanks[0].patterns[0] : current.synthBBanks[0].patterns[0];
      assert(pattern.steps[0].note == ((note.key == 'p' || note.scancode == GROOVEPUTER_P) ? 69 : 52));
    }
  }
  DrumSequencerPage drums(gfx, engine, AudioGuard{});
  PhrasePage material(gfx, engine, AudioGuard{}, false);
  GroovePuterState::setGenerationLevel(R::RealizationLevel::P1Canonical);
  auto p = letter('p'); assert(drums.handleEvent(p));
  assert(GroovePuterState::currentGenerationLevel() == R::RealizationLevel::P2Variation);
  assert(material.handleEvent(p));
  assert(GroovePuterState::currentGenerationLevel() == R::RealizationLevel::P3Transformation);
  std::puts("P: both synths, shared drums/MATERIAL selector, note entry and modifiers: PASS");
}

void testAlt(MiniAcid& engine, UiGfx& gfx) {
  auto& scene = engine.sceneManager().currentScene();
  engine.genreManager().setGenerativeMode(GenerativeMode::Acid);
  engine.genreManager().setRecipe(0);
  for (int voice = 0; voice < 2; ++voice) {
    PatternEditPage page(gfx, engine, AudioGuard{}, voice);
    auto& pattern = voice == 0 ? scene.synthABanks[0].patterns[0] : scene.synthBBanks[0].patterns[0];
    for (bool playing : {false, true}) {
      pattern = SynthPattern{};
      assert(engine.rebuildPatternRuntimeEventBank());
      if (playing) engine.start();
      UIEvent event{};
      event.event_type = GROOVEPUTER_KEY_DOWN;
      event.key = 'g';
      event.alt = true;
      assert(page.handleEvent(event));
      if (playing) {
        // Keep the old audible material until the transport's boundary hook.
        assert(engine.activePatternRuntimeEvents(voice).count == 0);
        engine.genreManager().commitPendingRecipe();
        assert(R::quantizedGenerationStatus() == R::QuantizedGenerationStatus::Activated);
      }
      assert(engine.activePatternRuntimeEvents(voice).count > 0);
      assertRuntimeMatches(engine, voice);
      engine.stop();
      event = UIEvent{};
      event.event_type = GROOVEPUTER_APPLICATION_EVENT;
      event.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
      assert(page.handleEvent(event));
      assert(engine.activePatternRuntimeEvents(voice).count == 0);
      assertRuntimeMatches(engine, voice);
      assert(page.handleEvent(event)); // Redo restores the new audible events.
      assert(engine.activePatternRuntimeEvents(voice).count > 0);
      assertRuntimeMatches(engine, voice);
    }
  }
  std::puts("Alt+G: both voices, STOP/PLAY, boundary, Undo/Redo: PASS");

}

void testAltIgnoresSelector(MiniAcid& engine, UiGfx& gfx) {
  engine.stop();
  engine.modeManager().setGenerationSeed(0x12345678u);
  engine.genreManager().setGenerativeMode(GenerativeMode::Techno);
  engine.genreManager().setRecipe(0);
  auto& scene = engine.sceneManager().currentScene();
  for (int voice = 0; voice < 2; ++voice) {
    SynthSequencerPage page(gfx, engine, AudioGuard{}, voice);
    auto& selected = voice == 0 ? scene.synthABanks[0].patterns[0] : scene.synthBBanks[0].patterns[0];
    const auto other = voice == 0 ? scene.synthBBanks[0].patterns[0] : scene.synthABanks[0].patterns[0];
    const auto drums = scene.drumBanks[0].patterns[0];
    SynthPattern reference{};
    bool first = true;
    for (auto level : {R::RealizationLevel::P1Canonical, R::RealizationLevel::P2Variation,
                       R::RealizationLevel::P3Transformation}) {
      selected = SynthPattern{};
      assert(engine.rebuildPatternRuntimeEventBank());
      GroovePuterState::setGenerationLevel(level);
      auto event = letter('g'); event.alt = true;
      assert(page.handleEvent(event));
      assertRuntimeMatches(engine, voice);
      if (first) { reference = selected; first = false; }
      else assert(GroovePuterUndo::PatternEdit::samePattern(selected, reference));
      const auto& unchanged = voice == 0 ? scene.synthBBanks[0].patterns[0] : scene.synthABanks[0].patterns[0];
      assert(GroovePuterUndo::PatternEdit::samePattern(other, unchanged));
      assert(std::memcmp(&drums, &scene.drumBanks[0].patterns[0], sizeof(drums)) == 0);
      assert(GroovePuterState::currentGenerationLevel() == level);
    }
  }
  std::puts("Alt+G: selected synth only, identical source/seed ignores shared P: PASS");
}

void testCancelAndNonzeroTarget(MiniAcid& engine, UiGfx& gfx) {
  engine.stop();
  for (int voice = 0; voice < 2; ++voice) {
    engine.set303BankIndex(voice, 1);
    engine.set303PatternIndex(voice, 3);
    assert(engine.current303BankIndex(voice) == 1 && engine.current303PatternIndex(voice) == 3);
    auto& scene = engine.sceneManager().currentScene();
    auto& selected = voice == 0 ? scene.synthABanks[1].patterns[3] : scene.synthBBanks[1].patterns[3];
    selected = SynthPattern{};
    assert(engine.rebuildPatternRuntimeEventBank());
    SynthSequencerPage page(gfx, engine, AudioGuard{}, voice);
    engine.start();
    auto g = letter('g');
    assert(page.handleEvent(g));
    assert(R::quantizedGenerationStatus() == R::QuantizedGenerationStatus::PendingNextBar);
    assert(engine.activePatternRuntimeEvents(voice).count == 0);
    const auto generated = selected;
    UIEvent undo{}; undo.event_type = GROOVEPUTER_APPLICATION_EVENT;
    undo.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
    assert(page.handleEvent(undo));
    assertRuntimeMatches(engine, voice);
    assert(engine.activePatternRuntimeEvents(voice).count == 0);
    assert(GroovePuterUndo::undoOwner().nextIsRedo());
    assert(page.handleEvent(undo)); // Redo is deliberately refused while PLAY.
    assert(GroovePuterUndo::undoOwner().nextIsRedo());
    assert(engine.activePatternRuntimeEvents(voice).count == 0);
    engine.genreManager().commitPendingRecipe(); // Cancelled G must not reappear.
    assert(engine.activePatternRuntimeEvents(voice).count == 0);
    engine.stop();
    assert(page.handleEvent(undo));
    assertRuntimeMatches(engine, voice);
    assert(GroovePuterUndo::PatternEdit::samePattern(selected, generated));
    assert(page.handleEvent(undo));
    assertRuntimeMatches(engine, voice);
    assert(engine.activePatternRuntimeEvents(voice).count == 0);
    engine.set303BankIndex(voice, 0); engine.set303PatternIndex(voice, 0);
  }
  std::puts("G: nonzero bank/slot, pre-boundary cancellation, blocked PLAY redo, STOP redo: PASS");
}

int main(int argc, char** argv) {

  std::setbuf(stdout, nullptr);
  SceneStorageSdl storage;
  MiniAcid engine(44100, &storage);
  engine.init();
  engine.setSongMode(false);
  UiGfx gfx;
  auto& scene = engine.sceneManager().currentScene();
  if (argc > 1 && std::strcmp(argv[1], "--selector") == 0) {
    testSharedSelector(engine, gfx);
    return 0;
  }
  if (argc > 1 && std::strcmp(argv[1], "--alt") == 0) {
    testAlt(engine, gfx);
    return 0;
  }
  // First establish the defect with no stale receipt from a previous command.
  GroovePuterUndo::undoOwner().clear();
  SynthSequencerPage stopped(gfx, engine, AudioGuard{}, 0);
  UIEvent first{}; first.event_type = GROOVEPUTER_KEY_DOWN; first.key = 'g';
  assert(stopped.handleEvent(first));
  assert(GroovePuterUndo::undoOwner().kind() == GroovePuterUndo::UndoKind::Generation);
  std::puts("STOP G reaches quantized Generation owner: PASS");

  for (auto level : {R::RealizationLevel::P1Canonical,
                     R::RealizationLevel::P2Variation,
                     R::RealizationLevel::P3Transformation}) {
    GroovePuterState::setGenerationLevel(level);
    for (int mode = 0; mode < kGenerativeModeCount; ++mode) {
      engine.genreManager().setGenerativeMode(static_cast<GenerativeMode>(mode));
      engine.genreManager().setRecipe(0);
      scene.genre.rhythmSelectionMode = 0;
      scene.genre.rhythmArchetypeId = 0;
      const auto originalDrums = scene.drumBanks[0].patterns[0];
      for (int drums = 0; drums < 3; ++drums) {
        scene.drumBanks[0].patterns[0] = drums == 0 ? originalDrums : DrumPatternSet{};
        if (drums == 2)
          for (auto& step : scene.drumBanks[0].patterns[0].voices[KICK].steps)
            step.hit = true;
        for (int voice = 0; voice < 2; ++voice) {
          const auto other = voice == 0 ? scene.synthBBanks[0].patterns[0] : scene.synthABanks[0].patterns[0];
          const auto drumBefore = scene.drumBanks[0].patterns[0];
          assert(R::regenerateSynthWithQuantizedCommit(engine, voice) ==
                 R::QuantizedGenerationResult::CommittedNow);
          assertRuntimeMatches(engine, voice);
          engine.start();
          assert(R::regenerateSynthWithQuantizedCommit(engine, voice) ==
                 R::QuantizedGenerationResult::PendingNextBar);
          engine.genreManager().commitPendingRecipe();
          assert(R::quantizedGenerationStatus() == R::QuantizedGenerationStatus::Activated);
          assertRuntimeMatches(engine, voice);
          const auto& otherAfter = voice == 0 ? scene.synthBBanks[0].patterns[0] : scene.synthABanks[0].patterns[0];
          assert(GroovePuterUndo::PatternEdit::samePattern(other, otherAfter));
          assert(std::memcmp(&drumBefore, &scene.drumBanks[0].patterns[0], sizeof(drumBefore)) == 0);
          engine.stop();
        }
      }
    }
  }
  std::puts("Plain G: both voices, 16 genres, P1/P2/P3, three drum patterns, STOP/PLAY: PASS");

  // Use the actual top-level NOTES controller: STOP must not bypass the
  // same G owner tested directly above.
  for (int voice = 0; voice < 2; ++voice) {
    SynthSequencerPage page(gfx, engine, AudioGuard{}, voice);
    for (bool playing : {false, true}) {
      if (playing) engine.start();
      UIEvent event{};
      event.event_type = GROOVEPUTER_KEY_DOWN;
      event.key = 'g';
      assert(page.handleEvent(event));
      assert(GroovePuterUndo::undoOwner().kind() == GroovePuterUndo::UndoKind::Generation);
      assert(GroovePuterUndo::undoOwner().payloadSize() == R::quantizedGenerationUndoPayloadSize());
      assert(R::quantizedGenerationStatus() ==
             (playing ? R::QuantizedGenerationStatus::PendingNextBar
                      : R::QuantizedGenerationStatus::Committed));
      if (playing) engine.genreManager().commitPendingRecipe();
      assertRuntimeMatches(engine, voice);
      engine.stop();
      UIEvent undo{};
      undo.event_type = GROOVEPUTER_APPLICATION_EVENT;
      undo.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
      assert(page.handleEvent(undo));
      assertRuntimeMatches(engine, voice);
      assert(page.handleEvent(undo));
      assertRuntimeMatches(engine, voice);
      event.alt = true;
      if (playing) engine.start();
      assert(page.handleEvent(event));
      assert(GroovePuterUndo::undoOwner().kind() == GroovePuterUndo::UndoKind::Generation);
      assert(GroovePuterUndo::undoOwner().payloadSize() == sizeof(GroovePuterUndo::SynthPatternUndoPayload));
      if (playing) engine.genreManager().commitPendingRecipe();
      assertRuntimeMatches(engine, voice);
      engine.stop();
    }
  }
  std::puts("NOTES controller: G/Alt+G keep their distinct owners in STOP/PLAY: PASS");

  testAlt(engine, gfx);

  PatternEditPage page(gfx, engine, AudioGuard{}, 0);
  engine.start();
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = 'g';
  assert(page.handleEvent(event));
  assert(R::quantizedGenerationStatus() == R::QuantizedGenerationStatus::PendingNextBar);
  assert(page.handleEvent(event));
  assert(R::quantizedGenerationStatus() == R::QuantizedGenerationStatus::Busy);
  gfx.labels.clear();
  UI::drawToast(gfx);
  assert(gfx.shows("GEN BUSY"));
  engine.genreManager().commitPendingRecipe();
  assert(page.handleEvent(event));
  assert(R::quantizedGenerationStatus() == R::QuantizedGenerationStatus::PendingNextBar);
  engine.genreManager().commitPendingRecipe();
  engine.stop();
  std::puts("Repeated G: BUSY until boundary, then generates again: PASS");
  testSharedSelector(engine, gfx);
  testAltIgnoresSelector(engine, gfx);
  testCancelAndNonzeroTarget(engine, gfx);
}
