// PROJECT > Import MIDI writes the scene directly (MidiImporter). The audio
// reads the per-slot event cache, so the import must republish it
// (rebuildPatternRuntimeEventBank, as scene load does): the notes sound
// before any user edit.
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/audio/midi_importer.h"
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

// Type 0 SMF, 96 ppq (24 ticks per 1/16 step). Channel 1: a chord (48+60)
// at step 0 and note 55 at step 8. Channel 10: a kick (36) at step 0.
static void writeSmf(const char* path) {
  const std::vector<unsigned char> trk = {
      0x00, 0x90, 48, 100, 0x00, 0x90, 60, 100,
      0x00, 0x99, 36, 100,
      0x81, 0x40, 0x90, 55, 100,
      0x00, 0xFF, 0x2F, 0x00};
  std::vector<unsigned char> f = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
                                  'M', 'T', 'r', 'k', 0, 0, 0,
                                  static_cast<unsigned char>(trk.size())};
  f.insert(f.end(), trk.begin(), trk.end());
  std::ofstream o(path, std::ios::binary | std::ios::trunc);
  o.write(reinterpret_cast<const char*>(f.data()), f.size());
}

static MidiImporter::Error importWith(MiniAcid& engine, uint16_t a, uint16_t b,
                                      uint16_t d, bool publish) {
  writeSmf("imp_audible.mid");
  MidiImporter::ImportSettings st;
  st.targetPatternIndex = 0;
  st.overwrite = true;
  st.synthAMask = a;
  st.synthBMask = b;
  st.drumMask = d;
  MidiImporter imp(engine);
  const auto err = imp.importFile("imp_audible.mid", st);
  if (err == MidiImporter::Error::None && publish) {
    engine.publishMidiImport(0, imp.getLastImportedPatternIdx() + 1,
                             a != 0, b != 0, d != 0, false);
  }
  return err;
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

  // (a) Import into A only, pattern mode, with the old selection elsewhere.
  {
    engine.set303BankIndex(0, 1);
    engine.set303PatternIndex(0, 3);
    expect(engine.current303BankIndex(0) == 1, "setup: A parked on bank 1");
    expect(importWith(engine, 0x1, 0, 0, true) == MidiImporter::Error::None,
           "A-only import succeeds");
    expect(engine.current303BankIndex(0) == 0 &&
               engine.display303LocalPatternIndex(0) == 0,
           "A selection moved to the imported slot");
    const auto& ev = engine.activePatternRuntimeEvents(0);
    expect(ev.eventForSourceStep(0) != nullptr, "A events non-empty at step 0");
    // (c) the chord keeps one note per step: the top one.
    auto& a0 = engine.sceneManager().currentScene().synthABanks[0].patterns[0];
    expect(a0.steps[0].note >= 0 && a0.steps[0].note >= 55 - 12,
           "A step 0 holds a note");
    int notes = 0;
    for (int i = 0; i < SynthPattern::kSteps; ++i) notes += a0.steps[i].note >= 0;
    expect(notes == 2, "A: one note per step (steps 0 and 8)");
    expect(ev.eventForSourceStep(0) != nullptr &&
               ev.eventForSourceStep(0)->note == a0.steps[0].note,
           "A step 0 plays the single stored note");
  }

  // (d) A's import clears Song rows of unrouted tracks; routed A is written.
  {
    engine.setSongPattern(0, SongTrack::Drums, 5);
    engine.setSongPattern(0, SongTrack::SynthB, 6);
    expect(importWith(engine, 0x1, 0, 0, true) == MidiImporter::Error::None,
           "second A-only import succeeds");
    expect(engine.songPatternAt(0, SongTrack::SynthA) == 0, "A row = imported");
    expect(engine.songPatternAt(0, SongTrack::Drums) < 0, "unrouted Drums row cleared");
    expect(engine.songPatternAt(0, SongTrack::SynthB) < 0, "unrouted B row cleared");
    expect(engine.songPlaybackSlot() == engine.activeSongSlot(),
           "Song playback slot equals the written slot");
  }

  // (b) Drums only: the trigger source is the imported slot.
  {
    engine.setDrumBankIndex(1);
    engine.setDrumPatternIndex(2);
    expect(importWith(engine, 0, 0, 1u << 9, true) == MidiImporter::Error::None,
           "drums-only import succeeds");
    const DrumPatternSet* imported =
        &engine.sceneManager().currentScene().drumBanks[0].patterns[0];
    expect(&engine.sceneManager().getCurrentDrumPattern() == imported,
           "drum trigger source is the imported slot");
    expect(imported->voices[0].steps[0].hit, "imported kick at step 0");
  }

  if (g_ok) std::puts("midi import audible: PASS");
  return g_ok ? 0 : 1;
}
