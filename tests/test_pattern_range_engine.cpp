// Pattern range above B4 (PATTERN_RANGE_MIDI): the STEPS editor writes C#5 and
// E5 (NOTE ENTRY rows, S/X octave), the step keeps the note as written up to
// C7, the Pattern player publishes it unchanged toward MIDI, and the internal
// voice plays it folded into C1..B4 (same pitch class). Old 24..71 notes play
// and persist exactly as before.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/pages/pattern_edit_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

namespace {

class NullGfx final : public IGfx {
 public:
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char*) override {}
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
  int textWidth(const char* v) const override { return v ? (int)std::strlen(v) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

bool g_ok = true;
void expect(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    g_ok = false;
  }
}

void press(PatternEditPage& page, char key) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = key;
  (void)page.handleEvent(e);
}

float readPhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

int stepNote(MiniAcid& engine, int step) {
  return engine.synthPattern(0).steps[step].note;
}

bool nearHz(float actual, float expected) {
  return std::fabs(actual - expected) <= expected * 1e-4f;
}

float hzOf(int note) { return 440.0f * std::pow(2.0f, (note - 69) / 12.0f); }

}  // namespace

int main() {
  // The fold itself: identity up to B4, whole octaves down above it.
  static_assert(MiniAcid::kMaxPatternNote == 96, "C7 pattern ceiling");
  static_assert(MiniAcid::internalVoiceNote(24) == 24, "C1 unchanged");
  static_assert(MiniAcid::internalVoiceNote(71) == 71, "B4 unchanged");
  static_assert(MiniAcid::internalVoiceNote(72) == 60, "C5 -> C4");
  static_assert(MiniAcid::internalVoiceNote(73) == 61, "C#5 -> C#4");
  static_assert(MiniAcid::internalVoiceNote(76) == 64, "E5 -> E4");
  static_assert(MiniAcid::internalVoiceNote(93) == 69, "A6 -> A4");
  static_assert(MiniAcid::internalVoiceNote(96) == 60, "C7 -> C4");
  static_assert(MiniAcid::internalVoiceNote(127) == 67, "G9 -> G4");
  for (int n = 0; n <= 127; ++n) {
    const int folded = MiniAcid::internalVoiceNote(n);
    expect(folded <= MiniAcid::kMax303Note, "fold stays at or below B4");
    expect((n - folded) % 12 == 0, "fold keeps the pitch class");
    if (n <= MiniAcid::kMax303Note) expect(folded == n, "24..71 never moves");
  }

  const auto root = std::filesystem::temp_directory_path() / "gp_test_pattern_range";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("range", 0);
  PatternPagingService::setProjectName("range");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  constexpr float kRate = 22050.0f;
  constexpr int kFrames = 512;
  MiniAcid engine{kRate, &storage};
  engine.init();
  engine.setSongMode(false);
  engine.setBpm(120.0f);
  engine.set303PatternIndex(0, 3);

  // 1. Editor: NOTE ENTRY reaches C#5/E5 one octave shift up; S/X step them.
  NullGfx gfx;
  PatternEditPage page(gfx, engine, AudioGuard{}, 0);
  {
    SynthPattern& pattern = engine.editSynthPattern(0);
    for (int i = 0; i < SynthPattern::kSteps; ++i) pattern.steps[i].note = -1;
  }
  press(page, 'n');                       // NOTE ENTRY on, cursor on step 0
  press(page, '=');                       // rows 4-5: lower C4.., upper C5..
  press(page, 't');                       // upper row index 4: E5
  expect(stepNote(engine, 0) == 76, "NOTE ENTRY: = then T writes E5 (76)");
  press(page, '\n');
  press(page, 'w');                       // upper row index 1: C#5
  expect(stepNote(engine, 1) == 73, "NOTE ENTRY: W writes C#5 (73)");
  press(page, '\n');
  press(page, '=');                       // rows 5-6
  press(page, '=');                       // refused: already the highest
  press(page, 'p');                       // upper row index 9: A6
  expect(stepNote(engine, 2) == 93, "NOTE ENTRY: top shift reaches A6 (93)");
  press(page, '\n');
  press(page, '-');
  press(page, '-');                       // back to the default rows 3-4
  press(page, 't');                       // E4
  expect(stepNote(engine, 3) == 64, "NOTE ENTRY: default rows unchanged (E4)");
  press(page, 'n');                       // NOTE ENTRY off, cursor still on 3
  press(page, 's');
  expect(stepNote(engine, 3) == 76, "S: E4 -> E5 is no longer clipped to B4");
  press(page, 's');
  press(page, 's');
  expect(stepNote(engine, 3) == 96, "S: saturates at C7 (96)");
  press(page, 'x');
  press(page, 'x');
  press(page, 'x');
  expect(stepNote(engine, 3) == 60, "X: C7 -> C4 by octaves");
  press(page, 'a');
  expect(stepNote(engine, 3) == 61, "A: semitone up");
  press(page, 'z');
  expect(stepNote(engine, 3) == 60, "Z: semitone down");

  press(page, 'n');                       // NOTE ENTRY on
  press(page, '\n');                      // cursor -> step 4
  press(page, 'p');                       // A4 (69) at the default rows
  press(page, 'n');
  press(page, 'a');
  press(page, 'a');
  expect(stepNote(engine, 4) == 71, "A: A4 -> B4, an old-range note");

  // 2. Playback: MIDI gets the step note as written, internal voice the fold.
  MusicalEventQueue queue;
  engine.setPatternEventQueue(&queue);
  queue.setPhaseReader(readPhase, &engine);

  std::vector<int> wireNotes;
  std::vector<float> internalHz;
  std::vector<int16_t> buffer(kFrames);
  auto internalTarget = [&]() -> float {
    IMonoSynthVoice* voice = engine.synthVoices_[0].activeVoice();
    auto* tb = dynamic_cast<TB303Voice*>(voice);
    return tb ? tb->targetFreq : -1.0f;
  };
  engine.start();
  for (uint32_t block = 1; block <= 40 && wireNotes.size() < 5; ++block) {
    queue.beginMidiRenderBlock(block, kFrames, engine.transportPhaseSteps(),
                               engine.bpm(), kRate, engine.isPlaying(), false,
                               true);
    engine.generateAudioBuffer(buffer.data(), kFrames);
    queue.endMidiRenderBlock();
    ScheduledMusicalEvent e{};
    while (queue.tryPop(e)) {
      if (e.event.source != MusicalEventSource::PatternPlayer ||
          e.event.target != MusicalEventTarget::SynthA ||
          e.event.type != MusicalEventType::NoteOn) {
        continue;
      }
      wireNotes.push_back(e.event.note);
      internalHz.push_back(internalTarget());
    }
  }
  engine.stop();

  if (wireNotes.size() != 5) {
    std::fprintf(stderr, "seen %zu Pattern NoteOns\n", wireNotes.size());
  }
  expect(wireNotes.size() == 5, "five Pattern NoteOns in the first steps");
  const int expectedWire[5] = {76, 73, 93, 60, 71};
  const int expectedInternal[5] = {64, 61, 69, 60, 71};
  for (std::size_t i = 0; i < wireNotes.size() && i < 5; ++i) {
    char what[96];
    std::snprintf(what, sizeof(what), "step %zu: wire note %d (want %d)", i,
                  wireNotes[i], expectedWire[i]);
    expect(wireNotes[i] == expectedWire[i], what);
    std::snprintf(what, sizeof(what),
                  "step %zu: internal %.2f Hz (want note %d = %.2f Hz)", i,
                  internalHz[i], expectedInternal[i],
                  hzOf(expectedInternal[i]));
    expect(nearHz(internalHz[i], hzOf(expectedInternal[i])), what);
  }

  // Engine-side manual editors share the ceiling (legacy pages, HUB grid).
  // Direct writes after playback: they do not go through the Pattern owner.
  engine.editSynthPattern(0).steps[10].note = 64;
  engine.adjust303StepOctave(0, 10, 1);
  expect(stepNote(engine, 10) == 76, "adjust303StepOctave: E4 -> E5");
  engine.adjust303StepOctave(0, 10, 2);
  expect(stepNote(engine, 10) == 96, "adjust303StepOctave: clamps at C7");
  engine.adjust303StepNote(0, 10, -23);
  expect(stepNote(engine, 10) == 73, "adjust303StepNote: C7 - 23 = C#5");

  // 3. Persistence: high notes round-trip; old 24..71 load unchanged.
  {
    SceneManager& manager = engine.sceneManager_;
    SynthPattern& p = manager.currentScene().synthABanks[0].patterns[5];
    const int8_t notes[8] = {24, 48, 60, 71, 73, 76, 96, -2};
    for (int i = 0; i < 8; ++i) p.steps[i].note = notes[i];
    const std::string json = manager.dumpCurrentScene();
    SceneManager loaded;
    expect(loaded.loadScene(json), "scene with C#5/E5/C7 loads");
    const SynthPattern& q = loaded.currentScene().synthABanks[0].patterns[5];
    for (int i = 0; i < 8; ++i) {
      expect(q.steps[i].note == notes[i], "step note survives save/load");
    }
  }

  if (!g_ok) return 1;
  std::puts("Pattern range engine (NOTE ENTRY E5/C#5, S/X to C7, wire = step, internal fold, save/load): PASS");
  return 0;
}
