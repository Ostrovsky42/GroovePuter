#include <cassert>
#include <cstdint>

#include "src/generation/roles/chord_progression.h"

using GroovePuterRhythm::ChordQuality;

int main() {
    uint8_t intervals[5]{};
    uint8_t count = 0;
    assert(GroovePuterRhythm::chordQualityIntervals(ChordQuality::Minor7, intervals, 5, count));
    assert(count == 4 && intervals[0] == 0 && intervals[1] == 3 &&
           intervals[2] == 7 && intervals[3] == 10);
    assert(GroovePuterRhythm::chordQualityIntervals(ChordQuality::Major9, intervals, 5, count));
    assert(count == 5 && intervals[0] == 0 && intervals[1] == 4 &&
           intervals[2] == 7 && intervals[3] == 11 && intervals[4] == 14);
    assert(!GroovePuterRhythm::chordQualityIntervals(ChordQuality::Count, intervals, 5, count));
    assert(!GroovePuterRhythm::chordQualityIntervals(ChordQuality::Major9, intervals, 4, count));
    return 0;
}
