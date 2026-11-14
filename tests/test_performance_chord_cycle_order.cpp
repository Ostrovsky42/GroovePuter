// Chord cycle/popup display order: diatonic first; enum values (NVS) unchanged.
#include <cassert>

#include "src/input/performance_keyboard.h"

using M = PerformanceChordMode;

static_assert(static_cast<int>(M::Off) == 0 && static_cast<int>(M::Major) == 1 &&
              static_cast<int>(M::Minor) == 2 && static_cast<int>(M::Fifth) == 3 &&
              static_cast<int>(M::Minor7) == 4 && static_cast<int>(M::Memory) == 5 &&
              static_cast<int>(M::Sus2) == 6 && static_cast<int>(M::Sus4) == 7 &&
              static_cast<int>(M::Dominant7) == 8 && static_cast<int>(M::Major7) == 9 &&
              static_cast<int>(M::ScaleTriad) == 10 && static_cast<int>(M::ScaleSeventh) == 11 &&
              static_cast<int>(M::Count) == 12, "chord enum values are persisted");

int main() {
    MusicalEventRouter router;
    PerformanceKeyboard kb(router);
    assert(kb.chordMode() == M::Off);

    // Table covers every mode exactly once.
    bool seen[12]{};
    for (int i = 0; i < PerformanceKeyboard::kChordDisplayCount; ++i) {
        const int v = static_cast<int>(PerformanceKeyboard::kChordDisplayOrder[i]);
        assert(v >= 0 && v < 12 && !seen[v]);
        seen[v] = true;
    }

    // No memory captured: MEMORY is skipped, forward and backward.
    assert(kb.chordMemorySize() == 0);
    const M forward[] = {M::ScaleTriad, M::ScaleSeventh, M::Fifth, M::Minor, M::Major,
                         M::Minor7, M::Sus2, M::Sus4, M::Dominant7, M::Major7, M::Off,
                         M::ScaleTriad};
    for (M expected : forward) {
        kb.cycleChordMode(1);
        assert(kb.chordMode() == expected);
    }
    kb.setChordMode(M::Off);
    const M backward[] = {M::Major7, M::Dominant7, M::Sus4, M::Sus2, M::Minor7, M::Major,
                          M::Minor, M::Fifth, M::ScaleSeventh, M::ScaleTriad, M::Off};
    for (M expected : backward) {
        kb.cycleChordMode(-1);
        assert(kb.chordMode() == expected);
    }

    // With a captured chord MEMORY returns, right after MIN7.
    assert(kb.keyDown('a'));
    assert(kb.keyDown('d'));
    assert(kb.captureChordMemory());
    assert(kb.keyUp('a'));
    assert(kb.keyUp('d'));
    assert(kb.chordMemorySize() > 0);
    kb.setChordMode(M::Minor7);
    kb.cycleChordMode(1);
    assert(kb.chordMode() == M::Memory);
    kb.cycleChordMode(1);
    assert(kb.chordMode() == M::Sus2);
    return 0;
}
