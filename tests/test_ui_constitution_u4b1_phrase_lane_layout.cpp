#include <cassert>
#include <cstdint>

#include "src/ui/phrase_notes_lane_layout.h"

namespace {

PhraseRuntime::RuntimeSynthEvent event(uint16_t startTick,
                                       uint16_t durationTicks,
                                       uint8_t note) {
  PhraseRuntime::RuntimeSynthEvent out{};
  out.startTick = startTick;
  out.durationSubticks = static_cast<uint16_t>(
      durationTicks * PhraseRuntime::kSubticksPerTick);
  out.note = note;
  out.velocity = 100;
  out.probability = 100;
  return out;
}

int findByStartAndNote(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                       uint16_t startTick,
                       uint8_t note) {
  for (uint16_t i = 0; i < phrase.count; ++i) {
    if (phrase.events[i].startTick == startTick && phrase.events[i].note == note) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

uint8_t laneFor(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                const PhraseNotesLaneLayout::Layout& layout,
                uint16_t startTick,
                uint8_t note) {
  const int index = findByStartAndNote(phrase, startTick, note);
  assert(index >= 0);
  return layout.laneByEvent[index];
}

}  // namespace

int main() {
  using PhraseNotesLaneLayout::Layout;

  // A normal monophonic Phrase stays on one temporal lane regardless of
  // buffer order. Y is not an event ordinal.
  PhraseRuntime::RuntimeSynthEventBuffer mono{};
  mono.lengthTicks = 4 * PhraseRuntime::kTicksPerBar;
  mono.count = 3;
  mono.events[0] = event(192, 48, 67);
  mono.events[1] = event(0, 48, 60);
  mono.events[2] = event(96, 48, 64);

  const Layout monoLayout = PhraseNotesLaneLayout::build(
      mono, 0, 2 * PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick, 4);
  assert(monoLayout.laneCount == 1);
  assert(monoLayout.overflowCount == 0);
  assert(laneFor(mono, monoLayout, 0, 60) == 0);
  assert(laneFor(mono, monoLayout, 96, 64) == 0);
  assert(laneFor(mono, monoLayout, 192, 67) == 0);

  // Actual temporal overlap gets deterministic visual separation.
  PhraseRuntime::RuntimeSynthEventBuffer overlap{};
  overlap.lengthTicks = 4 * PhraseRuntime::kTicksPerBar;
  overlap.count = 3;
  overlap.events[0] = event(24, 96, 64);  // overlaps both neighbours
  overlap.events[1] = event(0, 96, 60);
  overlap.events[2] = event(48, 96, 67);

  const Layout overlapLayout = PhraseNotesLaneLayout::build(
      overlap, 0, 2 * PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick, 4);
  const uint8_t laneA = laneFor(overlap, overlapLayout, 0, 60);
  const uint8_t laneB = laneFor(overlap, overlapLayout, 24, 64);
  const uint8_t laneC = laneFor(overlap, overlapLayout, 48, 67);
  assert(laneA != laneB);
  assert(laneA != laneC);
  assert(laneB != laneC);
  assert(overlapLayout.laneCount == 3);

  // Reordering the backing buffer must not change lanes for distinct musical
  // events. Buffer index is not spatial identity.
  PhraseRuntime::RuntimeSynthEventBuffer reordered = overlap;
  reordered.events[0] = overlap.events[2];
  reordered.events[1] = overlap.events[0];
  reordered.events[2] = overlap.events[1];
  const Layout reorderedLayout = PhraseNotesLaneLayout::build(
      reordered, 0, 2 * PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick, 4);
  assert(laneFor(reordered, reorderedLayout, 0, 60) == laneA);
  assert(laneFor(reordered, reorderedLayout, 24, 64) == laneB);
  assert(laneFor(reordered, reorderedLayout, 48, 67) == laneC);

  // An unrelated, non-overlapping event may be inserted anywhere in the
  // backing array without moving the existing musical objects vertically.
  PhraseRuntime::RuntimeSynthEventBuffer withUnrelated = overlap;
  withUnrelated.count = 4;
  withUnrelated.events[3] = event(600, 24, 72);
  const Layout withUnrelatedLayout = PhraseNotesLaneLayout::build(
      withUnrelated, 0,
      2 * PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick, 4);
  assert(laneFor(withUnrelated, withUnrelatedLayout, 0, 60) == laneA);
  assert(laneFor(withUnrelated, withUnrelatedLayout, 24, 64) == laneB);
  assert(laneFor(withUnrelated, withUnrelatedLayout, 48, 67) == laneC);

  // If overlap depth exceeds the visual capacity, overflow is explicit rather
  // than aliasing a different event onto an occupied lane.
  PhraseRuntime::RuntimeSynthEventBuffer dense{};
  dense.lengthTicks = 4 * PhraseRuntime::kTicksPerBar;
  dense.count = 3;
  dense.events[0] = event(0, 192, 60);
  dense.events[1] = event(12, 192, 64);
  dense.events[2] = event(24, 192, 67);
  const Layout denseLayout = PhraseNotesLaneLayout::build(
      dense, 0, 2 * PhraseRuntime::kTicksPerBar * PhraseRuntime::kSubticksPerTick, 2);
  assert(denseLayout.laneCount == 2);
  assert(denseLayout.overflowCount == 1);
  assert(laneFor(dense, denseLayout, 24, 67) == PhraseNotesLaneLayout::kOverflowLane);

  return 0;
}
