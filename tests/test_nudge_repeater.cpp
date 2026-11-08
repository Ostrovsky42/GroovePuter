#include <cassert>
#include <cstdio>

#include "src/midi/nudge_repeater.h"

using GroovePuterMidi::NudgeRepeater;

int main() {
    NudgeRepeater r;
    assert(r.poll(1000) == 0);                                     // idle: nothing
    r.onPress(-1, 1000);
    assert(r.poll(1000 + NudgeRepeater::kDelayMs - 1) == 0);       // still inside the initial delay
    assert(r.poll(1000 + NudgeRepeater::kDelayMs) == -1);          // first repeat after the delay
    assert(r.poll(1000 + NudgeRepeater::kDelayMs + NudgeRepeater::kIntervalMs - 1) == 0);
    assert(r.poll(1000 + NudgeRepeater::kDelayMs + NudgeRepeater::kIntervalMs) == -1);  // then steady
    r.onRelease();
    assert(!r.active() && r.poll(100000) == 0);                    // released: stops at once

    r.onPress(1, 5000);                                            // the other direction, then a reset
    assert(r.poll(5000 + NudgeRepeater::kDelayMs) == 1);
    r.reset();
    assert(r.poll(9000) == 0);

    r.onPress(1, 0xFFFFFF00u);                                     // clock wrap-around
    assert(r.poll(0xFFFFFF00u + 100u) == 0);
    assert(r.poll(0xFFFFFF00u + NudgeRepeater::kDelayMs) == 1);
    std::puts("nudge repeater: PASS");
    return 0;
}
