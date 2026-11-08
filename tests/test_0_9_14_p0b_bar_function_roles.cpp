// P0-B1 unit tests: bass/chord follow the phrase bar function (pure functions, no engine).
#include "src/generation/roles/bar_function_roles.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

using namespace GroovePuterRhythm;
namespace BFR = GroovePuterRhythm::BarFunctionRoles;

namespace {

uint32_t g_state = 0x12345678u;
uint32_t rnd() { g_state = g_state * 1664525u + 1013904223u; return g_state >> 8; }

bool eq(const BassRhythmPlan& a, const BassRhythmPlan& b) {
  return a.id == b.id && a.kickRelationship == b.kickRelationship && a.onsets == b.onsets && a.continuations == b.continuations;
}
bool eq(const ChordRhythmPlan& a, const ChordRhythmPlan& b) {
  return a.id == b.id && a.onsets == b.onsets && a.continuations == b.continuations && a.releasePoints == b.releasePoints;
}
bool subset(StepMask a, StepMask b) { return (a & ~b) == 0; }

// Random valid-looking plan: attacks with optional contiguous continuation chains.
void randomPlan(StepMask& onsets, StepMask& conts) {
  onsets = 0; conts = 0;
  const uint8_t target = static_cast<uint8_t>(rnd() % 7);
  for (uint8_t i = 0; i < target; ++i) {
    const uint8_t s = static_cast<uint8_t>(rnd() % kStepsPerBar);
    if ((onsets | conts) & stepBit(s)) continue;
    onsets = static_cast<StepMask>(onsets | stepBit(s));
    const uint8_t hold = static_cast<uint8_t>(rnd() % 3);
    for (uint8_t h = 1; h <= hold && s + h < kStepsPerBar; ++h) {
      const uint8_t c = static_cast<uint8_t>(s + h);
      if ((onsets | conts) & stepBit(c)) break;
      conts = static_cast<StepMask>(conts | stepBit(c));
    }
  }
}

constexpr BarFunction kFrozen[] = {BarFunction::Statement, BarFunction::Repeat, BarFunction::Return, BarFunction::Response,
                                   BarFunction::RepeatWithGhosts, BarFunction::Count};

}  // namespace

int main() {
  int cases = 0;
  for (int iter = 0; iter < 30000; ++iter) {
    BassRhythmPlan bass{};
    randomPlan(bass.onsets, bass.continuations);
    bass.id = BassRhythmId::KickAnswer;
    bass.kickRelationship = (rnd() % 4 == 0) ? RelationshipOp::FillGaps : RelationshipOp::Exclude;
    const StepMask kick = static_cast<StepMask>(rnd() & 0xFFFF);
    const StepMask prot = static_cast<StepMask>((rnd() % 3 == 0) ? (rnd() & 0xFFFF) : 0);

    ChordRhythmPlan chord{};
    randomPlan(chord.onsets, chord.continuations);
    chord.id = ChordRhythmId::Auto;
    chord.releasePoints = static_cast<StepMask>(rnd() & 0xFFFF);
    const StepMask baseBass = static_cast<StepMask>(rnd() & 0xFFFF);

    // 1. Frozen functions: bit-identical plans.
    for (BarFunction f : kFrozen) {
      assert(eq(BFR::applyToBassPlan(f, bass, kick, prot), bass));
      assert(eq(BFR::applyToChordPlan(f, chord, baseBass, prot), chord));
    }
    // Chord Turnaround is unchanged as well.
    assert(eq(BFR::applyToChordPlan(BarFunction::Turnaround, chord, baseBass, prot), chord));

    // 2. Bass Break: one existing attack, in place, no new bits.
    {
      const BassRhythmPlan r = BFR::applyToBassPlan(BarFunction::Break, bass, kick, prot);
      assert(r.id == bass.id && r.kickRelationship == bass.kickRelationship);
      if (bass.onsets == 0) { assert(eq(r, bass)); }
      else {
        const int first = BFR::firstStep(bass.onsets);
        assert(r.onsets == stepBit(static_cast<uint8_t>(first)));
        assert(subset(r.continuations, bass.continuations));
        assert(subset(r.continuations, BFR::continuationChain(bass.continuations, static_cast<uint8_t>(first))));
        assert(BFR::firstStep(r.onsets) == first);  // no downbeat is invented
      }
      assert(eq(BFR::applyToBassPlan(BarFunction::Break, r, kick, prot), r));  // idempotent
    }
    // 3. Bass Reduction: at most one attack, the last; anchor kept; chain of the removed goes.
    {
      const BassRhythmPlan r = BFR::applyToBassPlan(BarFunction::Reduction, bass, kick, prot);
      const uint8_t n = BFR::countSteps(bass.onsets);
      if (n < 2) { assert(eq(r, bass)); }
      else {
        assert(BFR::countSteps(r.onsets) == n - 1);
        assert(subset(r.onsets, bass.onsets) && subset(r.continuations, bass.continuations));
        assert(BFR::firstStep(r.onsets) == BFR::firstStep(bass.onsets));
        const int dropped = BFR::lastStep(bass.onsets);
        assert(!(r.onsets & stepBit(static_cast<uint8_t>(dropped))));
        assert((r.continuations & BFR::continuationChain(bass.continuations, static_cast<uint8_t>(dropped))) == 0);
      }
    }
    // 4/5. Bass Build / Turnaround: at most one legal added attack, never obligatory.
    for (BarFunction f : {BarFunction::Build, BarFunction::Turnaround}) {
      const BassRhythmPlan r = BFR::applyToBassPlan(f, bass, kick, prot);
      assert(r.continuations == bass.continuations && subset(bass.onsets, r.onsets));
      const StepMask added = static_cast<StepMask>(r.onsets & ~bass.onsets);
      assert(BFR::countSteps(added) <= 1);
      if (bass.kickRelationship != RelationshipOp::Exclude || bass.onsets == 0) { assert(added == 0); }
      if (added) {
        const uint8_t lo = f == BarFunction::Build ? 8 : 13;
        const int step = BFR::firstStep(added);
        assert(step >= lo);
        assert(!(added & (bass.onsets | bass.continuations | kick | prot)));
      }
    }
    // 6. Chord Break: silence, including continuations and releases.
    {
      const ChordRhythmPlan r = BFR::applyToChordPlan(BarFunction::Break, chord, baseBass, prot);
      assert(r.onsets == 0 && r.continuations == 0 && r.releasePoints == 0 && r.id == chord.id);
    }
    // 7. Chord Reduction: at most one attack; empty allowed; nothing new.
    {
      const ChordRhythmPlan r = BFR::applyToChordPlan(BarFunction::Reduction, chord, baseBass, prot);
      assert(subset(r.onsets, chord.onsets) && subset(r.continuations, chord.continuations) &&
             subset(r.releasePoints, chord.releasePoints));
      assert(BFR::countSteps(chord.onsets) - BFR::countSteps(r.onsets) == (chord.onsets ? 1 : 0));
    }
    // 8. Chord Build: only short plans, never onto a base bass attack or protected space.
    {
      const ChordRhythmPlan r = BFR::applyToChordPlan(BarFunction::Build, chord, baseBass, prot);
      const StepMask added = static_cast<StepMask>(r.onsets & ~chord.onsets);
      assert(BFR::countSteps(added) <= 1 && subset(chord.onsets, r.onsets));
      assert(r.continuations == chord.continuations && r.releasePoints == chord.releasePoints);
      if (chord.onsets == 0 || chord.continuations != 0) { assert(added == 0); }
      if (added) {
        assert(BFR::firstStep(added) >= 8);
        assert(!(added & (chord.onsets | chord.continuations | baseBass | prot)));
      }
    }
    ++cases;
  }

  // Concrete witnesses.
  {
    BassRhythmPlan p{};
    p.onsets = static_cast<StepMask>(stepBit(3) | stepBit(6) | stepBit(10));
    p.continuations = static_cast<StepMask>(stepBit(4) | stepBit(11) | stepBit(12));
    // Break keeps the earliest attack in place (step 3, NOT the downbeat) and its own chain (4).
    const BassRhythmPlan br = BFR::applyToBassPlan(BarFunction::Break, p, 0, 0);
    assert(br.onsets == stepBit(3) && br.continuations == stepBit(4));
    // Reduction removes the last attack (10) and its chain (11, 12).
    const BassRhythmPlan rd = BFR::applyToBassPlan(BarFunction::Reduction, p, 0, 0);
    assert(rd.onsets == static_cast<StepMask>(stepBit(3) | stepBit(6)) && rd.continuations == stepBit(4));
    // Build with only kick-free, free steps in the second half: farthest legal step from attacks.
    const StepMask kick = static_cast<StepMask>(stepBit(8) | stepBit(9) | stepBit(13));
    const BassRhythmPlan bd = BFR::applyToBassPlan(BarFunction::Build, p, kick, 0);
    assert(BFR::countSteps(bd.onsets & ~p.onsets) == 1);
    // No legal place: everything in [8,15] blocked -> plan untouched.
    assert(eq(BFR::applyToBassPlan(BarFunction::Build, p, 0xFFFF, 0), p));
    // A position freed by removing a bass attack is NOT offered to the chord (base bass onsets are passed).
    ChordRhythmPlan c{};
    c.onsets = stepBit(0);
    const StepMask baseBass = static_cast<StepMask>(stepBit(8) | stepBit(9) | stepBit(10) | stepBit(11) | stepBit(12) |
                                                    stepBit(13) | stepBit(14));
    const ChordRhythmPlan cb = BFR::applyToChordPlan(BarFunction::Build, c, baseBass, 0);
    assert(cb.onsets == static_cast<StepMask>(stepBit(0) | stepBit(15)));
  }
  std::printf("P0-B1 bar-function roles: %d random plans, all invariants hold: PASS\n", cases);
  return 0;
}
