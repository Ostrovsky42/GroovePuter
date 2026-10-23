#include <bits/stdc++.h>
#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/ui/pages/synth_sequencer_page.h"
#undef private
#include "src/ui/pages/pattern_edit_page.h"
#include "src/ui/ui_common.h"
#include "src/ui/ui_utils.h"
#include "src/generation/migration/quantized_generation_commit.h"

SerialMock Serial;
SDMock SD;
namespace {
int failures = 0;
void expect(bool condition, const char* message) {
  if (!condition) { std::fprintf(stderr, "NOTE DISPLAY FAIL: %s\n", message); ++failures; }
}
class RecordingGfx final : public IGfx {
 public:
  std::vector<std::string> texts;
  int playheadRects = 0;
  bool contains(const std::string& s) const {
    for (const auto& text : texts) if (text.find(s) != std::string::npos) return true;
    return false;
  }
  void reset() { texts.clear(); playheadRects = 0; }
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* text) override { if (text) texts.emplace_back(text); }
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor color) override {
    if (color.color24() == COLOR_STEP_HILIGHT.color24()) ++playheadRects;
  }
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
    return text ? static_cast<int>(std::strlen(text)) * 6 : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

constexpr VisualStyle styles[] = {VisualStyle::MINIMAL, VisualStyle::RETRO_CLASSIC, VisualStyle::AMBER};

void patternTruth(int voice) {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  engine.set303PatternIndex(voice, 0);
  Scene& scene = engine.sceneManager().currentScene();
  scene.materialSlots[voice][0].id = GroovePuterMaterial::MaterialId{101};
  scene.materialSlots[voice][1].id = GroovePuterMaterial::MaterialId{102};
  auto& banks = voice == 0 ? scene.synthABanks : scene.synthBBanks;
  for (auto& step : banks[0].patterns[0].steps) step.note = -1;
  for (auto& step : banks[0].patterns[1].steps) step.note = -1;
  banks[0].patterns[0].steps[0].note = 48;
  banks[0].patterns[1].steps[0].note = 74;
  RecordingGfx gfx;
  PatternEditPage page{gfx, engine, AudioGuard{}, voice};
  page.setBoundaries(Rect{0, 16, 240, 93});
  page.setPatternCursor(1); // Browsing/failed target switch must not replace audible notes.
  for (auto style : styles) {
    UI::currentStyle = style;
    gfx.reset();
    page.draw(gfx);
    expect(gfx.contains("C3"), "all styles must show current slot, not selection cursor");
    expect(!gfx.contains("D5"), "browsed inactive slot leaked into note grid");
    if (style == VisualStyle::MINIMAL)
      expect(gfx.playheadRects == 0, "STOP must not highlight step zero as playing");
  }
  UI::currentStyle = VisualStyle::MINIMAL;
  engine.playing = true;
  engine.currentTick_ = 48;
  gfx.reset(); page.draw(gfx);
  expect(gfx.playheadRects == 1, "running transport must retain its current-step marker");
  engine.playing = false;
  GroovePuterMaterial::MaterialReference ref{};
  expect(engine.current303MaterialReference_(voice, ref), "current Pattern identity must resolve");
  auto working = banks[0].patterns[0];
  working.steps[0].note = 65;
  engine.workingMaterial_[voice].storePattern(working, ref);
  for (auto style : styles) {
    UI::currentStyle = style;
    gfx.reset(); page.draw(gfx);
    expect(gfx.contains("F4"), "exact-bound working notes must win in every style");
    expect(!gfx.contains("C3") && !gfx.contains("D5"), "accepted/browsed notes leaked over working material");
  }
  {
    // COMMIT has persisted a candidate, but the activation lease still owns
    // the old audible snapshot until BAR_START. It must win over Working.
    using namespace GroovePuterRhythm;
    using namespace GroovePuterRhythm::QuantizedGenerationDetail;
    auto& pending = g_slots[0];
    pending.owner = &engine;
    pending.target = captureTarget(engine.sceneManager());
    pending.scope = voice == 0 ? QuantizedGenerationScope::SynthA
                              : QuantizedGenerationScope::SynthB;
    pending.synth[voice] = working;
    pending.synth[voice].steps[0].note = 67;
    g_slotState[0].store(static_cast<uint8_t>(SlotState::Ready));
    g_publishedSlot.store(0);
    for (auto style : styles) {
      UI::currentStyle = style;
      gfx.reset(); page.draw(gfx);
      expect(gfx.contains("G4") && !gfx.contains("F4"),
             "quantized audible snapshot must win until boundary");
    }
    g_publishedSlot.store(-1);
    g_slotState[0].store(static_cast<uint8_t>(SlotState::Empty));
    pending.owner = nullptr;
  }
  engine.workingMaterial_[voice].clear();
  engine.set303PatternIndex(voice, 1);
  page.setPatternCursor(0); // Actual external activation must win on next draw, without tick.
  for (auto style : styles) {
    UI::currentStyle = style;
    gfx.reset(); page.draw(gfx);
    expect(gfx.contains("D5") && !gfx.contains("C3"), "activated slot must be visible immediately");
  }
  engine.setSongPattern(0, voice == 0 ? SongTrack::SynthA : SongTrack::SynthB, -1);
  engine.songMode_ = true;
  for (auto style : styles) {
    UI::currentStyle = style;
    gfx.reset(); page.draw(gfx);
    expect(!gfx.contains("C3") && !gfx.contains("D5"), "empty Song row must not display resident Pattern notes");
  }
}

void songMelodySilence(int voice) {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();
  engine.setSongPattern(0, voice == 0 ? SongTrack::SynthA : SongTrack::SynthB, 0);
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0].note = 60;
  melody.events[0].velocity = 100;
  melody.events[0].probability = 100;
  melody.events[0].durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
  engine.workingMaterial_[voice].storeMelody(melody);
  engine.activeMaterial_[voice].kind = GroovePuterMaterial::MaterialKind::Melody;
  engine.songMode_ = true;
  engine.songVoiceState_[voice] = MiniAcid::SongVoiceState::Awaiting;
  RecordingGfx gfx;
  SynthSequencerPage page{gfx, engine, AudioGuard{}, voice};
  page.setBoundaries(Rect{0, 0, 240, 135});
  for (auto style : styles) {
    UI::currentStyle = style;
    gfx.reset(); page.draw(gfx);
    expect(gfx.contains("WAIT: SONG MELODY"), "silent Awaiting state must show WAIT instead of retained Melody");
    expect(!gfx.contains("SELECTED C4"), "silent Awaiting state displayed retained notes as active");
    engine.songLoadFailedSlot_[voice] = 0;
    gfx.reset(); page.draw(gfx);
    expect(gfx.contains("FAIL: MELODY LOAD"), "failed Song load must show failure instead of retained Melody");
    engine.songLoadFailedSlot_[voice] = -1;
  }
  engine.songVoiceState_[voice] = MiniAcid::SongVoiceState::Held;
  gfx.reset(); page.draw(gfx);
  expect(gfx.contains("SELECTED C4"), "held, still-playing Melody must remain visible");
  engine.setSongPattern(0, voice == 0 ? SongTrack::SynthA : SongTrack::SynthB, 1);
  Scene& scene = engine.sceneManager().currentScene();
  auto& banks = voice == 0 ? scene.synthABanks : scene.synthBBanks;
  for (auto& step : banks[0].patterns[1].steps) step.note = -1;
  banks[0].patterns[1].steps[0].note = 74;
  engine.setSequencedSource(voice, MiniAcid::SequencedSource::Pattern);
  expect(engine.display303LocalPatternIndex(voice) == 1,
         "after Held Melody -> Steps, address must follow actual Song Pattern");
  for (auto style : styles) {
    UI::currentStyle = style;
    gfx.reset(); page.draw(gfx);
    expect(gfx.contains("D5"), "switch from Held Melody to Steps must render Pattern source");
    expect(!gfx.contains("HOLD: WORKING MELODY"), "retained Melody must not cover active Steps");
  }
}
} // namespace
int main() {
  for (int voice = 0; voice < 2; ++voice) { patternTruth(voice); songMelodySilence(voice); }
  std::fprintf(stderr, "Synth note display: %d failure(s)\n", failures);
  return failures ? 1 : 0;
}
