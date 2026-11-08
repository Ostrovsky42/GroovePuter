#include <cassert>
#include <cstdio>

#include "src/midi/mod_hold_tracker.h"

using GroovePuterMidi::ModHoldTracker;

int main() {
    ModHoldTracker t;
    assert(!t.onRelease() && !t.pollLongHold(10000));        // nothing pressed: nothing happens

    t.onPress(1000);                                          // a tap: released before the hold time
    assert(!t.pollLongHold(1300));
    assert(t.onRelease());                                    // acts at the release
    assert(!t.onRelease());                                   // only once

    t.onPress(5000);                                          // a hold
    assert(!t.pollLongHold(5000 + ModHoldTracker::kHoldMs - 1));
    assert(t.pollLongHold(5000 + ModHoldTracker::kHoldMs));   // fires once, while still down
    assert(!t.pollLongHold(9000));
    assert(!t.onRelease());                                   // the release after a hold does nothing

    t.onPress(20000);                                         // cancel (session ended)
    t.reset();
    assert(!t.pollLongHold(30000) && !t.onRelease());

    t.onPress(0xFFFFFF00u);                                   // clock wrap-around
    assert(!t.pollLongHold(0xFFFFFF00u + 100u));
    assert(t.pollLongHold(0xFFFFFF00u + ModHoldTracker::kHoldMs));
    std::puts("Mod hold tracker: PASS");
    return 0;
}
