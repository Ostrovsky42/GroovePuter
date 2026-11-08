#include <cstdio>
#include <cstdlib>

#include "src/ui/midi_import_screen.h"

#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

using namespace MidiImportScreen;

static State sample() {
    State s;
    reset(s, 8);
    addRow(s, 2, 100, 48, 70, "LEAD", 0x0F);   // ch2 min 48
    addRow(s, 10, 200, 35, 81, "DRUMS", 0xFF); // ch10 drums
    addRow(s, 3, 80, 36, 60, "BASS", 0xFF);    // ch3 lowest melodic
    addRow(s, 5, 40, 60, 90, "PAD", 0xF0);     // ch5 highest
    assignInitialTargets(s);
    return s;
}

int main() {
    // Cycle order: A -> B -> DRUMS -> SKIP -> A, nothing skipped.
    CHECK(nextTarget(Target::A) == Target::B);
    CHECK(nextTarget(Target::B) == Target::Drums);
    CHECK(nextTarget(Target::Drums) == Target::Skip);
    CHECK(nextTarget(Target::Skip) == Target::A);

    // Initial targets: CH10 DRUMS, lowest melodic A, next B, rest SKIP.
    State s = sample();
    CHECK(s.rows[1].target == Target::Drums);
    CHECK(s.rows[2].target == Target::A);
    CHECK(s.rows[0].target == Target::B);
    CHECK(s.rows[3].target == Target::Skip);
    CHECK(clashSynth(s) == 0);

    // T on the cursor row cycles its target.
    s.cursor = 3;
    cycleTarget(s);
    CHECK(s.rows[3].target == Target::A);

    // Two tracks on A: clash on A, both rows marked, B untouched.
    CHECK(clashSynth(s) == 'A');
    CHECK(rowClashes(s, 3) && rowClashes(s, 2) && !rowClashes(s, 0));
    s.rows[3].target = Target::Skip;
    CHECK(clashSynth(s) == 0);
    s.rows[3].target = Target::B;
    CHECK(clashSynth(s) == 'B');
    s.rows[3].target = Target::Skip;

    // Masks: bit = channel - 1.
    Masks m = buildMasks(s);
    CHECK(m.a == (1u << 2));
    CHECK(m.b == (1u << 1));
    CHECK(m.d == (1u << 9));
    s.rows[0].target = Target::Skip;
    s.rows[1].target = Target::Skip;
    s.rows[2].target = Target::Skip;
    CHECK(!buildMasks(s).any());

    // Range defaults to the whole file (capped at 32 bars) and S/E set it.
    State r;
    reset(r, 100);
    CHECK(r.fileBars == 32 && r.rangeStart == 0 && r.rangeEnd == 31);
    r = sample();
    CHECK(r.rangeStart == 0 && r.rangeEnd == 7);
    moveBar(r, 2); setRangeStart(r);
    moveBar(r, 3); setRangeEnd(r);
    CHECK(r.rangeStart == 2 && r.rangeEnd == 5);
    moveBar(r, -5); setRangeEnd(r);   // end before start drags the start
    CHECK(r.rangeStart == 0 && r.rangeEnd == 0);
    int from = -1, len = -1;
    r.rangeStart = 2; r.rangeEnd = 5;
    rangeToSettings(r, from, len);
    CHECK(from == 2 && len == 4);

    // Scrolling keeps the cursor inside the 7-row window.
    State big;
    reset(big, 4);
    for (int i = 1; i <= 12; ++i) addRow(big, static_cast<uint8_t>(i), 1, 40, 50, "T", 1);
    for (int i = 0; i < 9; ++i) moveRow(big, 1, 7);
    CHECK(big.cursor == 9 && big.scroll == 3);
    for (int i = 0; i < 20; ++i) moveRow(big, -1, 7);
    CHECK(big.cursor == 0 && big.scroll == 0);

    std::printf("test_import_screen PASS\n");
    return 0;
}
