#pragma once

// [ / ] on the Melody editor: the previous/next slot that holds a saved
// Melody, in slot order A1..A8, B1..B8, wrapping. Step slots are skipped and
// the current slot never counts as "next". Returns -1 when no other slot holds
// a Melody.
namespace MelodySlotBrowse {

template <typename IsMelodySlot>
int neighbour(int currentSlot, int direction, int slotCount,
              IsMelodySlot&& isMelodySlot) {
    if (slotCount <= 1 || direction == 0) return -1;
    const int step = direction > 0 ? 1 : -1;
    for (int offset = 1; offset < slotCount; ++offset) {
        const int slot =
            ((currentSlot + step * offset) % slotCount + slotCount) % slotCount;
        if (isMelodySlot(slot)) return slot;
    }
    return -1;
}

}  // namespace MelodySlotBrowse
