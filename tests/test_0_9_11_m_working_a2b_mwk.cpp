#include <cassert>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

SerialMock Serial;
SDMock SD;

int main() {
  using GroovePuterMaterial::MaterialAddress;
  using GroovePuterMaterial::MaterialId;
  using GroovePuterMaterial::MaterialReference;

  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager().loadDefaultScene();

  Scene& scene = engine.sceneManager().currentScene();
  SynthPattern& accepted = scene.synthABanks[0].patterns[0];
  accepted.steps[0].note = 60;

  const int page = engine.currentPageIndex();
  const int bank = engine.current303BankIndex(0);
  const int pattern = engine.display303LocalPatternIndex(0);
  assert(page == 0);
  assert(bank == 0);
  assert(pattern == 0);

  constexpr MaterialAddress address{0, 0};
  constexpr MaterialId idM{101};
  constexpr MaterialId idN{102};
  constexpr MaterialReference refM{address, idM};
  constexpr MaterialReference refN{address, idN};

  scene.materialSlots[0][0].id = idM;

  // No retained session payload means no user modification.
  assert(engine.workingMaterial_[0].empty());
  assert(!engine.hasModifiedWorking303Pattern(0));

  // A Working copy bound to the current material identity is clean while it is
  // musically equal to ACCEPTED.
  engine.workingMaterial_[0].storePattern(accepted, refM);
  assert(engine.currentWorking303Pattern(0) != nullptr);
  assert(!engine.hasModifiedWorking303Pattern(0));

  // A musical delta on that exact material identity is modified.
  SynthPattern working = accepted;
  working.steps[0].note = 61;
  engine.workingMaterial_[0].storePattern(working, refM);
  assert(engine.hasModifiedWorking303Pattern(0));

  // Playback representation is not material identity. Pattern -> Phrase ->
  // Pattern must not erase the retained modified-state verdict.
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  assert(engine.hasModifiedWorking303Pattern(0));
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
  assert(engine.hasModifiedWorking303Pattern(0));

  // A2-B ABA witness: the physical/global address is unchanged, but the slot
  // now contains a different MaterialId. The old M@A Working value must be
  // foreign to N@A and therefore cannot dirty or render as the current target.
  scene.materialSlots[0][0].id = idN;
  assert(!engine.hasModifiedWorking303Pattern(0));
  assert(engine.currentWorking303Pattern(0) == nullptr);

  // Rebinding an explicit Working value to N@A restores exact identity and the
  // same musical delta becomes modified for N.
  engine.workingMaterial_[0].storePattern(working, refN);
  assert(engine.hasModifiedWorking303Pattern(0));
  assert(engine.currentWorking303Pattern(0) != nullptr);

  // Invalid/unassigned identity fails closed instead of degrading to address.
  scene.materialSlots[0][0].id = MaterialId{};
  assert(!engine.hasModifiedWorking303Pattern(0));
  assert(engine.currentWorking303Pattern(0) == nullptr);

  std::puts("M-WORKING A2-B identity-bound modified-state: PASS");
  return 0;
}
