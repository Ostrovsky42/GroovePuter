#include <cassert>

#include "src/ui/melody_slot_browse.h"

namespace {
// 16 slots: A1..A8 = 0..7, B1..B8 = 8..15.
bool melodyAt(unsigned mask, int slot) { return (mask >> slot) & 1u; }
}  // namespace

int main() {
    using MelodySlotBrowse::neighbour;
    // Melodies in A2, A5, B3 (slots 1, 4, 10); everything else holds steps.
    const unsigned mask = (1u << 1) | (1u << 4) | (1u << 10);
    auto isMelody = [&](int slot) { return melodyAt(mask, slot); };

    assert(neighbour(1, +1, 16, isMelody) == 4);    // A2 -> A5, skips steps
    assert(neighbour(4, +1, 16, isMelody) == 10);   // A5 -> B3, across banks
    assert(neighbour(10, +1, 16, isMelody) == 1);   // B3 -> A2, wraps
    assert(neighbour(1, -1, 16, isMelody) == 10);   // A2 <- B3, wraps back
    assert(neighbour(4, -1, 16, isMelody) == 1);
    // From a step slot (working Melody not saved yet) the nearest saved one.
    assert(neighbour(6, +1, 16, isMelody) == 10);
    assert(neighbour(6, -1, 16, isMelody) == 4);

    // The only Melody is the current slot: there is no other one to go to.
    auto onlyA2 = [&](int slot) { return slot == 1; };
    assert(neighbour(1, +1, 16, onlyA2) == -1);
    assert(neighbour(1, -1, 16, onlyA2) == -1);
    assert(neighbour(0, +1, 16, onlyA2) == 1);
    // No Melodies at all.
    assert(neighbour(3, +1, 16, [](int) { return false; }) == -1);
    return 0;
}
