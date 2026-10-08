// GRAB lands in a new saved Melody slot, by the steps a user takes by hand:
// STEPS, move to a free slot (Q..I), new Melody, the notes, Alt+Enter.
#include <cassert>
#include <cstdio>
#include <filesystem>

#include "src/dsp/miniacid_engine.h"
#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/pattern_paging.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/pages/synth_sequencer_page.h"

SerialMock Serial;
SDMock SD;

namespace {
using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Result = SynthSequencerPage::GrabSlotResult;

Buffer twoNotes(uint8_t first, uint8_t second) {
  Buffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  for (uint8_t i = 0; i < 2; ++i) {
    auto& e = melody.events[melody.count++];
    e.startTick = static_cast<uint16_t>(i * 96u);
    e.durationSubticks = 96u * PhraseRuntime::kSubticksPerTick;
    e.note = i == 0 ? first : second;
    e.velocity = 90;
    e.probability = 100;
  }
  return melody;
}

void expectSlotHolds(MiniAcid& engine, int bank, int pattern, const Buffer& melody) {
  assert(engine.current303BankIndex(0) == bank);
  assert(engine.display303LocalPatternIndex(0) == pattern);
  assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);
  assert(!engine.hasUnsavedWorkingMelody(0));
  const Scene& scene = engine.sceneManager().currentScene();
  const int slot = bank * Bank<SynthPattern>::kPatterns + pattern;
  assert(scene.materialSlots[0][slot].kind == GroovePuterMaterial::MaterialKind::Melody);
  Buffer saved{};
  assert(engine.loadCurrentSlotMelody(0, saved));  // read back from SD
  assert(saved.count == melody.count);
  for (uint16_t i = 0; i < melody.count; ++i) assert(saved.events[i].note == melody.events[i].note);
}
}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_grab_into_free_slot";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("grab_slot", 0);
  PatternPagingService::setProjectName("grab_slot");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(44100.0f, &storage);
  engine.init();
  engine.setSongMode(false);

  const int startBank = engine.current303BankIndex(0);
  const int startPattern = engine.display303LocalPatternIndex(0);

  // First GRAB: a free slot becomes a saved Melody holding exactly the notes.
  int bank = -1, pattern = -1;
  const Buffer first = twoNotes(60, 67);
  assert(SynthSequencerPage::grabIntoFreeSlot(engine, AudioGuard{}, 0, first, bank, pattern) ==
         Result::Saved);
  assert(!(bank == startBank && pattern == startPattern));  // never the slot it was on
  expectSlotHolds(engine, bank, pattern, first);

  // Second GRAB: the first slot is a Melody now, so it is not reused.
  int bank2 = -1, pattern2 = -1;
  const Buffer second = twoNotes(48, 55);
  assert(SynthSequencerPage::grabIntoFreeSlot(engine, AudioGuard{}, 0, second, bank2, pattern2) ==
         Result::Saved);
  assert(!(bank2 == bank && pattern2 == pattern));
  expectSlotHolds(engine, bank2, pattern2, second);

  // Unsaved edits on the voice: GRAB refuses and the voice stays where it is.
  assert(SynthSequencerPage::replaceMelodyFor(engine, AudioGuard{}, 0, twoNotes(70, 72)));
  assert(engine.hasUnsavedWorkingMelody(0));
  assert(SynthSequencerPage::voiceHasUnsavedEdits(engine, 0));
  int bank3 = -1, pattern3 = -1;
  assert(SynthSequencerPage::grabIntoFreeSlot(engine, AudioGuard{}, 0, first, bank3, pattern3) ==
         Result::Unsaved);
  assert(engine.current303BankIndex(0) == bank2 &&
         engine.display303LocalPatternIndex(0) == pattern2);
  assert(engine.currentPhraseBuffer(0).events[0].note == 70);  // the edit is untouched

  std::puts("GRAB into a free Melody slot: PASS");
  return 0;
}
