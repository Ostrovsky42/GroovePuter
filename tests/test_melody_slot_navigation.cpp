// M2 slot navigation on a voice playing MELODY: Q..I / B move only between
// slots that hold an accepted Melody, load it from storage, and never drop
// unsaved Working edits.
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/audio/pattern_paging.h"
#include "src/state/melody_promotion.h"
#include "src/ui/phrase_source_toggle.h"

SerialMock Serial;
SDMock SD;

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Result = MiniAcid::MelodySlotResult;

Buffer makeMelody(uint8_t firstNote) {
  Buffer melody{};
  melody.count = 4;
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  for (uint16_t i = 0; i < 4; ++i) {
    melody.events[i].startTick = static_cast<uint16_t>(i * 96);
    melody.events[i].durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
    melody.events[i].note = static_cast<uint8_t>(firstNote + i * 2);
    melody.events[i].velocity = 100;
    melody.events[i].probability = 100;
  }
  return melody;
}

bool sameNotes(const Buffer& a, const Buffer& b) {
  if (a.count != b.count || a.lengthTicks != b.lengthTicks) return false;
  for (uint16_t i = 0; i < a.count; ++i) {
    if (a.events[i].note != b.events[i].note ||
        a.events[i].startTick != b.events[i].startTick) {
      return false;
    }
  }
  return true;
}

void acceptInto(MiniAcid& engine, int bank, int pattern, const Buffer& melody) {
  if (engine.current303BankIndex(0) != bank) engine.set303BankIndex(0, bank);
  engine.set303PatternIndex(0, pattern);
  engine.workingMaterial_[0].storeMelody(melody);
  const auto acceptResult = engine.acceptMaterialWorking(0);
  assert(acceptResult == MiniAcid::AcceptResult::Accepted);
  assert(engine.isMelodySlot(0, bank, pattern));
  assert(!engine.hasUnsavedWorkingMelody(0));
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_test_melody_slots";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);

  const std::string proj = "melody_slots";
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");

  MiniAcid engine{44100.0f, &storage};
  engine.init();
  // Q..I select slots only outside Song mode (Song drives the slot there).
  engine.setSongMode(false);

  const Buffer melodyQ = makeMelody(48);  // bank A slot 1 (Q)
  const Buffer melodyE = makeMelody(60);  // bank A slot 3 (E)
  const Buffer melodyB2 = makeMelody(36); // bank B slot 2 (W)
  acceptInto(engine, 0, 0, melodyQ);
  acceptInto(engine, 1, 1, melodyB2);
  acceptInto(engine, 0, 2, melodyE);
  assert(!engine.isMelodySlot(0, 0, 1));

  std::unique_ptr<Buffer> loaded(new Buffer());

  // 1. Moving to a Melody slot loads exactly that slot's Melody.
  assert(engine.prepareMelodySlot(0, 0, 0, *loaded) == Result::Ready);
  assert(sameNotes(*loaded, melodyQ));
  assert(engine.activateMelodySlot(0, 0, 0, *loaded));
  assert(engine.display303LocalPatternIndex(0) == 0);
  assert(engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase);
  assert(sameNotes(engine.workingMaterial_[0].melody(), melodyQ));
  assert(!engine.hasUnsavedWorkingMelody(0));
  std::puts("MSLOT-1 PASS: Q..I loads the accepted Melody of the target slot");

  // 2. A slot without a Melody is refused and nothing moves.
  assert(engine.prepareMelodySlot(0, 0, 1, *loaded) == Result::NoMelody);
  assert(engine.display303LocalPatternIndex(0) == 0);
  std::puts("MSLOT-2 PASS: an empty slot is refused");

  // 3. An unsaved edit blocks the move instead of being dropped.
  engine.workingMaterial_[0].melodyIfHeld()->events[0].note = 90;
  assert(engine.hasUnsavedWorkingMelody(0));
  assert(engine.prepareMelodySlot(0, 0, 2, *loaded) == Result::Unsaved);
  assert(!engine.activateMelodySlot(0, 0, 2, melodyE));
  assert(engine.display303LocalPatternIndex(0) == 0);
  assert(engine.workingMaterial_[0].melody().events[0].note == 90);
  std::puts("MSLOT-3 PASS: unsaved edits block the move and survive");

  // 4. Reverting the edit makes the Melody clean again.
  engine.workingMaterial_[0].melodyIfHeld()->events[0].note = melodyQ.events[0].note;
  assert(!engine.hasUnsavedWorkingMelody(0));
  assert(engine.prepareMelodySlot(0, 0, 2, *loaded) == Result::Ready);
  assert(sameNotes(*loaded, melodyE));
  assert(engine.activateMelodySlot(0, 0, 2, *loaded));
  assert(sameNotes(engine.workingMaterial_[0].melody(), melodyE));
  std::puts("MSLOT-4 PASS: a clean Melody moves to the next Melody slot");

  // 5. A Melody carried onto another slot is not that slot's saved Melody.
  engine.set303PatternIndex(0, 3);
  assert(engine.hasUnsavedWorkingMelody(0));
  assert(engine.prepareMelodySlot(0, 0, 0, *loaded) == Result::Unsaved);
  engine.set303PatternIndex(0, 2);
  assert(!engine.hasUnsavedWorkingMelody(0));
  std::puts("MSLOT-5 PASS: a Melody on a foreign slot counts as unsaved");

  // 6. B switches to the same slot position in the other bank.
  assert(engine.prepareMelodySlot(0, 1, 2, *loaded) == Result::NoMelody);
  assert(engine.prepareMelodySlot(0, 1, 1, *loaded) == Result::Ready);
  assert(sameNotes(*loaded, melodyB2));
  assert(engine.activateMelodySlot(0, 1, 1, *loaded));
  assert(engine.current303BankIndex(0) == 1);
  assert(sameNotes(engine.workingMaterial_[0].melody(), melodyB2));
  std::puts("MSLOT-6 PASS: bank switch lands on the other bank's Melody");

  // 7. Selecting the current slot is a no-op.
  assert(engine.prepareMelodySlot(0, 1, 1, *loaded) == Result::AlreadyCurrent);
  std::puts("MSLOT-7 PASS: current slot is a no-op");

  // 8. Reported bug: saved Melody on Q, back to STEPS, move to another slot,
  //    Alt+R. The new slot must get a Melody of its OWN steps, not Q's.
  assert(engine.prepareMelodySlot(0, 0, 0, *loaded) == Result::Ready);
  assert(engine.activateMelodySlot(0, 0, 0, *loaded));
  assert(!engine.hasUnsavedWorkingMelody(0));
  assert(PhraseSourceToggle::toggle(engine, nullptr, 0) ==
         PhraseSourceToggle::Result::SwitchedToPattern);
  {
    SynthPattern& slot4 = engine.sceneManager().currentScene().synthABanks[0].patterns[4];
    for (auto& step : slot4.steps) step = SynthStep{};
    slot4.steps[0].note = 72;
    slot4.steps[8].note = 75;
  }
  assert(engine.tryManual303TargetSwitch(0, 4));
  assert(!engine.workingMaterial_[0].holdsMelody());
  assert(PhraseSourceToggle::toggle(engine, nullptr, 0) ==
         PhraseSourceToggle::Result::MadePhrase);
  const Buffer& made = engine.workingMaterial_[0].melody();
  assert(made.count == 2 && made.events[0].note == 72 && made.events[1].note == 75);
  assert(engine.hasUnsavedWorkingMelody(0));
  std::puts("MSLOT-8 PASS: Alt+R on a new slot uses that slot's steps, not the previous Melody");

  // 9. On STEPS an unsaved Melody blocks the slot switch; after ACCEPT the
  //    saved Melody stays on SD and Working follows the slot.
  assert(PhraseSourceToggle::toggle(engine, nullptr, 0) ==
         PhraseSourceToggle::Result::SwitchedToPattern);
  assert(!engine.tryManual303TargetSwitch(0, 2));
  assert(engine.display303LocalPatternIndex(0) == 4);
  assert(engine.workingMaterial_[0].holdsMelody());
  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
  assert(engine.tryManual303TargetSwitch(0, 2));
  assert(!engine.workingMaterial_[0].holdsMelody());
  std::puts("MSLOT-9 PASS: STEPS slot switch blocks unsaved and releases saved Melody");

  // 10. Alt+R on a slot that already holds an accepted Melody loads it.
  assert(PhraseSourceToggle::toggle(engine, nullptr, 0) ==
         PhraseSourceToggle::Result::SwitchedToPhrase);
  assert(sameNotes(engine.workingMaterial_[0].melody(), melodyE));
  assert(!engine.hasUnsavedWorkingMelody(0));
  std::puts("MSLOT-10 PASS: Alt+R on a Melody slot loads its saved Melody");

  std::puts("M2 melody slot navigation: PASS");
  return 0;
}
