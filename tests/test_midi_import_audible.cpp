// PROJECT > Import MIDI writes the scene directly (MidiImporter). The audio
// reads the per-slot event cache, so the import must republish it
// (rebuildPatternRuntimeEventBank, as scene load does): the notes sound
// before any user edit.
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

SerialMock Serial;
SDMock SD;

static bool g_ok = true;
static void expect(bool cond, const char* what) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    g_ok = false;
  }
}

int main() {
  MiniAcid engine(44100.0f, nullptr);
  engine.init();
  engine.setSongMode(false);
  const int bank = engine.current303BankIndex(0);
  const int pat = engine.display303LocalPatternIndex(0);
  auto& slot = engine.sceneManager().currentScene().synthABanks[bank].patterns[pat];

  // What the importer does: two notes straight into the scene pattern.
  slot.steps[0].note = 48;
  slot.steps[0].velocity = 100;
  slot.steps[8].note = 55;
  slot.steps[8].velocity = 100;

  expect(engine.patternRuntimeBank_.select(0, bank, pat).count == 0,
         "cache is stale after a direct scene write (the bug)");
  expect(engine.activePatternRuntimeEvents(0).count == 0,
         "silent before the import republishes");

  // The call ProjectPage::importMidiAtSelection makes after a successful import.
  expect(engine.rebuildPatternRuntimeEventBank(), "rebuild succeeds");

  const auto& ev = engine.activePatternRuntimeEvents(0);
  expect(ev.count == 2, "both imported notes are in the active events");
  expect(ev.eventForSourceStep(0) != nullptr &&
             ev.eventForSourceStep(0)->note == 48,
         "step 0 plays note 48");
  expect(ev.eventForSourceStep(8) != nullptr &&
             ev.eventForSourceStep(8)->note == 55,
         "step 8 plays note 55");

  if (g_ok) std::puts("midi import audible: PASS");
  return g_ok ? 0 : 1;
}
