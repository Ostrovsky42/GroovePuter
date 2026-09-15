// 0.9.12 Material Closure: Material Working LENGTH ownership (FS2B-L1)
//
// Proves that LENGTH 1/2/4/8 is an atomic Material Working mutation across the
// Pattern/Melody representation seam, preserving Material identity and canonical
// Accepted truth, with exact before-image Undo and strict safe-shortening semantics.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/state/material_slot_access.h"
#include "src/state/material_version.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialId;
using GroovePuterMaterial::MaterialKind;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialVersionToken;
using GroovePuterMaterial::versionForPattern;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "LENGTH FAIL: %s\n", message);
  ++g_failures;
}

bool samePattern(const SynthPattern& lhs, const SynthPattern& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(SynthPattern)) == 0;
}

bool sameReference(const MaterialReference& lhs, const MaterialReference& rhs) {
  return lhs.address.voice == rhs.address.voice &&
         lhs.address.globalSlot == rhs.address.globalSlot &&
         lhs.id == rhs.id;
}

bool sameMelody(const PhraseRuntime::RuntimeSynthEventBuffer& lhs,
                const PhraseRuntime::RuntimeSynthEventBuffer& rhs) {
  if (lhs.count != rhs.count || lhs.lengthTicks != rhs.lengthTicks) return false;
  for (uint16_t i = 0; i < lhs.count; ++i) {
    const auto& a = lhs.events[i];
    const auto& b = rhs.events[i];
    if (a.startTick != b.startTick ||
        a.durationSubticks != b.durationSubticks ||
        a.note != b.note ||
        a.velocity != b.velocity ||
        a.probability != b.probability ||
        a.flags != b.flags ||
        a.fx != b.fx ||
        a.fxParam != b.fxParam) {
      return false;
    }
  }
  return true;
}

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};

  static MaterialId fixtureMaterialId(int voice, int resident) {
    return MaterialId{static_cast<uint32_t>(
        1001 + voice * Scene::kMaterialSlotsPerVoice + resident)};
  }

  Fixture() {
    engine.setBpm(120.0f);
    Scene& scene = engine.sceneManager_.currentScene();
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
      for (int resident = 0; resident < Scene::kMaterialSlotsPerVoice;
           ++resident) {
        scene.materialSlots[voice][resident].kind = MaterialKind::Pattern;
        scene.materialSlots[voice][resident].id =
            fixtureMaterialId(voice, resident);
      }
    }

    // Give voice A steps distinct notes so we can verify content preservation
    canonical(0).steps[0].note = 60;
    canonical(0).steps[4].note = 63;
    canonical(0).steps[8].note = 67;
    canonical(0).steps[12].note = 70;

    canonical(1).steps[0].note = 48;
    assert(engine.rebuildPatternRuntimeEventBank());
    assertCanonicalReality(0);
    assertCanonicalReality(1);
  }

  MaterialReference reference(int voice) const {
    MaterialReference out{};
    assert(engine.current303MaterialReference_(voice, out));
    return out;
  }

  SynthPattern& canonical(int voice) {
    const int bank = engine.current303BankIndex(voice);
    const int pattern = engine.display303LocalPatternIndex(voice);
    Scene& scene = engine.sceneManager_.currentScene();
    return voice == 0 ? scene.synthABanks[bank].patterns[pattern]
                      : scene.synthBBanks[bank].patterns[pattern];
  }

  const SynthPattern& canonical(int voice) const {
    const int bank = engine.current303BankIndex(voice);
    const int pattern = engine.display303LocalPatternIndex(voice);
    const Scene& scene = engine.sceneManager_.currentScene();
    return voice == 0 ? scene.synthABanks[bank].patterns[pattern]
                      : scene.synthBBanks[bank].patterns[pattern];
  }

  void assertCanonicalReality(int voice) const {
    const MaterialReference ref = reference(voice);
    const int resident = GroovePuterMaterial::residentSlotFor(ref.address);
    assert(GroovePuterMaterial::residentSlotInRange(voice, resident));
    const auto& descriptor =
        engine.sceneManager_.currentScene().materialSlots[voice][resident];
    assert(descriptor.kind == MaterialKind::Pattern);
    assert(descriptor.id.valid());
    assert(descriptor.id == ref.id);
    assert(versionForPattern(canonical(voice)).valid());
  }

  bool isCurrentDirty(int voice) const {
    GroovePuterMaterial::MaterialReference ref{};
    GroovePuterMaterial::MaterialVersionToken acceptedVersion{};
    const auto state = engine.classifyCurrentForNext_(voice, ref, acceptedVersion);
    return state == MiniAcid::CurrentNextState::DirtyCurrent;
  }
};

}  // namespace

int main() {
  // L1 — Pattern LENGTH growth: Pattern 1 -> LENGTH 2/4/8 succeeds via Material API
  {
    // Pattern 1 -> 2 bars
    {
      Fixture fixture;
      expect(!fixture.isCurrentDirty(0), "L1: start clean");
      expect(fixture.engine.workingMaterial_[0].empty(), "L1: working empty");
      expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
             "L1: active material Pattern");

      const auto res = fixture.engine.setMaterialLength(0, 2);
      expect(res == MiniAcid::MaterialLengthResult::Changed, "L1: 1->2 Changed");
      expect(fixture.engine.workingMaterial_[0].holdsMelody(), "L1: holds Melody");
      expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 2 * PhraseRuntime::kTicksPerBar,
             "L1: 2 bars lengthTicks");
      expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Melody, "L1: kind Melody");
    }
    // Pattern 1 -> 4 bars
    {
      Fixture fixture;
      const auto res = fixture.engine.setMaterialLength(0, 4);
      expect(res == MiniAcid::MaterialLengthResult::Changed, "L1: 1->4 Changed");
      expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 4 * PhraseRuntime::kTicksPerBar,
             "L1: 4 bars lengthTicks");
    }
    // Pattern 1 -> 8 bars
    {
      Fixture fixture;
      const auto res = fixture.engine.setMaterialLength(0, 8);
      expect(res == MiniAcid::MaterialLengthResult::Changed, "L1: 1->8 Changed");
      expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 8 * PhraseRuntime::kTicksPerBar,
             "L1: 8 bars lengthTicks");
    }
  }

  // L2 — Melody LENGTH: Melody -> LENGTH 1/2/4/8 uses same Material-level operation
  {
    Fixture fixture;
    expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
           "L2: set to 4 bars first");
    expect(fixture.engine.workingMaterial_[0].holdsMelody(), "L2: working holds Melody");

    // Melody 4 bars -> 8 bars using exact same setMaterialLength API
    const auto result8 = fixture.engine.setMaterialLength(0, 8);
    expect(result8 == MiniAcid::MaterialLengthResult::Changed,
           "L2: setMaterialLength(0, 8) on Melody must return Changed");
    expect(fixture.engine.workingMaterial_[0].holdsMelody(), "L2: remains Melody");
    expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 8 * PhraseRuntime::kTicksPerBar,
           "L2: lengthTicks 8 bars");

    // Melody 8 bars -> 4 bars (bars 5..8 are empty)
    const auto result4 = fixture.engine.setMaterialLength(0, 4);
    expect(result4 == MiniAcid::MaterialLengthResult::Changed,
           "L2: setMaterialLength(0, 4) on Melody must return Changed");
    expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 4 * PhraseRuntime::kTicksPerBar,
           "L2: lengthTicks 4 bars");

    // Melody 4 bars -> 2 bars (bars 3..4 are empty)
    const auto result2 = fixture.engine.setMaterialLength(0, 2);
    expect(result2 == MiniAcid::MaterialLengthResult::Changed,
           "L2: setMaterialLength(0, 2) on Melody must return Changed");
    expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 2 * PhraseRuntime::kTicksPerBar,
           "L2: lengthTicks 2 bars");
  }

  // L3 — Identity preservation: Pattern -> Melody LENGTH preserves MaterialId, reference, version
  {
    Fixture fixture;
    const auto refBefore = fixture.reference(0);
    const auto versionBefore = versionForPattern(fixture.canonical(0));
    const auto acceptedBefore = fixture.canonical(0);
    const auto slotBefore = fixture.engine.activeMaterial(0).slot;

    expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
           "L3: growth to 4 bars");
    expect(sameReference(fixture.reference(0), refBefore),
           "L3: MaterialReference/MaterialId must remain unchanged");
    expect(fixture.engine.activeMaterial(0).slot == slotBefore,
           "L3: active slot must remain unchanged");
    expect(versionForPattern(fixture.canonical(0)) == versionBefore,
           "L3: canonical version token must remain unchanged");
    expect(samePattern(fixture.canonical(0), acceptedBefore),
           "L3: accepted canonical pattern content must remain unchanged");
  }

  // L4 — Single atomic commit: failure leaves Working untouched
  {
    Fixture fixture;
    expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
           "L4: setup 4 bars");
    auto& melody = fixture.engine.workingMaterial_[0].melody();
    // Add an event in bar 3 (tick = 2 * 384 + 10)
    melody.events[melody.count].startTick = 2 * PhraseRuntime::kTicksPerBar + 10;
    melody.events[melody.count].durationSubticks = 16 * PhraseRuntime::kSubticksPerTick;
    melody.events[melody.count].note = 72;
    melody.events[melody.count].velocity = 100;
    melody.events[melody.count].probability = 100;
    ++melody.count;

    const auto snapshotBefore = melody;
    // Request length 1 bar: would truncate event in bar 3!
    const auto failResult = fixture.engine.setMaterialLength(0, 1);
    expect(failResult == MiniAcid::MaterialLengthResult::WouldTruncate,
           "L4: shrink that would truncate events must return WouldTruncate");
    expect(sameMelody(fixture.engine.workingMaterial_[0].melody(), snapshotBefore),
           "L4: failed length request must leave Working exact before-image");
  }

  // L5 — Dirty transition: clean false->true; existing dirty true->true
  {
    // Clean -> dirty
    {
      Fixture fixture;
      expect(!fixture.isCurrentDirty(0), "L5 clean: must start clean");
      expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
             "L5 clean: set to 4 bars");
      expect(fixture.isCurrentDirty(0), "L5 clean: must become dirty after LENGTH");
    }
    // Existing dirty -> dirty
    {
      Fixture fixture;
      expect(fixture.engine.adjustWorking303StepNote(0, 0, 1),
             "L5 dirty: make pattern dirty first");
      expect(fixture.isCurrentDirty(0), "L5 dirty: pattern is dirty");
      expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
             "L5 dirty: set to 4 bars");
      expect(fixture.isCurrentDirty(0), "L5 dirty: must remain dirty");
    }
  }

  // L6 — One Undo receipt: exactly one receipt published for one gesture
  {
    Fixture fixture;
    auto& undo = GroovePuterUndo::undoOwner();
    undo.clear();
    expect(!undo.hasUndo(), "L6: undo starts empty");

    expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
           "L6: set length 4");
    expect(undo.hasUndo(), "L6: undo receipt must exist after LENGTH");
    expect(undo.kind() == GroovePuterUndo::UndoKind::RuntimePhrase,
           "L6: undo kind must be runtime/working receipt");
  }

  // L7 — Exact Undo before-image
  {
    // 7A: Clean Pattern 1 bar -> LENGTH 4 -> Undo restores exact Clean Pattern representation
    {
      Fixture fixture;
      expect(!fixture.isCurrentDirty(0), "L7A: start clean");
      expect(fixture.engine.workingMaterial_[0].empty(), "L7A: working empty");

      expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
             "L7A: set to 4 bars");
      expect(fixture.engine.workingMaterial_[0].holdsMelody(), "L7A: now holds Melody");
      expect(fixture.isCurrentDirty(0), "L7A: now dirty");

      expect(fixture.engine.undoMaterialWorking(0), "L7A: undo must succeed");
      expect(fixture.engine.workingMaterial_[0].empty(),
             "L7A: undo must restore empty working storage for clean pattern");
      expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
             "L7A: undo must restore Pattern representation (not equivalent Melody!)");
      expect(fixture.engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Pattern,
             "L7A: undo must restore Pattern sequenced source");
      expect(!fixture.isCurrentDirty(0), "L7A: undo must restore clean dirty state");
    }

    // 7B: Dirty Pattern 1 bar -> LENGTH 4 -> Undo restores exact Dirty Pattern representation
    {
      Fixture fixture;
      expect(fixture.engine.adjustWorking303StepNote(0, 0, 1), "L7B: make pattern dirty");
      const auto dirtyPattern = fixture.engine.workingMaterial_[0].pattern();
      expect(fixture.engine.workingMaterial_[0].holdsPattern(), "L7B: holds dirty pattern");
      expect(fixture.isCurrentDirty(0), "L7B: starts dirty");

      expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
             "L7B: set to 4 bars");
      expect(fixture.engine.workingMaterial_[0].holdsMelody(), "L7B: now holds Melody");

      expect(fixture.engine.undoMaterialWorking(0), "L7B: undo must succeed");
      expect(fixture.engine.workingMaterial_[0].holdsPattern(),
             "L7B: undo must restore dirty Pattern representation");
      expect(samePattern(fixture.engine.workingMaterial_[0].pattern(), dirtyPattern),
             "L7B: undo must restore exact dirty pattern contents");
      expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
             "L7B: active kind must be Pattern");
      expect(fixture.isCurrentDirty(0), "L7B: undo must restore dirty state as true");
    }
  }

  // L8 — Failure preserves history and state
  {
    Fixture fixture;
    expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
           "L8: setup 4 bars");
    const auto melodyBefore = fixture.engine.workingMaterial_[0].melody();
    auto& undo = GroovePuterUndo::undoOwner();
    undo.clear();

    // Invalid bar length (e.g. 3, 5, 0)
    expect(fixture.engine.setMaterialLength(0, 3) == MiniAcid::MaterialLengthResult::InvalidLength,
           "L8: 3 bars must be rejected as InvalidLength");
    expect(sameMelody(fixture.engine.workingMaterial_[0].melody(), melodyBefore),
           "L8: invalid length must leave Working untouched");
    expect(!undo.hasUndo(),
           "L8: invalid length must not touch Undo history");

    // Invalid voice (-1, 2)
    expect(fixture.engine.setMaterialLength(-1, 2) == MiniAcid::MaterialLengthResult::InvalidVoice,
           "L8: -1 voice must be rejected as InvalidVoice");
    expect(fixture.engine.setMaterialLength(NUM_303_VOICES, 2) == MiniAcid::MaterialLengthResult::InvalidVoice,
           "L8: NUM_303_VOICES must be rejected as InvalidVoice");
    expect(!undo.hasUndo(),
           "L8: invalid voice must not touch Undo history");
  }

  // Structural assertion: LENGTH != REPEAT
  {
    Fixture fixture;
    expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
           "Structural: set to 4 bars");
    const auto& melody = fixture.engine.workingMaterial_[0].melody();
    expect(melody.lengthTicks == 4 * PhraseRuntime::kTicksPerBar,
           "Structural: length must be 4 bars");

    // In bar 1 (ticks 0..383), events exist
    uint16_t bar1Count = 0;
    uint16_t subsequentBarsCount = 0;
    for (uint16_t i = 0; i < melody.count; ++i) {
      if (melody.events[i].startTick < PhraseRuntime::kTicksPerBar) {
        ++bar1Count;
      } else {
        ++subsequentBarsCount;
      }
    }
    expect(bar1Count > 0, "Structural: bar 1 must contain projected pattern notes");
    expect(subsequentBarsCount == 0,
           "Structural: bars 2..4 must be completely empty (LENGTH != REPEAT witness [A _ _ _] not [A A A A])");
  }

  // SHORTENING CONTRACT: Content loss => REJECT
  // Rule: Melody 4 -> 2:
  //   if bars 3-4 empty and no note crosses new boundary: OK
  //   if any musical information would be lost: REJECT, CURRENT bit-identical, no Undo receipt
  {
    // Case A: Shortening rejected when bars 3-4 contain musical events
    {
      Fixture fixture;
      expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
             "Shortening Case A: setup 4 bars");
      auto& melody = fixture.engine.workingMaterial_[0].melody();
      // Inject note in bar 3 (tick = 2 * 384 + 20)
      melody.events[melody.count].startTick = 2 * PhraseRuntime::kTicksPerBar + 20;
      melody.events[melody.count].durationSubticks = 16 * PhraseRuntime::kSubticksPerTick;
      melody.events[melody.count].note = 64;
      melody.events[melody.count].velocity = 90;
      melody.events[melody.count].probability = 100;
      ++melody.count;

      const auto snapshotBefore = melody;
      auto& undo = GroovePuterUndo::undoOwner();
      undo.clear();
      const bool dirtyBefore = fixture.isCurrentDirty(0);

      // Attempt 4 -> 2 shortening: must REJECT because bar 3 has a note!
      const auto res = fixture.engine.setMaterialLength(0, 2);
      expect(res == MiniAcid::MaterialLengthResult::WouldTruncate,
             "Shortening Case A: shortening with notes in bar 3 must be REJECTED with WouldTruncate");
      expect(sameMelody(fixture.engine.workingMaterial_[0].melody(), snapshotBefore),
             "Shortening Case A: rejected shortening leaves CURRENT bit-identical");
      expect(!undo.hasUndo(),
             "Shortening Case A: rejected shortening publishes NO Undo receipt");
      expect(fixture.isCurrentDirty(0) == dirtyBefore,
             "Shortening Case A: rejected shortening leaves dirty state unchanged");
    }

    // Case B: Shortening rejected when a note in bar 2 crosses boundary into bar 3
    {
      Fixture fixture;
      expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
             "Shortening Case B: setup 4 bars");
      auto& melody = fixture.engine.workingMaterial_[0].melody();
      // Inject note in bar 2 starting at tick 750 (boundary is 2 * 384 = 768)
      // Duration = 30 ticks (starts at 750, ends at 780 > 768)
      melody.events[melody.count].startTick = 750;
      melody.events[melody.count].durationSubticks = 30 * PhraseRuntime::kSubticksPerTick;
      melody.events[melody.count].note = 67;
      melody.events[melody.count].velocity = 100;
      melody.events[melody.count].probability = 100;
      ++melody.count;

      const auto snapshotBefore = melody;
      auto& undo = GroovePuterUndo::undoOwner();
      undo.clear();

      // Attempt 4 -> 2 shortening: must REJECT because note crosses tick 768 into bar 3!
      const auto res = fixture.engine.setMaterialLength(0, 2);
      expect(res == MiniAcid::MaterialLengthResult::WouldTruncate,
             "Shortening Case B: note crossing boundary into bar 3 must be REJECTED with WouldTruncate");
      expect(sameMelody(fixture.engine.workingMaterial_[0].melody(), snapshotBefore),
             "Shortening Case B: rejected boundary-crossing leaves CURRENT bit-identical");
      expect(!undo.hasUndo(),
             "Shortening Case B: rejected boundary-crossing publishes NO Undo receipt");
    }

    // Case C: Shortening succeeds when bars 3-4 are empty and no note crosses boundary
    {
      Fixture fixture;
      expect(fixture.engine.setMaterialLength(0, 4) == MiniAcid::MaterialLengthResult::Changed,
             "Shortening Case C: setup 4 bars");
      auto& undo = GroovePuterUndo::undoOwner();
      undo.clear();

      // Bars 3-4 are empty (from blank extension), notes only in bar 1 (ticks 0..383)
      const auto res = fixture.engine.setMaterialLength(0, 2);
      expect(res == MiniAcid::MaterialLengthResult::Changed,
             "Shortening Case C: shortening empty bars 3-4 must SUCCEED with Changed");
      expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 2 * PhraseRuntime::kTicksPerBar,
             "Shortening Case C: lengthTicks must now be 2 bars");
      expect(undo.hasUndo(),
             "Shortening Case C: successful shortening publishes an Undo receipt");

      GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
      expect(undo.read(GroovePuterUndo::UndoKind::RuntimePhrase, receipt),
             "Shortening Case C: receipt is readable");
      expect(receipt.before.lengthTicks == 4 * PhraseRuntime::kTicksPerBar,
             "Shortening Case C: receipt before length must be 4 bars");

      // Case D: Undo of shortening restores full 4-bar melody
      expect(fixture.engine.undoMaterialWorking(0),
             "Shortening Case D: undo shortening succeeds");
      expect(fixture.engine.workingMaterial_[0].melody().lengthTicks == 4 * PhraseRuntime::kTicksPerBar,
             "Shortening Case D: undo shortening restores 4-bar length");
    }
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "Material LENGTH contract: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("Material LENGTH contract: PASS\n");
  return 0;
}
