#include <cassert>
#include <cstdio>

#include "src/midi/latency_histogram.h"

using GroovePuterMidi::LatencyHistogram;

int main() {
    LatencyHistogram h;
    assert(h.count() == 0 && h.percentileUs(50) == 0 && h.maxUs() == 0);
    for (int i = 0; i < 100; ++i) h.add(100);          // bucket 0: 0..249 us
    assert(h.percentileUs(50) == 250 && h.percentileUs(95) == 250);
    for (int i = 0; i < 10; ++i) h.add(1200);          // bucket 4: 1000..1249 us
    assert(h.count() == 110);
    assert(h.percentileUs(50) == 250);                 // median stays in the fast bucket
    assert(h.percentileUs(95) == 1250);                // p95 reaches the slow bucket
    assert(h.maxUs() == 1200);
    h.add(900000);                                     // saturates into the last bucket, max exact
    assert(h.maxUs() == 900000);
    assert(h.percentileUs(100) == 16 * 250);
    h.reset();
    assert(h.count() == 0 && h.maxUs() == 0);
    std::puts("latency histogram: PASS");
    return 0;
}
