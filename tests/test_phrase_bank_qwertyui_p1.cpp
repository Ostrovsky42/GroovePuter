// Phrase Bank QWERTYUI P1 characterization.
//
// The bank is an address/selection layer over existing material ownership.
// It is deliberately NOT a sequencer and does not own PLAY state. Audible
// material remains authoritative in MiniAcid::activeMaterial(). This state
// object owns only the user's EDIT target for Synth A/B.

#include <cstdio>

#include "src/phrase/phrase_bank_state.h"

namespace {

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "Phrase Bank P1 FAIL: %s\n", message);
  ++g_failures;
}

}  // namespace

int main() {
  using PhraseBank::PhraseBankState;

  static_assert(PhraseBank::kPhraseBankSlotCount == 8,
                "QWERTYUI must address exactly eight Phrase slots");
  static_assert(sizeof(PhraseBankState) <= 2,
                "Phrase Bank state may only retain one EDIT byte per synth voice");

  constexpr char keys[PhraseBank::kPhraseBankSlotCount] = {
      'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'};

  for (uint8_t slot = 0; slot < PhraseBank::kPhraseBankSlotCount; ++slot) {
    uint8_t decoded = 0xFF;
    expect(PhraseBank::slotForKey(keys[slot], decoded),
           "an uppercase QWERTYUI key was not recognized");
    expect(decoded == slot, "QWERTYUI key mapped to the wrong slot");
    expect(PhraseBank::keyForSlot(slot) == keys[slot],
           "slot did not map back to its stable QWERTYUI identity");

    decoded = 0xFF;
    const char lower = static_cast<char>(keys[slot] - 'A' + 'a');
    expect(PhraseBank::slotForKey(lower, decoded),
           "a lowercase qwertyui key was not recognized");
    expect(decoded == slot, "lowercase qwertyui mapped to the wrong slot");
  }

  {
    uint8_t slot = 7;
    expect(!PhraseBank::slotForKey('A', slot),
           "a non-bank key was accepted as a Phrase slot");
    expect(slot == 7, "rejected key mutated the caller's slot value");
    expect(PhraseBank::keyForSlot(8) == '-',
           "out-of-range slot did not render as unavailable");
  }

  PhraseBankState state;
  expect(state.editingSlot(0) == 0 && state.editingSlot(1) == 0,
         "Synth A/B EDIT targets must start deterministically at Q");

  expect(state.selectEditingSlot(0, 3), "Synth A could not select EDIT R");
  expect(state.editingSlot(0) == 3, "Synth A EDIT target did not become R");
  expect(state.editingSlot(1) == 0,
         "editing Synth A changed Synth B's independent EDIT target");

  expect(state.selectEditingSlot(1, 6), "Synth B could not select EDIT U");
  expect(state.editingSlot(0) == 3,
         "editing Synth B changed Synth A's independent EDIT target");
  expect(state.editingSlot(1) == 6, "Synth B EDIT target did not become U");

  expect(!state.selectEditingSlot(0, 8),
         "out-of-range Phrase slot was accepted");
  expect(state.editingSlot(0) == 3,
         "rejected slot changed the existing EDIT target");
  expect(!state.selectEditingSlot(2, 1),
         "out-of-range synth voice was accepted");

  if (g_failures == 0) {
    std::printf("Phrase Bank QWERTYUI P1 state: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "Phrase Bank QWERTYUI P1 state: %d failure(s)\n",
               g_failures);
  return 1;
}
