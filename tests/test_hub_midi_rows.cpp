#include <cstdio>
#include <cstdlib>

#include "src/ui/hub_midi_bar_strip.h"

#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

int main() {
    using namespace HubMidiBarStrip;
    uint8_t s[40];

    // 1:1 cells: bars 0 and 2 have notes, bar 1 and 3 are empty.
    CHECK(fill(0b0101u, 4, 32, s, 40) == 4);
    CHECK(s[0] == kCellNotes && s[1] == kCellEmpty);
    CHECK(s[2] == kCellNotes && s[3] == kCellEmpty);

    // Empty file bars: no cells; no occupancy: all empty.
    CHECK(fill(0u, 0, 32, s, 40) == 0);
    CHECK(fill(0u, 3, 32, s, 40) == 3 && s[0] == 0 && s[1] == 0 && s[2] == 0);

    // Only the last bar has notes.
    CHECK(fill(1u << 4, 5, 32, s, 40) == 5);
    CHECK(s[3] == kCellEmpty && s[4] == kCellNotes);

    // Files longer than the stored 32 bars are capped at 32.
    CHECK(fill(0xFFFFFFFFu, 100, 40, s, 40) == 32);

    // Grouping: 10 bars in 4 cells -> [0,2) [2,5) [5,7) [7,10).
    CHECK(cellFirstBar(1, 4, 10) == 2 && cellFirstBar(2, 4, 10) == 5);
    CHECK(cellFirstBar(3, 4, 10) == 7);
    // Notes only in the last (partial, 3-bar) group, bar 9.
    CHECK(fill(1u << 9, 10, 4, s, 40) == 4);
    CHECK(s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == kCellNotes);
    // Notes in bar 4 land in cell 1 only.
    CHECK(fill(1u << 4, 10, 4, s, 40) == 4);
    CHECK(s[1] == kCellNotes && s[0] == 0 && s[2] == 0 && s[3] == 0);

    // Capacity bound.
    CHECK(fill(0xFFu, 8, 8, s, 3) == 3);

    std::printf("test_hub_midi_rows PASS\n");
    return 0;
}
